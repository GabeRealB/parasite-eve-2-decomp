#include "rooms/dryfield_breezeway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_breezeway_private.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/item_pickup.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

/// 0x14 work block the breezeway's room task hangs off the `Task::work` slot
/// (0x1C) -- that slot is *not* a `TaskIdMap` here. Reach it with
/// `(DbwWork*)task->work`.
///
/// `func_dryfield_breezeway_8017E010` (and its twin
/// `func_dryfield_breezeway_8017E114`) allocates the block
/// (`Mem_Malloc(0x14, 0)`), fills the three leading pointers and publishes the
/// owning task in `D_dryfield_breezeway_801843C0`: the slot-3 game pointer
/// (`gameGetPtrSlot(3)`), then `field_0` of the work `Gp_FindWorkById` finds
/// for the id formed from `gGameSession` bytes 6/7 and for that id OR'd with
/// 0x1000. `field_8` is the dispatch slot
/// `func_dryfield_breezeway_8017E390` hands to `Gp_DispatchMsg`.
///
/// `field_C` and `field_E` are the two shorts the breezeway hotspot action
/// `func_dryfield_breezeway_8017E370` writes, which the room's data table calls
/// with 1 or 2.
typedef struct DbwWork {
    /* 0x00 */ void* field_0;
    /* 0x04 */ void* field_4;
    /* 0x08 */ void* field_8;
    /* 0x0C */ u16   field_C;
    /* 0x0E */ s16   field_E;
    /* 0x10 */ byte  pad_10[0x4];
} DbwWork;
STATIC_ASSERT_SIZEOF(DbwWork, 0x14);

/// 0x60 work block of the second task family in this room, also hung off
/// `Task::work` (0x1C): `func_dryfield_breezeway_8017E464` allocates it with
/// `memCalloc(0x60, 0)` and parks the family's `GpMsgEntry[]`
/// (`D_dryfield_breezeway_80182DCC`, a single 0x13F1 entry) in
/// `Task::msgTable`, which is what makes `Gp_DispatchMsg` route messages into
/// this family at all. Reach the block with `(DbwEventWork*)task->work`.
///
/// `field_40` is the answer latch the message handler
/// `func_dryfield_breezeway_8017FBC8` sets: message 0x13F1 is the "can this key
/// item be used here?" query `Gp_UseKeyItemRow` sends to slot 7, carrying the
/// highlighted item as its payload, and the handler stores 1 exactly when that
/// payload is 0x11B, the one item this room accepts. The block starts out
/// zeroed by the allocator and by `func_dryfield_breezeway_8017E464` itself;
/// `func_dryfield_breezeway_8017FE08` reads the latch back and picks state 6
/// when it is 1 and state 2 otherwise.
///
/// `promptKind` is the display mode the hotspot scan
/// `func_dryfield_breezeway_8017E65C` copies off the `OverlayHotspot` the cursor
/// landed on (whose id it parks at 0x4C) before it picks state 3;
/// `func_dryfield_breezeway_8017FD9C` forwards it to `func_800D4E78` when it
/// re-spawns the prompt.
///
/// `cursorX` / `cursorY` are the on-screen pair the same scan is hit-tested
/// against: `func_dryfield_breezeway_8017E464` seeds them with the reset
/// position (0, 0x20) `func_dryfield_breezeway_8017FD9C` also passes to
/// `func_dryfield_breezeway_8017EB8C`, and `func_dryfield_breezeway_8017E81C`
/// feeds them to `func_dryfield_breezeway_8017FCB4`.
///
/// `light` / `color` are the room's own lighting pair, the block's whole first
/// 0x40 bytes: `func_dryfield_breezeway_8017E464` publishes them onto
/// `TmdObject::lightMtx` / `colorMtx` -- the slots `Gp_BindDefaultMtx` otherwise
/// points at `Gp_DefaultMtx` / `Gp_DefaultMtx2` -- so the event object draws
/// with this lighting rather than the shared defaults, and
/// `Gp_SetObjTrans` writes the 0x800 translation into `color.t`.
typedef struct DbwEventWork {
    /* 0x00 */ MATRIX light;
    /* 0x20 */ MATRIX color;
    /* 0x40 */ s32    field_40;
    /* 0x44 */ byte   pad_44[0x8];
    /* 0x4C */ s16    field_4C;
    /* 0x4E */ s16    cursorX;
    /* 0x50 */ s16    cursorY;
    /* 0x52 */ u16    field_52;
    /* 0x54 */ u16    field_54;
    /* 0x56 */ s16    field_56;
    /* 0x58 */ s16    field_58;
    /* 0x5A */ s16    field_5A;
    /* 0x5C */ s8     promptKind;
    /* 0x5D */ byte   pad_5D[0x3];
} DbwEventWork;
STATIC_ASSERT_SIZEOF(DbwEventWork, 0x60);

/// The `TaskDesc` `func_dryfield_breezeway_8017E464` spawns the room's prompt
/// task (`func_dryfield_breezeway_8017FA80`) from, and the single-entry `GpMsgEntry[]` it parks in `Task::msgTable`
/// so `Gp_DispatchMsg` routes the family's messages (the 0x13F1 "can this key
/// item be used here?" query) into it. Both sit in the room's trailing data
/// blob, the table immediately after the descriptor.
extern TaskDesc   D_dryfield_breezeway_80182DC0;
extern GpMsgEntry D_dryfield_breezeway_80182DCC[];

/// The one scratch buffer `func_dryfield_breezeway_8017E390` builds both of its
/// payloads in, which is why they share a frame slot: `rec` is the 0x14-byte
/// slot-3 weapon record msg 0x3E8 takes (the `AnimationPlayRequest` `Gp_MsgPlayerWeapon`
/// also sends, with `field_4` set to this room's 9 and `field_C`/`field_10`
/// zeroed), and `msg` the `ActorCommand` the 0x7DA prompt takes right after it.
typedef union DbwMsgBuf {
    /* 0x0 */ AnimationPlayRequest rec;
    /* 0x0 */ ActorCommand         msg;
} DbwMsgBuf;
STATIC_ASSERT_SIZEOF(DbwMsgBuf, 0x14);

/// A point in the plane the room works in: an `SVECTOR`'s three components plus
/// the halfword that rounds the record up to the 8-byte stride the room's stack
/// slots for it have, with `vz` a real component the room pins to zero.
/// `func_dryfield_breezeway_8017FAD0` reads `vx` and `vy` unsigned and truncates
/// each difference back to 16 bits before squaring it, so the sign of the load
/// never reaches its result.
typedef struct DbwVec {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 vy;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad;
} DbwVec;
STATIC_ASSERT_SIZEOF(DbwVec, 0x8);

/// The edge a breezeway prompt beam starts from, and the mode that selects it.
///
/// `func_dryfield_breezeway_8017F1F4` draws the beam's cursor segment: a
/// raw-textured quad whose near edge is either the `arg2` origin (the first
/// segment of a beam, `mode` 0) or the two corners recorded here, and whose
/// far edge is always `arg2` plus the `arg1`-rotated offsets. The tail of
/// every call writes that far edge back into the two corners, so a beam that
/// is redrawn each frame grows from where the previous segment ended; the
/// caller's scan (`func_dryfield_breezeway_8017EB8C`) passes mode 0 on the
/// first step of a beam and mode 1 on the rest. The corners are `DbwVec`s
/// because the scan reads the tip it advances to as unsigned, the same way
/// `func_dryfield_breezeway_8017FAD0` reads its points.
typedef struct DbwBeamEdge {
    /* 0x00 */ DbwVec fromA;
    /* 0x08 */ DbwVec fromB;
    /* 0x10 */ s16    mode;
} DbwBeamEdge;
STATIC_ASSERT_SIZEOF(DbwBeamEdge, 0x12);

/// Placement this room hands on with message 0x7D4 from
/// `func_dryfield_breezeway_8017E2D4`, `func_dryfield_breezeway_8017E390` and
/// `func_dryfield_breezeway_8017DEC0`: world x 17000, y 0, z 3000, yaw 0xA00.
extern ActorTransform D_dryfield_breezeway_80181E28;

/// The two placements that follow it in the same three-record run, which
/// `func_dryfield_breezeway_8017DEC0` sends to slot 3 as the second and third
/// message of its state-1 sequence: `[0]` is the record message 0x3E9 places the
/// player with, and `[1]` -- the run's third record -- the one message 0x3EE
/// does. The label the decomp references is the start of this array, so the
/// third record is reached as `[1]` rather than by a symbol of its own.
extern ActorTransform D_dryfield_breezeway_80181E40[];

/// The key-item prompt's own hotspot table: the one-entry 0xFFFF-terminated
/// `OverlayHotspot` run `func_dryfield_breezeway_8017E65C` hit-tests at the
/// prompt's own screen position and walks for the entry the cursor landed on,
/// where the prop table below is hit-tested at the cursor itself. Its `id` is
/// the script variant the prompt confirms, which the scan parks in the event
/// work block (`DbwEventWork.field_4C`, with `promptKind` at 0x5C) before state
/// 3. `func_dryfield_breezeway_8017E464` clears its `hit` along with the other
/// table's.
extern OverlayHotspot D_dryfield_breezeway_80182E00[];

/// This room's prop hotspot table, the 0xFFFF-terminated `OverlayHotspot` run
/// `func_dryfield_breezeway_8017FCB4` hit-tests the action cursor against. Its
/// entries are the room's interactive props:
/// `func_dryfield_breezeway_8017E464` clears every entry's `hit` through it
/// before the first frame -- both tables', so the key-item prompt above starts
/// clean too -- and the scan in
/// `func_dryfield_breezeway_8017E81C` walks it for the entry the cursor landed
/// on.
extern OverlayHotspot D_dryfield_breezeway_80182DDC[];

static void func_dryfield_breezeway_8017E464(Task* arg0);
static void func_dryfield_breezeway_8017E65C(Task* task);
static void func_dryfield_breezeway_8017E81C(Task* task);
static void func_dryfield_breezeway_8017EB8C(Task* task, s16 arg1, s16 arg2);
static void func_dryfield_breezeway_8017F1F4(s16 arg0, s16 arg1, DbwVec* arg2, DbwVec* arg3, DbwBeamEdge* arg4);
static void func_dryfield_breezeway_8017F998(s32 x, s32 y, s32 variant);
static s16  func_dryfield_breezeway_8017FAD0(DbwVec* target, DbwVec* pos);
static void func_dryfield_breezeway_8017FB30(Task* task, s16 arg1, s16 arg2);
static s16  func_dryfield_breezeway_8017FBEC(s16 arg0, s16 arg1, s16 arg2, s16 arg3);
static s32  func_dryfield_breezeway_8017FCB4(OverlayHotspot* table, s16 x, s16 y);
static void func_dryfield_breezeway_8017FD68(Task* task);
static void func_dryfield_breezeway_8017FD9C(Task* task);
static void func_dryfield_breezeway_8017FE08(Task* task);
static void func_dryfield_breezeway_8017FE90(Task* arg0);
static void func_dryfield_breezeway_8017FF1C(Task* task);
static void func_dryfield_breezeway_8018034C(GfxCoord* coord, u8* data, s32 arg2, s32 arg3);
static void func_dryfield_breezeway_80180858(GfxCoord* coord, u8* data, s32 arg2, s32 arg3);
static void func_dryfield_breezeway_80181938(Task* task, u8* color);

/// State handlers of the room's key-item event task, indexed by its state
/// through `func_dryfield_breezeway_8017FC38`: set-up, prompt arming, the
/// prompt-position scan, prompt spawning, the key-item answer, the exit and
/// the cursor-hotspot scan.
static const TaskFuncTable7 D_dryfield_breezeway_8017D5E8 = {
    {
        func_dryfield_breezeway_8017E464,
        func_dryfield_breezeway_8017FD68,
        func_dryfield_breezeway_8017E65C,
        func_dryfield_breezeway_8017FD9C,
        func_dryfield_breezeway_8017FE08,
        func_dryfield_breezeway_8017FE90,
        func_dryfield_breezeway_8017E81C,
    }
};

void func_dryfield_breezeway_8017E2D4(void);
void func_dryfield_breezeway_8017E350(void);
void func_dryfield_breezeway_8017E370(s16);

void func_dryfield_breezeway_8017E010(Task*);
void func_dryfield_breezeway_8017E114(Task*);
void func_dryfield_breezeway_8017E350(void);
void func_dryfield_breezeway_8017E390(void);

s32  func_dryfield_breezeway_8017FBC8(Task*, s32, s32, s32);
void func_dryfield_breezeway_8017FA80(Task*);
void func_dryfield_breezeway_8017FC38(Task*);

TaskDesc D_dryfield_breezeway_80181DD4 = { 0, 32, func_dryfield_breezeway_8017D79C, { .model = NULL } };

GpMsgEntry D_dryfield_breezeway_80181DE0[6] = {
    { 5102, func_dryfield_breezeway_8017D940 },
    { 5105, func_dryfield_breezeway_8017D90C },
    { 5104, func_dryfield_breezeway_8017DA48 },
    { 5106, func_dryfield_breezeway_8017DBA4 },
    { 5103, func_dryfield_breezeway_8017DBD8 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_breezeway_80181E10[2] = {
    { 0, 32, func_dryfield_breezeway_8017DC3C, { .model = NULL } },
    { 0, 32, func_dryfield_breezeway_8017DCE4, { .model = NULL } },
};

ActorTransform D_dryfield_breezeway_80181E28 = { { 0x4268, 0, 3000, 0 }, { 0, 2560, 0, 0 } };

ActorTransform D_dryfield_breezeway_80181E40[2] = {
    { { 0x4074, 0, 1500, 0 }, { 0, 1024, 0, 0 } },
    { { 0x4074, 0, 1500, 0 }, { 0, 512, 0, 0 } },
};

GpEvsCmd D_dryfield_breezeway_80181E70[12] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_breezeway_8017E370 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_breezeway_8017E370 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_breezeway_8017E2D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_breezeway_8017E350 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_breezeway_80181F90[12] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_breezeway_8017E390 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_breezeway_8017E350 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_dryfield_breezeway_801820B0[2] = {
    { 0, 192, func_dryfield_breezeway_8017E010, { .model = NULL } },
    { 0, 192, func_dryfield_breezeway_8017E114, { .model = NULL } },
};

TaskDesc D_dryfield_breezeway_801820C8 = { 0, 192, taskKill, { .model = NULL } };

TmdBone D_dryfield_breezeway_801820D4[1] = {
#include "assets/dryfield_breezeway_model_057DC_skeleton.inc"
};

u32 D_dryfield_breezeway_801820F8[1] = {
#include "assets/dryfield_breezeway_model_057DC_partVerts.inc"
};

SVECTOR D_dryfield_breezeway_801820FC[88] = {
#include "assets/dryfield_breezeway_model_057DC_verts.inc"
};

SVECTOR D_dryfield_breezeway_801823BC[18] = {
#include "assets/dryfield_breezeway_model_057DC_normals.inc"
};

u32 D_dryfield_breezeway_8018244C[596] = {
#include "assets/dryfield_breezeway_model_057DC_stream.inc"
};

TmdSource D_dryfield_breezeway_80182D9C = {
    0,
    4324,
    0,
    1,
    D_dryfield_breezeway_801820F8,
    D_dryfield_breezeway_801820FC,
    D_dryfield_breezeway_801823BC,
    D_dryfield_breezeway_801820D4,
    D_dryfield_breezeway_8018244C,
};

TaskDesc D_dryfield_breezeway_80182DC0 = { 0, 192, func_dryfield_breezeway_8017FA80, { .model = NULL } };

GpMsgEntry D_dryfield_breezeway_80182DCC[2] = {
    { 5105, func_dryfield_breezeway_8017FBC8 },
    { 0x7FFFFFFF, NULL },
};

OverlayHotspot D_dryfield_breezeway_80182DDC[3] = {
    { 102, -80, 34, 30, 1, 0, 0 },
    { 115, -50, 20, 30, 1, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

OverlayHotspot D_dryfield_breezeway_80182E00[2] = {
    { -16, 20, 32, 48, 1, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

TaskDesc D_dryfield_breezeway_80182E18 = { 1, 192, func_dryfield_breezeway_8017FC38, { .model = &D_dryfield_breezeway_80182D9C } };

u_long D_dryfield_breezeway_80182E24[64] = {
    0,
    0x30A0903,
    0x70C0D07,
    3,
    0,
    0x3070B04,
    0x7090F0C,
    5,
    0,
    0x7040A05,
    0x9060D0E,
    7,
    0,
    0xC040806,
    0xB060B0F,
    7,
    0x1000000,
    0xE060607,
    0xC08060E,
    5,
    0x1000000,
    0xE090305,
    0xC0C040A,
    3,
    0x2000000,
    0xC0B0303,
    0xB0D0605,
    2,
    0x1000000,
    0x70C0601,
    0x80D0B04,
    3,
    0,
    0x40A0903,
    0x70C0D06,
    3,
    0,
    0x3070B04,
    0x7080F0C,
    5,
    0,
    0x7040A05,
    0x9060D0E,
    7,
    0,
    0xC040806,
    0xB060B0F,
    7,
    0x1000000,
    0xE060607,
    0xC08060E,
    5,
    0x1000000,
    0xE090305,
    0xC0C040A,
    3,
    0x1000000,
    0xC0B0303,
    0xB0D0605,
    2,
    0x2000000,
    0x80C0602,
    0x80D0B04,
    3,
};

GpImgRec D_dryfield_breezeway_80182F24[2] = {
    { 0, 0, { 896, 0, 8, 16 }, D_dryfield_breezeway_80182E24 },
    { 255, 0, { 0, 0, 0, 0 }, NULL },
};

/// The pair of cutscene blocks `func_800E8634` hands to `Task_Spawn` (bank 9,
/// type 7): the table the spawned task starts from and the event-command
/// stream it parks in `D_801156D0` for the task that follows it. Both live in
/// the room's trailing data blob.
extern GpEvsCmd D_dryfield_breezeway_80181E70[];

extern GpEvsCmd D_dryfield_breezeway_80181F90[];

static void func_dryfield_breezeway_8017DEC0(Task* arg0);
static void func_dryfield_breezeway_8017E948(RoomRect* rect, u8 r, u8 g, u8 b);
static void func_dryfield_breezeway_8017F538(Task* task);

/// The one state machine that arms the breezeway, switched on the room task's
/// `DbwWork.field_C`:
///
/// * 0 does nothing but clear the state (the `case 0: break;` the switch needs
///   to build its dispatch tree -- reaching the tail is what clears it).
/// * 1 sends the scene's opening sequence: the session's two id bytes as the
///   0x7DA prompt payload with the 1 the receiver reads as "armed", the room's
///   reset placement (`D_dryfield_breezeway_80181E28`) with 0x7D4, and the two
///   `D_dryfield_breezeway_80181E40` placements -- the message-0x3E9 that moves
///   the player and the 0x3EE that takes the run's third record -- to the slot-3
///   game task before publishing view 4's area-record index.
/// * 2 republishes the player's weapon as slot-3 msg 0x3E8, the same record
///   `Gp_MsgPlayerWeapon` builds: this room's `field_4` 9, `field_8` 1, a
///   `field_C` of 0xA and everything else zeroed.
///
/// Anything else (`field_C` above 2) clears the state and returns, which is how
/// a finished arm retires. The pointer into the record is what makes the middle
/// three field stores go through `$a1` rather than the frame pointer: taking the
/// address as a value first lets CSE rewrite them as base+offset, the same
/// allocation the original compiler reached.
static void func_dryfield_breezeway_8017DEC0(Task* arg0)
{
    ActorCommand          msg;
    DbwMsgBuf             buf;
    AnimationPlayRequest* rec;
    DbwWork*              work;
    s32                   state;
    s32                   id;

    work  = (DbwWork*)arg0->work;
    state = work->field_C;

    switch (state) {
        default:
            work->field_C = 0;
            return;
        case 0:
            break;
        case 1:
            msg.context.loc.stage = gGameSession->at4.loc.stage;
            msg.context.loc.area  = gGameSession->at4.loc.area;
            msg.command           = 1;
            Gp_DispatchMsgPtr(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            Gp_DispatchMsgPtr(work->field_4, 0x7D4, &D_dryfield_breezeway_80181E28, 0);
            Gp_DispatchMsgPtr(work->field_0, 0x3E9, &D_dryfield_breezeway_80181E40[0], 0);
            Gp_DispatchMsgPtr(work->field_0, 0x3EE, &D_dryfield_breezeway_80181E40[1], 0);
            Mc_SaveData[0].state.at4.loc.view = Gp_FindViewIndex(4);
            break;
        case 2:
            rec                          = &buf.rec;
            id                           = Player_Status.weapon;
            buf.rec.source.index         = (Mc_SaveData[0].state.characterId == 1) ? id + 1 : id + 0x22;
            rec->animationId             = 9;
            rec->blend                   = ANIMATION_BLEND_INTERPOLATE;
            rec->blendFrames             = 0xA;
            buf.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &buf, 0);
            break;
    }
    work->field_C = 0;
}

void func_dryfield_breezeway_8017E010(Task* arg0)
{
    DbwWork* work;
    s32      id;

    switch (arg0->state) {
        case 0:
            work       = (DbwWork*)Mem_Malloc(0x14, 0);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x14);
                work->field_0                 = gameGetPtrSlot(3);
                D_dryfield_breezeway_801843C0 = arg0;
                id                            = gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8);
                work->field_4                 = Gp_FindWorkById(id)->field_0;
                id                            = ((gGameSession->at4.loc.stage << 8) | 0x1000) | gGameSession->at4.loc.area;
                work->field_8                 = Gp_FindWorkById(id)->field_0;
            }
            arg0->state += 1;
            return;
        case 1:
            taskKill(arg0);
            return;
    }
}

/// The long-lived half of the arming pair: `func_dryfield_breezeway_8017E010`
/// is the same state 0 with no sequencer and no cutscene behind it, and is the
/// one `dryfield_night_water_tank` spawns. This one arms the room and then
/// stays resident to run `func_dryfield_breezeway_8017DEC0` every frame.
///
/// State 0 arms the room, but only while no cutscene is running
/// (`Gp_StateC08.field_A != 1`) and the area is not cleared (`gDisplayState.pendingMode == DISPLAY_MODE_NONE`) --
/// otherwise it returns having done nothing, which retires the task on the
/// next frame. It allocates the 0x14 `DbwWork` block, publishes the room task
/// in `D_dryfield_breezeway_801843C0`, republishes the player's weapon as
/// slot-3 msg 0x3E8 (`AnimationPlayRequest`, the record `Gp_MsgPlayerWeapon` also builds:
/// `field_0` off the equipped-weapon index in `Player_Status.weapon`, `field_4` and
/// `field_8` both 1, `field_C` 0xA and `field_10` zero) and starts the room's
/// opening cutscene through `func_800E8634`, which is what raises
/// `gGameSession::eventState`. It then advances to state 1.
///
/// State 1 runs the sequencer every frame until the cutscene clears
/// `gGameSession::eventState`, at which point the task kills itself. Any other
/// state goes straight to the sequencer.
void func_dryfield_breezeway_8017E114(Task* arg0)
{
    AnimationPlayRequest buf;
    DbwWork*             work;
    s32                  id;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_A == 1 || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = (DbwWork*)Mem_Malloc(0x14, 0);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x14);
                work->field_0                 = gameGetPtrSlot(3);
                D_dryfield_breezeway_801843C0 = arg0;
                id                            = gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8);
                work->field_4                 = Gp_FindWorkById(id)->field_0;
                id                            = ((gGameSession->at4.loc.stage << 8) | 0x1000) | gGameSession->at4.loc.area;
                work->field_8                 = Gp_FindWorkById(id)->field_0;
            }
            id                       = Player_Status.weapon;
            buf.source.index         = (Mc_SaveData[0].state.characterId == 1) ? id + 1 : id + 0x22;
            buf.animationId          = 1;
            buf.blend                = ANIMATION_BLEND_INTERPOLATE;
            buf.blendFrames          = 0xA;
            buf.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &buf, 0);
            func_800E8634(D_dryfield_breezeway_80181E70, 0, D_dryfield_breezeway_80181F90);
            arg0->state += 1;
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                taskKill(arg0);
                return;
            }
            break;
    }
    func_dryfield_breezeway_8017DEC0(arg0);
}

void func_dryfield_breezeway_8017E2D4(void)
{
    DbwWork*     work;
    ActorCommand msg;

    work                  = (DbwWork*)D_dryfield_breezeway_801843C0->work;
    msg.context.loc.stage = gGameSession->at4.loc.stage;
    msg.context.loc.area  = gGameSession->at4.loc.area;
    msg.command           = 2;
    Gp_DispatchMsgPtr(gameGetPtrSlot(4), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
    Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_dryfield_breezeway_80181E28, 0);
}

void func_dryfield_breezeway_8017E350(void)
{
    Gp_ArmStateF0(1);
}

void func_dryfield_breezeway_8017E370(s16 arg0)
{
    DbwWork* work;

    work          = (DbwWork*)D_dryfield_breezeway_801843C0->work;
    work->field_C = arg0;
    work->field_E = 0;
}

void func_dryfield_breezeway_8017E390(void)
{
    DbwMsgBuf buf;
    DbwWork*  work;
    s32       id;

    id                           = Player_Status.weapon;
    buf.rec.source.index         = (Mc_SaveData[0].state.characterId == 1) ? id + 1 : id + 0x22;
    buf.rec.animationId          = 9;
    buf.rec.blend                = ANIMATION_BLEND_RESET;
    buf.rec.blendFrames          = 0;
    buf.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    /* The message ABI carries this object address in one 32-bit word. */
    Gp_DispatchMsg(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, (s32)&buf, 0);

    work                      = (DbwWork*)D_dryfield_breezeway_801843C0->work;
    buf.msg.context.loc.stage = gGameSession->at4.loc.stage;
    buf.msg.context.loc.area  = gGameSession->at4.loc.area;
    buf.msg.command           = 2;
    Gp_DispatchMsgPtr(gameGetPtrSlot(4), 0x7DA, &buf, 0x7DB);
    Gp_DispatchMsgPtr(work->field_8, 0x7D4, &D_dryfield_breezeway_80181E28, 0);
}

/// Brings up the room's second task family, the key-item event the prompt in
/// `func_dryfield_breezeway_8017E65C` rides on. The 0x60 `DbwEventWork` block
/// is allocated and published in `Task::work`, the family's own `GpMsgEntry[]`
/// (`D_dryfield_breezeway_80182DCC`, the one 0x13F1 record) goes to
/// `Task::msgTable` -- which is what routes the key-item query into this room
/// at all -- and the room's own event task is spawned from
/// `D_dryfield_breezeway_80182DC0` into `Task::spawnArg2`. `Mc_SaveData[0].state.at4.loc.view` is
/// stamped with 6, the area-record index the view gate reads back.
///
/// The event object then draws with the room's lighting rather than the shared
/// defaults: the work block's `light` / `color` pair is splatted onto
/// `TmdObject::lightMtx` / `colorMtx` (the slots `Gp_BindDefaultMtx` otherwise
/// points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`), the 0x800 translation goes
/// into the colour matrix, and the hotspot scan's cursor is seeded with the
/// reset pair (0, 0x20). Both hotspot tables are walked to clear `hit`, so the
/// prompt and the prop cursor both start the room with nothing highlighted.
///
/// The three descriptor stores sit in a one-iteration `do { } while (0)`
/// because retail's source had them there, and the loop note that leaves
/// behind is load-bearing twice: its loop depth doubles those stores' ref
/// weights, which is what lifts the 6 above the state reload in `local-alloc`'s
/// quantity order, and it stops that reload being hoisted above the
/// `Mc_SaveData[0].state.at4.loc.view` store once it holds `$v0`. Three plain statements instead of
/// the wrapper score 98.5%; wrapping a fourth statement reweights it too and
/// does not match.
///
/// The block at the end is deliberately written against the task rather than
/// against `work` and `ext`: it re-reads both slots, which is what makes its
/// base pointers fresh values rather than the ones the middle of the function
/// already holds.
static void func_dryfield_breezeway_8017E464(Task* arg0)
{
    TmdObject*      ext;
    GfxCoord*       coord;
    DbwEventWork*   work;
    OverlayHotspot* hs;

    ext   = arg0->extra.tmd;
    coord = ext->coords;

    work = (DbwEventWork*)memCalloc(0x60, false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }

    arg0->spawnArg2.pointer = Task_SpawnFromTable(&D_dryfield_breezeway_80182DC0, 0, 1, 0);
    do {
        arg0->msgTable                    = D_dryfield_breezeway_80182DCC;
        arg0->work                        = work;
        Mc_SaveData[0].state.at4.loc.view = 6;
    } while (0);
    arg0->state   += 1;
    work->field_40 = 0;
    Display_AcquireRef();

    hs = D_dryfield_breezeway_80182E00;
    while (hs->id != -1) {
        hs->hit = 0;
        hs++;
    }

    hs = D_dryfield_breezeway_80182DDC;
    while (hs->id != -1) {
        hs->hit = 0;
        hs++;
    }

    ext->colorMtx = &work->color;
    ext->flags    = 0;
    ext->lightMtx = &work->light;
    coord->parent = NULL;

    gGameSession->eventState   = 1;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    work->cursorX              = 0;
    work->cursorY              = 0x20;

    {
        DbwEventWork* eventWork = (DbwEventWork*)arg0->work;
        TmdObject*    eventObj  = arg0->extra.tmd;
        OverlayMat*   light     = (OverlayMat*)&eventWork->light;
        OverlayMat*   color     = (OverlayMat*)&eventWork->color;

        light->ident.m00_m01 = 0x1000;
        light->ident.m02_m10 = 0;
        light->ident.m11_m12 = 0x1000;
        light->ident.m20_m21 = 0;
        light->ident.m22     = 0x1000;

        color->ident.m00_m01 = 0x1000;
        color->ident.m02_m10 = 0;
        color->ident.m11_m12 = 0x1000;
        color->ident.m20_m21 = 0;
        color->ident.m22     = 0x1000;

        eventObj->lightMtx = &eventWork->light;

        eventWork->color.m[0][0] = 0x1000;
        eventWork->color.m[0][1] = 0x1000;
        eventWork->color.m[0][2] = 0x1000;
        eventWork->color.m[1][0] = 0x1000;
        eventWork->color.m[1][1] = 0x1000;
        eventWork->color.m[1][2] = 0x1000;
        eventWork->color.m[2][0] = 0x1000;
        eventWork->color.m[2][1] = 0x1000;
        eventWork->color.m[2][2] = 0x1000;

        eventWork->light.m[0][0] = 0x1000;
        eventWork->light.m[0][1] = 0x1000;
        eventWork->light.m[0][2] = 0x1000;
        eventWork->light.m[1][0] = 0;
        eventWork->light.m[1][1] = 0x1000;
        eventWork->light.m[1][2] = 0x1000;
        eventWork->light.m[2][0] = 0x1000;
        eventWork->light.m[2][1] = 0x1000;
        eventWork->light.m[2][2] = 0;

        eventObj->colorMtx = &eventWork->color;
        Gp_SetObjTrans(eventObj, 0x800, 0x800, 0x800);
    }
}

/// Main-executable symbols with no module header yet: `gDisplayState.animFrame` is the
/// frame counter the prop's swing angle is derived from, and `RotMatrixY`
/// is the Y rotation builder `ActorsShared80139948` also reaches.
///
/// Its `angle` parameter is declared `s32` rather than the `s16` the actor
/// headers use because the calls below feed it `rsin`'s `int` result, which
/// the target passes through untruncated.

/// The two image records the key-item prompt's scan uploads the first time it
/// runs, taken from the room's trailing data blob: the confirm and cancel
/// artwork `Gp_LoadImages` stages into VRAM.

/// Runs the key-item prompt's scan state: uploads this room's two prompt
/// `GpImgRec`s the first time it runs (`Task::killCountdown` is zero, and the
/// increment latches it so a later frame never reloads them), rebuilds the
/// event task's display object matrix as the same pure Y rotation of
/// `rsin(gDisplayState.animFrame * 16)` the prop's swing builds -- one full turn every 256
/// frames -- and re-seeds `func_dryfield_breezeway_8017EB8C` at the reset
/// position (0, 0x20) rather than at the cursor the prop's scan passes.
///
/// Highlighting the cursor (`mode` 1) is the state the scan runs in; landing on
/// `D_dryfield_breezeway_80182E00` -- the key-item prompt's own one-entry table,
/// where `func_dryfield_breezeway_8017E81C` reaches the two-entry prop table --
/// confirms it (`mode` 2) and walks that table for the entry whose `hit` is
/// raised. The entry's `id` and `promptKind` go to the event work block
/// (`DbwEventWork.field_4C` / `promptKind`), which
/// `func_dryfield_breezeway_8017FD9C` re-spawns the prompt from, and the task
/// advances to state 3. A cancel press (`buttons[1].state` 2) ends the script
/// in state 5, and a busy cap abandons the scan with the prompt cleared.
static void func_dryfield_breezeway_8017E65C(Task* task)
{
    DbwEventWork*     work;
    OverlayHotspot*   hs;
    RoomActionPrompt* prompt;
    GfxCoord*         coord;
    MATRIX*           m;

    coord  = task->extra.tmd->coords;
    work   = (DbwEventWork*)task->work;
    hs     = D_dryfield_breezeway_80182E00;
    prompt = D_80114D28;

    if (task->killCountdown == 0) {
        Gp_LoadImages(&D_dryfield_breezeway_80182F24[0]);
        Gp_LoadImages(&D_dryfield_breezeway_80183144[0]);
        task->killCountdown = (u16)task->killCountdown + 1;
    }

    m                    = &coord->coord;
    MATRIX_PAIR(m, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 1, 1) = 0x1000;
    *&m->m[2][2]         = 0x1000;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 2, 0) = 0;

    RotMatrixY(rsin(gDisplayState.animFrame * 0x10), m);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_dryfield_breezeway_8017EB8C(task, 0, 0x20);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_dryfield_breezeway_8017FCB4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if ((prompt->buttons.slots[0].state == 2) && (hs->id != -1)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_4C   = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
                hs++;
            } while (hs->id != -1);
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}

/// Breathes the room's hanging prop: rebuilds the display object's coordinate
/// matrix as a pure Y rotation of `rsin(gDisplayState.animFrame * 16)` -- one full turn
/// every 256 frames -- off an identity built the same word-at-a-time way
/// `func_dryfield_breezeway_8017E464` builds the event work's two matrices, then
/// re-seeds the hotspot scan `func_dryfield_breezeway_8017EB8C` at the
/// prompt's own screen position and hit-tests it against the room's table.
///
/// Highlighting the cursor (`mode` 1) is the state the scan runs in; landing on
/// an entry confirms it (`mode` 2) and walks `D_dryfield_breezeway_80182DDC`
/// for the entry that was hit, which is the prop the player is looking at --
/// pressing confirm against it runs cap slot 3 and ends the script in state 5.
/// A cancel press (`buttons[1].state` 2) ends it in state 5 as well.
static void func_dryfield_breezeway_8017E81C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    GfxCoord*         coord  = task->extra.tmd->coords;
    DbwEventWork*     work   = (DbwEventWork*)task->work;
    OverlayHotspot*   hs     = D_dryfield_breezeway_80182DDC;
    MATRIX*           m;

    prompt->mode     = 1;
    prompt->targetId = 0x80;

    m                                = &coord->coord;
    MATRIX_PAIR(&coord->coord, 0, 0) = 0x1000;
    MATRIX_PAIR(m, 1, 1)             = 0x1000;
    *&m->m[2][2]                     = 0x1000;
    MATRIX_PAIR(m, 0, 2)             = 0;
    MATRIX_PAIR(m, 2, 0)             = 0;

    RotMatrixY(rsin(gDisplayState.animFrame * 0x10), m);
    func_dryfield_breezeway_8017EB8C(task, prompt->screen.xy.x, prompt->screen.xy.y);

    if (func_dryfield_breezeway_8017FCB4(hs, work->cursorX, work->cursorY) != 0) {
        prompt->mode = 2;
        while (hs->id != -1) {
            if (hs->hit != 0) {
                Gp_RunCapCmd1(3);
                task->state = 5;
                return;
            }
            hs++;
        }
    }

    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `gGpuCurrentOt[1]`.
///
/// Nothing in this room calls it.
static void func_dryfield_breezeway_8017E948(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

/// The breezeway's cursor scan, run every frame its key-item event task is on a
/// state that watches the cursor. `arg1` / `arg2` are the position the scan
/// starts from - the prompt's own `screen` coordinates from
/// `func_dryfield_breezeway_8017E81C`, or the reset pair (0, 0x20) the other
/// states pass - and the scan answers by writing `D_80114D28::mode` (1 = over a
/// hotspot, 2 = confirmed) as well as advancing the hotspot's own animation.
static void func_dryfield_breezeway_8017EB8C(Task* task, s16 arg1, s16 arg2)
{
    s32           x;
    s32           ay;
    s32           ty;
    SVECTOR       pos;
    SVECTOR       out;
    SVECTOR       target;
    DbwBeamEdge   edge;
    DbwEventWork* work;
    s32           dist;
    s32           cdist;
    s32           dx;
    s32           dy;
    s16           y;
    s16           sx;
    s16           sy;
    s32           scale;
    s32           px;
    s32           py;
    s32           r;
    s32           d;
    s32           i;
    s16           a;
    s16           angle;
    s16           t;

    RoomActionPrompt* prompt;

    x      = arg1;
    ay     = arg2;
    work   = (DbwEventWork*)task->work;
    prompt = D_80114D28;
    ty     = ay + 0x50;
    dist   = SquareRoot0(x * x + ty * ty);
    sx     = work->cursorX;
    dx     = sx - prompt->screen.xy.x;
    sy     = work->cursorY;
    dy     = sy - prompt->screen.xy.y;
    cdist  = SquareRoot0(dx * dx + dy * dy);
    if (dist >= 0x70 || ay < -0x4F || cdist > 0x20) {
        prompt->mode   = 1;
        work->cursorY += work->field_52;
        dist           = 0x70;
        if (work->cursorY >= 0x20) {
            work->cursorY  = 0x20;
            work->field_52 = 0;
        } else {
            work->field_52++;
        }
        if (work->field_56 < 8) {
            work->field_58 = work->cursorX;
            if (work->cursorX > 0) {
                work->field_54 = (u16)(work->field_54 - 2) - work->field_56;
            }
            if (work->cursorX < 0) {
                work->field_54 = work->field_56 + (u16)(work->field_54 + 2);
            }
            work->cursorX += (s16)work->field_54 >> work->field_56;
            if ((work->cursorX > 0 && work->field_58 <= 0) || (work->cursorX < 0 && work->field_58 >= 0)) {
                work->field_56++;
            }
        }
    } else {
        prompt->mode   = 2;
        work->field_56 = 3;
        work->field_54 = 0;
        work->field_52 = 0;
        work->field_58 = work->cursorX;
        work->field_5A = work->cursorY;
        work->cursorX += (prompt->screen.xy.x - work->cursorX) >> 2;
        work->cursorY += (prompt->screen.xy.y - work->cursorY) >> 2;
        if (work->cursorX != work->field_58 || work->cursorY != work->field_5A) {
            SndEvt_EnqueueType6(0x5216000D, 0, 0);
        }
    }

    y     = work->cursorY;
    x     = work->cursorX;
    ty    = y + 0x50;
    scale = 0x800 - (ty << 12) / 224;
    scale = 0xE00 - scale;
    sx    = work->cursorX;
    sy    = work->cursorY;
    r     = 0x70 - dist;
    px    = (x * scale / 8) >> 9;
    py    = ((ty * scale / 8) >> 9) + ((r * scale / 8) >> 9);
    py   -= 0x50;

    target.vx = 0;
    target.vy = -0x50;
    target.vz = 0;
    pos.vx    = px;
    pos.vy    = py;
    pos.vz    = 0;
    a         = func_dryfield_breezeway_8017FBEC(0, -0x50, px, py);
    angle     = -((func_dryfield_breezeway_8017FBEC(px, py, x, y) + a) / 2) + 0x800;
    for (i = 0; i < 30; i++) {
        if (i == 0) {
            edge.mode = 0;
        } else {
            edge.mode = 1;
        }
        func_dryfield_breezeway_8017F1F4(angle, 4, (DbwVec*)&pos, (DbwVec*)&out, &edge);
        if (func_dryfield_breezeway_8017FAD0((DbwVec*)&target, (DbwVec*)&out) != 0) {
            break;
        }
        t   = angle + func_dryfield_breezeway_8017FBEC(out.vx, out.vy, 0, -0x50);
        d   = (t << 20) >> 20;
        pos = out;
        if (d > 0x200) {
            angle -= 0x200;
        } else if (d > 0x100) {
            angle -= 0x100;
        } else if (d > 0x80) {
            angle -= 0x80;
        } else if (d < -0x200) {
            angle += 0x200;
        } else if (d < -0x100) {
            angle += 0x100;
        } else if (d < -0x80) {
            angle += 0x80;
        } else {
            angle = -func_dryfield_breezeway_8017FBEC(out.vx, out.vy, 0, -0x50);
        }
    }

    target.vx = sx;
    target.vy = sy;
    target.vz = 0;
    pos.vx    = px;
    pos.vy    = py;
    pos.vz    = 0;
    a         = func_dryfield_breezeway_8017FBEC(0, -0x50, px, py);
    angle     = -((func_dryfield_breezeway_8017FBEC(px, py, sx, sy) + a) / 2);
    for (i = 0; i < 30; i++) {
        func_dryfield_breezeway_8017F1F4(angle, 4, (DbwVec*)&pos, (DbwVec*)&out, &edge);
        if (func_dryfield_breezeway_8017FAD0((DbwVec*)&target, (DbwVec*)&out) != 0) {
            break;
        }
        t   = angle + func_dryfield_breezeway_8017FBEC(out.vx, out.vy, sx, sy);
        d   = (t << 20) >> 20;
        pos = out;
        if (d > 0x200) {
            angle -= 0x200;
        } else if (d > 0x100) {
            angle -= 0x100;
        } else if (d > 0x80) {
            angle -= 0x80;
        } else if (d < -0x200) {
            angle += 0x200;
        } else if (d < -0x100) {
            angle += 0x100;
        } else if (d < -0x80) {
            angle += 0x80;
        } else {
            angle = -func_dryfield_breezeway_8017FBEC(out.vx, out.vy, sx, sy);
        }
    }
    func_dryfield_breezeway_8017FB30(task, out.vx, out.vy);
}

/// Draws one segment of the breezeway's prompt beam: a raw-textured quad of
/// two `arg0`-rotated edges, eight halfwords wide, whose far edge is `arg1`
/// down the rotated frame from its near one. All five probe points go through
/// `RotTransSV` (so `arg0` has to be a real rotation: the identity matrix the
/// two `Set` calls start from is splatted word-wise and then handed to
/// `RotMatrixZ`), and the quad is carved from `gGpuPrimCursor` and linked into
/// `gGpuCurrentOt[0x64]` with the room's tpage 0x8E / clut 0x4000 texture.
///
/// `arg2` is the scan's own position, added to every projected point; the
/// rotated probe at (0, `arg1`, 0) -- the far edge's centre -- lands in `arg3`
/// as the segment's tip, which is the position the caller advances its cursor
/// to. `arg4` carries the near edge: `mode` 0 draws it from the `arg2` origin
/// (the first segment of a beam), anything else from the two corners stored in
/// `arg4`, which the tail of every call overwrites with the far edge -- so a
/// beam that keeps being redrawn starts where the previous segment ended.
///
/// Nothing is written to `arg4`'s corners on a `mode` 0 call beyond that tail,
/// which is what makes the first segment of a beam run from the origin.
static void func_dryfield_breezeway_8017F1F4(s16 arg0, s16 arg1, DbwVec* arg2, DbwVec* arg3, DbwBeamEdge* arg4)
{
    SVECTOR    probe;
    DbwVec     tip;
    SVECTOR    near0;
    SVECTOR    near1;
    SVECTOR    far0;
    SVECTOR    far1;
    DbwVec     corner0;
    DbwVec     corner1;
    DbwVec     corner2;
    DbwVec     corner3;
    OverlayMat matw;
    MATRIX*    mtx;
    long       flag;
    POLY_FT4*  p;

    mtx                    = &matw.mat;
    matw.ident.m00_m01     = 0x1000;
    matw.ident.m02_m10     = 0;
    MATRIX_PAIR(mtx, 1, 1) = 0x1000;
    matw.ident.m20_m21     = 0;
    mtx->m[2][2]           = 0x1000;
    matw.mat.t[0]          = 0;
    matw.mat.t[1]          = 0;
    matw.mat.t[2]          = 0;
    RotMatrixZ(arg0, &matw.mat);
    SetRotMatrix(&matw.mat);
    SetTransMatrix(&matw.mat);

    probe.vx = 0;
    probe.vy = arg1;
    probe.vz = 0;
    RotTransSV(&probe, (SVECTOR*)&tip, &flag);
    arg3->vx = arg2->vx + tip.vx;
    arg3->vy = arg2->vy + tip.vy;
    arg3->vz = arg2->vz + tip.vz;

    if (arg4->mode == 0) {
        near0.vx = -4;
        near0.vy = 0;
        near0.vz = 0;
        RotTransSV(&near0, (SVECTOR*)&corner0, &flag);
        near1.vx = 4;
        near1.vy = 0;
        near1.vz = 0;
        RotTransSV(&near1, (SVECTOR*)&corner1, &flag);
    }

    far0.vx = -4;
    far0.vy = arg1;
    far0.vz = 0;
    RotTransSV(&far0, (SVECTOR*)&corner2, &flag);
    far1.vx = 4;
    far1.vy = arg1;
    far1.vz = 0;
    RotTransSV(&far1, (SVECTOR*)&corner3, &flag);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyFT4(p);
    p->tpage = 0x8E;
    p->clut  = 0x4000;

    if (arg4->mode == 0) {
        p->x0 = corner0.vx + arg2->vx;
        p->y0 = corner0.vy + arg2->vy;
        p->x1 = corner1.vx + arg2->vx;
        p->y1 = corner1.vy + arg2->vy;
    } else {
        p->x0 = arg4->fromA.vx;
        p->y0 = arg4->fromA.vy;
        p->x1 = arg4->fromB.vx;
        p->y1 = arg4->fromB.vy;
    }
    p->x2 = corner2.vx + arg2->vx;
    p->y2 = corner2.vy + arg2->vy;
    p->x3 = corner3.vx + arg2->vx;
    p->y3 = corner3.vy + arg2->vy;

    p->u0 = 0;
    p->v0 = 0;
    p->u1 = 0x10;
    p->v1 = 0;
    p->u2 = 0;
    p->v2 = 4;
    p->u3 = 0x10;
    p->v3 = 4;

    setShadeTex(p, 1);
    addPrim(&gGpuCurrentOt[0x64], p);

    arg4->fromA.vx = corner2.vx + arg2->vx;
    arg4->fromA.vy = corner2.vy + arg2->vy;
    arg4->fromB.vx = corner3.vx + arg2->vx;
    arg4->fromB.vy = corner3.vy + arg2->vy;
}

/// Per-frame cursor driver of the room's action prompt, state 1 of the prompt
/// task `func_dryfield_breezeway_8017FA80` runs.
///
/// `Task::spawnArg1` picks which pad ports take part: 1 drives port 0 only,
/// 2 port 1 only, anything else both. For each port it integrates the analog
/// stick (input format 0x12 reads it linearly, 0x73 squares it) and then the
/// d-pad -- whose four bits select one of eight 1/16-of-a-turn headings fed to
/// `rsin`/`rcos` -- into the prompt's 1/512-pixel position, clamps that to the
/// screen, classifies the confirm (0x40) and cancel (0xA0) buttons into the
/// prompt's two button slots, and hands the rounded position to
/// `func_dryfield_breezeway_8017F998` to draw the cursor.
/// `RoomActionPrompt::targetId` acts as the cursor speed here and `field_E` as
/// the double-press window: a second press inside that many frames without the
/// cursor having moved reports state 4 instead of 2.
static void func_dryfield_breezeway_8017F538(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               inputFormat;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1.value) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt      = &D_80114D28[port];
        pad         = &Pad_States[port];
        inputFormat = pad->inputFormat;
        if (inputFormat == PAD_INPUT_FORMAT_MOUSE) {
            speed            = prompt->targetId;
            step             = ((u16)pad->stickAxes[PAD_STICK_LEFT_X] << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->stickAxes[PAD_STICK_LEFT_Y] << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (inputFormat == PAD_INPUT_FORMAT_ANALOG) {
            stick = pad->stickAxes[PAD_STICK_LEFT_X];
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->stickAxes[PAD_STICK_LEFT_Y];
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons.halfwords[0];
        heldp  = &prompt->buttons.halfwords[1];
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                                          = 0;
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos = prompt->screen.packed;
                    *statep                                                             = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_dryfield_breezeway_8017F998(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues one 16x24 textured quad -- the room's on-screen action prompt icon --
/// at (`x`, `y`) into the head of the current OT. `variant` selects the palette,
/// 0x3C87 when it is 2 and 0x3C88 otherwise, and 0 draws nothing at all.
static void func_dryfield_breezeway_8017F998(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// The room's prompt task, run from `D_dryfield_breezeway_80182DC0`: state 0
/// resets both action-prompt slots (`func_dryfield_breezeway_8017FF1C`), state
/// 1 drives the cursor every frame after that
/// (`func_dryfield_breezeway_8017F538`). The handler pair is built on the stack
/// rather than read from rodata.
void func_dryfield_breezeway_8017FA80(Task* task)
{
    TaskFunc states[2] = { func_dryfield_breezeway_8017FF1C, func_dryfield_breezeway_8017F538 };

    states[task->state](task);
}

/// 1 when `pos` is closer than 9 units to `target`: a real distance, since the
/// sum of the two squared component differences is square-rooted before the
/// comparison. Only `vx` and `vy` take part. `func_dryfield_breezeway_8017EB8C`
/// asks this of each point it generates while it looks for somewhere to put the
/// hotspot prompt, and leaves its loop on the first point this accepts, so the
/// answer marks the candidate that has converged onto the target.
static s16 func_dryfield_breezeway_8017FAD0(DbwVec* target, DbwVec* pos)
{
    s16 dx = pos->vx - target->vx;
    s16 dy = pos->vy - target->vy;

    return SquareRoot0((dx * dx) + (dy * dy)) < 9;
}

/// Parks the room task's display object on the hotspot cursor: the position the
/// scan `func_dryfield_breezeway_8017EB8C` advanced to is carried into the
/// object's coordinate scaled by the depth it is placed at (`0x5DC` over 680),
/// and `composeStamp` is cleared so the next coord-tree update rebuilds the world matrix
/// from the new translation. The scan calls this once, as it leaves its loop.
static void func_dryfield_breezeway_8017FB30(Task* task, s16 arg1, s16 arg2)
{
    GfxCoord* coord = task->extra.tmd->coords;

    coord->coord.t[2]   = 0x5DC;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[0]   = (arg1 * 0x5DC) / 680;
    coord->coord.t[1]   = (arg2 * 0x5DC) / 680;
}

/// `GpMsgEntry` handler for message 0x13F1, the "can this key item be used
/// here?" query `Gp_UseKeyItemRow` sends to slot 7. `item` is the key item the
/// player highlighted; 0x11B is the only one the breezeway accepts, and the
/// answer is latched in the work block's `field_40` for
/// `func_dryfield_breezeway_8017FE08` to pick its next state from.
s32 func_dryfield_breezeway_8017FBC8(Task* task, s32 msgId, s32 item, s32 arg3)
{
    DbwEventWork* work = (DbwEventWork*)task->work;

    if (item == 0x11B) {
        work->field_40 = 1;
        return 1;
    }
    work->field_40 = 0;
    return 0;
}

/// The `ratan2` angle of the direction from (`arg0`, `arg1`) to (`arg2`, `arg3`).
static s16 func_dryfield_breezeway_8017FBEC(s16 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    SVECTOR vec;

    vec.vx = arg2 - arg0;
    vec.vy = arg3 - arg1;
    vec.vz = 0;
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vy);
}

/// The room's key-item event task, run from `D_dryfield_breezeway_80182E18`:
/// dispatches the current state through the seven handlers of
/// `D_dryfield_breezeway_8017D5E8`, copied onto the stack first so the call
/// goes through a local table rather than through `.rodata`.
void func_dryfield_breezeway_8017FC38(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_breezeway_8017D5E8;
    sp.funcs[task->state](task);
}

/// Hit-tests the point (`x`, `y`) against the 0xFFFF-terminated hotspot table
/// `table`, raising `hit` on every entry whose rectangle contains the point and
/// clearing it on every other one. Returns non-zero if any entry was hit.
static s32 func_dryfield_breezeway_8017FCB4(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// State 1 of the room's key-item event task: arms the action prompt and
/// resets the caller's kill countdown. It highlights the prompt for the fixed
/// target id 0x80, clears the on-screen position `func_800D4E78` fills in
/// again when the prompt is spawned, and steps the caller's script on one
/// state.
static void func_dryfield_breezeway_8017FD68(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// State 3 of the room's key-item event task, run once the room's hotspot
/// scan has landed on an entry: re-seeds the cursor scan
/// `func_dryfield_breezeway_8017EB8C` at its reset position, clears the
/// prompt's highlight state, then re-spawns the prompt at the coordinates the
/// gameplay side left in `D_80114D28` with the display mode the scan latched in
/// `DbwEventWork::promptKind`, and steps the caller's script on one state.
static void func_dryfield_breezeway_8017FD9C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    DbwEventWork*     work   = (DbwEventWork*)task->work;

    func_dryfield_breezeway_8017EB8C(task, 0, 0x20);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Closes whatever the hotspot scan left up and picks the room's next state:
/// re-seeds the cursor scan `func_dryfield_breezeway_8017EB8C` and clears the
/// prompt's highlight state as the arm above does, then interrogates the
/// gameplay side. While `func_800D4EC0` still reports a prompt on screen there
/// is nothing to decide, so the arm tears one down with cap slot 7 and parks on
/// state 2; once it is gone the `DbwEventWork::field_40` answer latch the
/// message handler `func_dryfield_breezeway_8017FBC8` wrote decides between
/// state 6 (the key item was accepted here) and state 2.
static void func_dryfield_breezeway_8017FE08(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    DbwEventWork*     work   = (DbwEventWork*)task->work;
    s32               state;

    func_dryfield_breezeway_8017EB8C(task, 0, 0x20);
    prompt->mode     = 0;
    prompt->targetId = 0;
    if (func_800D4EC0() != 0) {
        Gp_StartCapSlot(7, 0, 0);
        state = 2;
    } else if (work->field_40 == 1) {
        state = 6;
    } else {
        state = 2;
    }
    /* `*&state`: taking the address keeps `state` in a stack slot, so the arms
       above are memory stores rather than the register assignments jump.c's
       `if (c) x = a; else x = b;` fold needs to hoist the else arm over the
       `field_40` test. Keeping that arm in its own block is what puts the value
       in $v0. */
    task->state = *&state;
}

/// Exit state of the room's key-item event task, undoing its set-up
/// (`func_dryfield_breezeway_8017E464`): sends the two player messages with 1,
/// releases the display reference, clears the session's event, HUD and
/// cutscene holds, puts `Mc_SaveData[0].state.at4.loc.view` back from 6 to 4, kills the prompt task
/// the set-up spawned (`Task::spawnArg2`) and asks for its own removal.
static void func_dryfield_breezeway_8017FE90(Task* arg0)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    Mc_SaveData[0].state.at4.loc.view = 4;
    /* Without the barrier GCC fills taskKill's delay slot with the byte store. */
    taskKill((Task*)arg0->spawnArg2.pointer);
    Task_RequestKill(arg0, 0);
}

/// Resets both action-prompt slots before a script's first cursor scan and steps
/// the caller on one state: clears each slot's leading words and its two
/// trailing shorts, parks the target id at 0x100 with `field_E` at 0xF, and
/// marks the slot as highlighted (`mode` 1).
static void func_dryfield_breezeway_8017FF1C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0                     = 0;
        prompt->field_4                     = 0;
        prompt->targetId                    = 0x100;
        prompt->field_E                     = 0xF;
        prompt->buttons.slots[0].heldFrames = 0;
        prompt->buttons.slots[1].heldFrames = 0;
        prompt->mode                        = 1;
    }
    task->state = task->state + 1;
}

void func_dryfield_breezeway_8017FF7C(Task* task)
{
    s32        mask;
    GpEffWork* eff;
    GfxCoord*  coord;
    GfxCoord*  player;
    s32        limit;
    s32        pan;

    mask   = 1 << gGameSession->at4.loc.view;
    eff    = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    player = gameGetPtrSlot(3)->extra.tmd->coords;
    if (mask & 0x18) {
        func_dryfield_breezeway_8018034C(coord, D_dryfield_breezeway_80183164, 0x600, 0x80);
    } else if (mask & 0x20) {
        func_dryfield_breezeway_80180858(coord, D_dryfield_breezeway_80183164, 0x600, 0x10);
    }
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    Gp_State1C->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    if (GameFlag_GetNibble(0x5D) == 0) {
        if (gGameSession->at4.loc.view == 2) {
            limit        = (player->coord.t[0] - 5856) >> 7;
            eff->move.vx = 12000;
            eff->move.vy = -3000;
            eff->move.vz = 3000;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((u16)((Gp_LcgState >> 16) % 100) < limit) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003C, coord, (s32)(Gp_LcgState >> 16) % limit + 0x40, &eff->move);
            }
            if (eff->step == 0) {
                SndEvt_EnqueueType6(0x5216000A, 0, 0);
                eff->step = 1;
            }
        } else if (gGameSession->at4.loc.view == 3) {
            eff->scale   = 0x10;
            eff->move.vx = player->coord.t[0] + 0x100;
            eff->move.vy = -3000;
            eff->move.vz = 3000;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0x7F) + 0x40, &eff->move);
            if (eff->step < 2) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (!((Gp_LcgState >> 16) & 3)) {
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(0x5216000B, pan, (s8)gpGetObjDepth(coord));
                    eff->step = 2;
                }
            }
        }
    } else if (GameFlag_GetNibble(0x5D) == 1) {
        if (eff->step != 0) {
            SndEvt_EnqueueType7(0x5216000A, 0);
            eff->step = 0;
        }
        if (eff->scale != 0) {
            eff->scale--;
            eff->move.vx = 16000;
            eff->move.vy = -3000;
            eff->move.vz = 2750;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0xFF) + 0x40, &eff->move);
            eff->move.vx = 17000;
            eff->move.vy = -3000;
            eff->move.vz = 4000;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0xFF) + 0x40, &eff->move);
        }
    }
}

/// Draws a red light shaft at a point. `data` is rotated by
/// `coord`'s `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` into a 0x14-byte scratch stack block; nothing is drawn when
/// `otz` is 0x10 or less. Two gouraud `POLY_G4` halves of half width
/// `(s16)arg3 * 32 / otz` and two `LINE_G3` diagonals meet at the projected
/// point, whose vertex pulses red as `rsin(animFrame * arg2) / 34 + 0x78`.
static void func_dryfield_breezeway_8018034C(GfxCoord* coord, u8* data, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               i;
    s32               color;
    s32               pulse;
    s32               twice;
    s32               t;
    s32               t2;

    Gp_UpdateCoord(coord);
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
    head     = *scratch;
    *scratch = head - 0x14;
    block    = (RoomShaftScratch*)(head - 0x14);

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0();
    gte_stsv(&((RoomShaftScratch*)(head - 0x14))->vec);
    block->vec.vx = (u16)block->vec.vx + (u16)coord->workm.t[0];
    block->vec.vy = (u16)block->vec.vy + (u16)coord->workm.t[1];
    block->vec.vz = (u16)block->vec.vz + (u16)coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomShaftScratch*)(head - 0x14))->vec);
    gte_rtps();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        pulse            = rsin(gDisplayState.animFrame * (s16)arg2);
        i                = 0;
        block->halfWidth = ((s16)arg3 << 5) / ((RoomShaftScratch*)(head - 0x14))->otz;
        color            = pulse / 34 + 0x78;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->halfWidth;
            prim->x1 = prim->x2 = block->sx;
            prim->x3            = block->sx + (u16)block->halfWidth;
            prim->y0 = prim->y2 = prim->y3 = block->sy;
            twice                          = i << 1;
            prim->y1                       = (block->sy - (u16)block->halfWidth) + block->halfWidth * twice;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, color, 0, 0);
            setRGB2(line, 0, 0, 0);
            t        = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x14);
}

static void func_dryfield_breezeway_80180858(GfxCoord* coord, u8* data, s32 arg2, s32 arg3)
{
    u8*              head;
    RoomGlowScratch* block;
    POLY_G4*         prim;
    s32              pulse;
    s32              color;
    s32              half;
    s32              size;
    s32              ang;
    s32              t;
    s32              t2;
    s32              u;

    Gp_UpdateCoord(coord);
    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - 0x18);
        block   = (RoomGlowScratch*)tmp;
    }

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0();
    gte_stsv(&((RoomGlowScratch*)(head - 0x18))->vec);
    block->vec.vx += coord->workm.t[0];
    block->vec.vy += coord->workm.t[1];
    block->vec.vz += coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomGlowScratch*)(head - 0x18))->vec);
    gte_rtps();
    gte_stsxy(&((RoomGlowScratch*)(head - 0x18))->sx);
    gte_stszotz(&block->otz);
    if (((RoomGlowScratch*)(head - 0x18))->otz > 16) {
        pulse         = rsin(gDisplayState.animFrame * (s16)arg2);
        ang           = 0;
        size          = (s16)arg3;
        block->rOuter = (size * 64) / ((RoomGlowScratch*)(head - 0x18))->otz;
        color         = pulse / 34 + 0x78;
        block->rInner = (size * 8) / ((RoomGlowScratch*)(head - 0x18))->otz;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            half = (s16)color >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, half, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        color = (u16)half;
        ang   = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Per-frame update for a bouncing sprite particle drawn by
/// `func_dryfield_breezeway_80181938`. The first frame resets the model's
/// rotation, rolls a frame period, start frame, angle and spin from the LCG,
/// picks a random direction in `move` when none was supplied, and
/// normalises it. Afterwards it steps along `move` at speed `scale`
/// and tests the step with `func_800DE7CC`; a hit undoes the step, blends the
/// direction with the returned vector, halves speed and spin, and spawns
/// effect 0x60054 while the particle is young, settling into state 2 once hits
/// come close together at low speed. A miss adds `0x5000 / scale` to the
/// direction's y component. Over `age` the sprite fades from 30 to 60 and
/// is then released. The age does not advance while an event is running.
void func_dryfield_breezeway_80181264(Task* task)
{
    GpEffWork* work  = task->spawnArg2.pointer;
    GfxCoord*  coord = task->extra.coordBody->coord;
    MATRIX*    m;
    SVECTOR    delta;
    SVECTOR    dir;
    SVECTOR    pos;
    u8         color[3];

    if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }

    Gp_UpdateCoord(coord);
    work->age++;

    switch (task->state) {
        case 0:
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = (u16)task->spawnArg1.value & 0xFFF;
            work->scale          = 0x50;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->pos.vy         = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->index          = ((u32)Gp_LcgState >> 16) & 7;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->pos.vz         = ((u32)Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
            work->period         = 0x200 - (((u32)Gp_LcgState >> 16) & 0x3FF);
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vx = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vy = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vz = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
            break;
        case 1:
            if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            } else {
                work->pos.vz += work->period;
                if (work->pos.vy != 0 && work->age % work->pos.vy == 0) {
                    work->index++;
                }
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&delta);
                coord->coord.t[0]  += delta.vx;
                coord->coord.t[1]  += delta.vy;
                coord->coord.t[2]  += delta.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&delta);
                gte_rtv0();
                gte_stsv(&dir);
                pos.vx  = coord->workm.t[0];
                pos.vy  = coord->workm.t[1];
                pos.vz  = coord->workm.t[2];
                dir.vx += pos.vx;
                dir.vy += pos.vy;
                dir.vz += pos.vz;
                if (func_800DE7CC(&dir, &pos, &dir, &pos) == 1) {
                    coord->coord.t[0] -= delta.vx;
                    coord->coord.t[1] -= delta.vy;
                    coord->coord.t[2] -= delta.vz;
                    work->move.vx      = (pos.vx >> 1) + (work->move.vx >> 1);
                    work->move.vy      = pos.vy + (work->move.vy >> 1);
                    work->move.vz      = (pos.vz >> 1) + (work->move.vz >> 1);
                    VectorNormalSS(&work->move, &work->move);
                    work->scale  = work->scale >> 1;
                    work->period = work->period >> 1;
                    gte_lddp(work->scale);
                    gte_ldsv(&work->move);
                    gte_gpf12();
                    gte_stsv(&delta);
                    coord->coord.t[0] += delta.vx;
                    coord->coord.t[1] += delta.vy;
                    coord->coord.t[2] += delta.vz;
                    if (work->age < 60) {
                        Gp_SpawnEff(0x60054, coord, work->pos.vx + 0x2100, NULL);
                    }
                    if (work->age - work->step < 8 && work->scale < 0x20) {
                        task->state = 2;
                    } else {
                        work->step = work->age;
                    }
                } else if (work->scale > 0) {
                    work->move.vy += 0x5000 / work->scale;
                }
            }
            if (work->age < 30) {
                func_dryfield_breezeway_80181938(task, NULL);
            } else if (work->age < 60) {
                color[0] = color[1] = color[2] = (60 - work->age) * 4;
                func_dryfield_breezeway_80181938(task, color);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
        case 2:
            if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            }
            if (work->age < 30) {
                func_dryfield_breezeway_80181938(task, NULL);
            } else if (work->age < 60) {
                color[0] = color[1] = color[2] = (60 - work->age) * 4;
                func_dryfield_breezeway_80181938(task, color);
            } else {
            release:
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

/// Draws `task`'s effect as a camera-facing 16x16 `POLY_FT4` sprite at the
/// translation of its body's single coordinate, through a 0x1C-byte scratch stack
/// block. Nothing is drawn when the projection flags a negative result. The
/// frame is `index & 7` along row 0xF0 of texture page 0x2B, and the quad's
/// half extent is `pos.vx * 23 / otz`, rotated by the angle in `pos.vz`.
/// A non-null `color` tints the sprite and makes it semi-transparent.
static void func_dryfield_breezeway_80181938(Task* task, u8* color)
{
    ModelObjectCoordBody* body = task->extra.coordBody;
    GpEffWork*            work = task->spawnArg2.pointer;
    void**                scratch;
    GfxCoord*             coord;
    GpFxQuadScratch*      block;
    POLY_FT4*             prim;

    scratch       = SCRATCH_HEAD_ADDR;
    coord         = body->coord;
    block         = SCRATCH_PUSH_AT(scratch, GpFxQuadScratch);
    block->vec.vx = coord->workm.t[0];
    block->vec.vy = coord->workm.t[1];
    block->vec.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = 0x2B;
        prim->clut  = 0x43C0;
        prim->u0    = (work->index & 7) * 16;
        prim->v0    = 0xF0;
        prim->u1    = (work->index & 7) * 16 + 0xF;
        prim->v1    = 0xF0;
        prim->u2    = (work->index & 7) * 16;
        prim->v2    = 0xFF;
        prim->u3    = (work->index & 7) * 16 + 0xF;
        prim->v3    = 0xFF;
        block->dx   = (((work->pos.vx * 0x17) / block->otz) * rsin(work->pos.vz)) >> 12;
        block->dy   = (((work->pos.vx * 0x17) / block->otz) * rcos(work->pos.vz)) >> 12;
        prim->x0    = block->sx + block->dx;
        prim->x3    = block->sx - block->dx;
        prim->y0    = block->sy - block->dy;
        prim->y3    = block->sy + block->dy;
        block->dx   = (((work->pos.vx * 0x17) / block->otz) * rsin(work->pos.vz + 0x400)) >> 12;
        block->dy   = (((work->pos.vx * 0x17) / block->otz) * rcos(work->pos.vz + 0x400)) >> 12;
        prim->x1    = block->sx + block->dx;
        prim->x2    = block->sx - block->dx;
        prim->y1    = block->sy - block->dy;
        prim->y2    = block->sy + block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_POP_BYTES(0x1C);
}
