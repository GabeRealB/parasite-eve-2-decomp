#include "rooms/acropolis_security_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_input.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/action_prompt.h"
#include "../../shared/actor_contacts.h"

/// 0xA work block of the security-monitor task, hung off the `Task::work`
/// slot (0x1C) -- that slot is *not* a `TaskIdMap` here, it is the
/// `memCalloc(0xA)` block `func_acropolis_security_room_8017D9DC` allocates.
/// Reach it with `(AsrMonitorWork*)task->work`.
///
/// `cameraId` is the camera the monitor is currently showing, seeded from the
/// `GameFlag_GetNibble(0x2A)` lookup table and offset by 0x7F before being
/// handed to the panel drawer `func_acropolis_security_room_8017E0C4`.
/// `blinkTimer` is the cursor blink counter the overlay drawer
/// `func_acropolis_security_room_8017E37C` advances, `selection` the row the
/// player has highlighted, and `promptKind` the display mode forwarded to
/// `func_800D4E78` when the action prompt is spawned.
typedef struct AsrMonitorWork {
    /* 0x00 */ u16  cameraId;
    /* 0x02 */ s16  blinkTimer;
    /* 0x04 */ s16  selection;
    /* 0x06 */ s8   promptKind;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ s8   field_8;
    /* 0x09 */ byte pad_9[0x1];
} AsrMonitorWork;
STATIC_ASSERT_SIZEOF(AsrMonitorWork, 0xA);

/// Scratch state of the security-room cap script, stored at `Task::work`.
///
/// `func_acropolis_security_room_8017FA18` -- state 0 of the family whose
/// handler table is `D_acropolis_security_room_8017D63C` -- allocates it with
/// `memCalloc(0x10, 0)` and stores it straight into the `Task::work` slot,
/// so the size below is the allocation and not a guess; the same function
/// parks the family's `TaskMessageEntry[]` in `Task::msgTable`. The overlay's other
/// two allocators (`memCalloc(0xA)` in `func_acropolis_security_room_8017D9DC`
/// and `memCalloc(4)` in `func_acropolis_security_room_80180368`) belong to
/// other task families and to a different block.
typedef struct AcropolisSecurityRoomState {
    /* 0x0 */ s32   field_0;    // sub-step picked by the previous cap event
    /* 0x4 */ Task* child;      // task this state spawned, polled by Task_PollKill
    /* 0x8 */ u16   frames;     // frames the current state has been running
    /* 0xA */ s16   variant;    // 0 = the pair-4 cap script, 1 = the pair-3 one
    /* 0xC */ s8    promptKind; // display mode forwarded to `func_800D4E78`
    /* 0xD */ byte  pad_D[0x3];
} AcropolisSecurityRoomState;
STATIC_ASSERT_SIZEOF(AcropolisSecurityRoomState, 0x10);

/// Scratch state of the security-room ambience task, stored at `Task::work`.
/// `func_acropolis_security_room_80180368` allocates it with `memCalloc(4, 0)`,
/// so the size below is the allocation and not a guess.
typedef struct {
    /* 0x0 */ u16  fadeStarted; // the looping ambience has already been faded out
    /* 0x2 */ byte pad_2[0x2];
} AsrAmbienceState;

/// One frame of the 128x128 textured quad `func_acropolis_security_room_80180E34`
/// draws: the sprite is centred on (`x`, `y`) with texture page 0xAB, the CLUT
/// `clut << 6` and its top-left texel at (`u`, `v`), the other three corners
/// being that texel plus 0x7F on each axis. The table
/// (`D_acropolis_security_room_80183970`) has four entries and is indexed by
/// the low two bits of the drawing task's `Task::spawnArg1`.
typedef struct AsrSpriteFrame {
    /* 0x0 */ u16  clut;
    /* 0x2 */ u8   u;
    /* 0x3 */ byte pad_3[1];
    /* 0x4 */ u8   v;
    /* 0x5 */ byte pad_5[1];
    /* 0x6 */ u16  x;
    /* 0x8 */ u16  y;
} AsrSpriteFrame;
STATIC_ASSERT_SIZEOF(AsrSpriteFrame, 0xA);

/// 0x14-byte scratch block `func_acropolis_security_room_80180A78` takes from
/// the scratch stack while it draws the security laser. `a` and `b` are the two
/// endpoints of the beam in the emitter's local frame; each is rotated by the
/// emitter's `GfxCoord::workm` and then biased by that matrix's
/// translation, so both end up in world space. `otz` receives `SZ3 >> 2` from
/// the `RTPS` of `a` and doubles as the OT slot selector.
typedef struct AsrBeamScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR a;
    /* 0x0C */ SVECTOR b;
} AsrBeamScratch;
STATIC_ASSERT_SIZEOF(AsrBeamScratch, 0x14);

/// Projection and radius scratch for the security-room flash effect.
typedef struct AsrFlashScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     step;
    /* 0x08 */ SVECTOR v;
    /* 0x10 */ s16     x;
    /* 0x12 */ s16     y;
} AsrFlashScratch;
STATIC_ASSERT_SIZEOF(AsrFlashScratch, 0x14);

/// The tasks `func_acropolis_security_room_8017D77C` and
/// `func_acropolis_security_room_8017D834` spawn and poll until they end;
/// message 0x13F1 is forwarded to the second while it is alive.
extern Task* D_acropolis_security_room_801855A8;
extern Task* D_acropolis_security_room_801855AC;

/// The whole-unit world displacement the last `ActorContact_PushContact`
/// call produced.
extern SVECTOR ActorContact_ScratchPosition;

/// The contact routines' scratch position.
static inline SVECTOR* ActorContact_GetScratchPosition(void)
{
    return &ActorContact_ScratchPosition;
}

/// The room's message table, parked in `Task::msgTable` by its message task.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.
typedef struct {
    s32 id;
    union {
        TaskMessageHandler call0;
        TaskMessageHandler call1;
        void               (*call2)(Task*, s32, DirectionActionRequest*);
    } handler;
} AcropolisSecurityRoomMsgEntry;
STATIC_ASSERT_SIZEOF(AcropolisSecurityRoomMsgEntry, 8);

extern AcropolisSecurityRoomMsgEntry D_acropolis_security_room_801825DC[];
extern TaskDesc                      D_acropolis_security_room_80182618[];
extern TaskDesc                      D_acropolis_security_room_8018263C;

/// The security monitor's own hotspot table, hit-tested by
/// `actionPromptHitTest`.
extern OverlayHotspot D_acropolis_security_room_80182648[];

/// The five camera ids the security monitor can display, in the order the
/// `GameFlag_GetNibble(0x2A)` nibble indexes them.
extern s16 D_acropolis_security_room_801826B4[];

/// The single-entry `TaskDesc` table the script spawns its child task from:
/// `func_acropolis_security_room_8017F9C8`.
extern TaskDesc D_acropolis_security_room_801826C0[];
/// The script's message table, parked in `Task::msgTable`.
extern TaskMessageEntry D_acropolis_security_room_801826CC[];

/// The script's hotspot table, terminated by an entry whose `id` is -1.
extern OverlayHotspot D_acropolis_security_room_801826DC[];

/// The two `TaskDesc`s this room's script spawns from: index 0 is
/// `func_acropolis_security_room_80180368`, index 1 is
/// `func_acropolis_security_room_801804CC`.
extern TaskDesc D_acropolis_security_room_80182700[];

/// The 0x100-entry RGB555 palette every monitor-screen CLUT is blended
/// towards: the "off" colours of the four security-camera feeds.
extern u16 D_acropolis_security_room_80182718[];
/// The four lit palettes, one per camera feed, blended against
/// `D_acropolis_security_room_80182718` by the feed's own brightness.
extern u16 D_acropolis_security_room_80182918[];
extern u16 D_acropolis_security_room_80182B18[];
extern u16 D_acropolis_security_room_80182D18[];
extern u16 D_acropolis_security_room_80182F18[];
/// The four blend results, uploaded to VRAM by
/// `D_acropolis_security_room_80183918`.
// Color/byte updates and the GPU upload share the same backing storage.
typedef union {
    u16    colors[256];
    u_long words[128];
} AcropolisSecurityRoomPalette;
STATIC_ASSERT_SIZEOF(AcropolisSecurityRoomPalette, 512);

extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183118;
extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183318;
extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183518;
extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183718;
/// The upload records for the four blended CLUTs above.
extern GpuImageUpload D_acropolis_security_room_80183918[];
/// Camera-lit bitmask for each value of `GameFlag_GetNibble(9)`; bit N is set
/// while feed N is showing something.
extern u16 D_acropolis_security_room_80183968[];

/// The four sprite frames `func_acropolis_security_room_80180E34` picks from.
extern AsrSpriteFrame D_acropolis_security_room_80183970[];

/// Spawn position and effect id of the flash each newly lit feed plays,
/// indexed by feed.
extern SVECTOR D_acropolis_security_room_80183998[];
extern s16     D_acropolis_security_room_801839B8[];

extern EffectUnitQuadCorner D_acropolis_security_room_801839C0[];

/// 0xFF-terminated area-record lists applied as the script ends.
extern GpAreaApplyRec D_acropolis_security_room_80184F50[];
extern GpAreaApplyRec D_acropolis_security_room_80184F78[];
extern GpAreaApplyRec D_acropolis_security_room_80184F7C[];
extern GpAreaApplyRec D_acropolis_security_room_80184F80[];

static void func_acropolis_security_room_8017D930(Task* task);
static void func_acropolis_security_room_8017D97C(Task* task);
static void func_acropolis_security_room_8017D9DC(Task* task);
static void func_acropolis_security_room_8017DB30(Task* task);
static void func_acropolis_security_room_8017DC7C(Task* task);
static void func_acropolis_security_room_8017E0C4(s16 id);
static void func_acropolis_security_room_8017E37C(Task* task);
static void func_acropolis_security_room_8017EA28(Task* task);
static void func_acropolis_security_room_8017EA5C(Task* task);
static void func_acropolis_security_room_8017EADC(Task* task);
static void func_acropolis_security_room_8017EB9C(Task* task);
static void func_acropolis_security_room_8017EE44(Task* task);
static void func_acropolis_security_room_8017F480(Task* task);
static void func_acropolis_security_room_8017F8E0(s32 x, s32 y, s32 variant);
static void func_acropolis_security_room_8017FA18(Task* task);
static void func_acropolis_security_room_8017FB20(Task* task);
static void func_acropolis_security_room_8017FB54(Task* task);
static void func_acropolis_security_room_8017FBA4(Task* task);
static void func_acropolis_security_room_8017FC30(Task* task);
static s32  func_acropolis_security_room_8017FCB0(OverlayHotspot* table, s16 x, s16 y);
static void func_acropolis_security_room_8017FD64(s32 flags);
static void func_acropolis_security_room_8017FE6C(Task* task);
static void func_acropolis_security_room_8017FF0C(Task* task);
static void func_acropolis_security_room_8017FF84(Task* task);
static void func_acropolis_security_room_8017FFD0(Task* task);
static void func_acropolis_security_room_80180010(Task* task);
static void func_acropolis_security_room_80180030(Task* task);
static void func_acropolis_security_room_801800A4(Task* task);
static void func_acropolis_security_room_8018014C(Task* task);
static void func_acropolis_security_room_801801C4(Task* task);
static void func_acropolis_security_room_80180218(Task* task);
static void func_acropolis_security_room_80180308(Task* task);
static void func_acropolis_security_room_80180A78(Task* task);

void func_acropolis_security_room_8017E9D8(Task*);
void func_acropolis_security_room_8017F9C8(Task*);
s32  func_acropolis_security_room_8017FE24(Task*, s32, s32, s32);
void func_acropolis_security_room_80180368(Task*);
void func_acropolis_security_room_801804CC(Task*);

void func_acropolis_security_room_8017D77C(Task*);
void func_acropolis_security_room_8017D834(Task*);

extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183118;
extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183318;
extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183518;
extern AcropolisSecurityRoomPalette D_acropolis_security_room_80183718;
extern WorldCollisionGrid           D_acropolis_security_room_80183D94[1];
extern WorldCollisionTrigger        D_acropolis_security_room_80183DB8[4];
extern WorldCollisionTrigger        D_acropolis_security_room_80183EE8[5];
extern WorldCoordRoomLights         D_acropolis_security_room_801841C8[1];

extern SpriteBatch  D_acropolis_security_room_801841E0[2];
extern SpriteBatch  D_acropolis_security_room_80184358[3];
extern SpriteBatch  D_acropolis_security_room_80184370[2];
extern SpriteBatch  D_acropolis_security_room_80184380[2];
extern SpriteBatch  D_acropolis_security_room_80184458[3];
extern SpriteBatch  D_acropolis_security_room_80184498[4];
extern SpriteBatch  D_acropolis_security_room_801844B8[2];
extern SpriteSource D_acropolis_security_room_801841F0[18];
extern SpriteSource D_acropolis_security_room_80184390[10];
extern SpriteSource D_acropolis_security_room_80184470[2];

s32  func_acropolis_security_room_8017D6AC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_acropolis_security_room_8017D6D4(Task*, s32, s32, s32);
s32  func_acropolis_security_room_8017D708(Task*, s32, s32, s32);
void func_acropolis_security_room_8017D740(Task*, s32, DirectionActionRequest* request);

AcropolisSecurityRoomMsgEntry D_acropolis_security_room_801825DC[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, { .call0 = func_acropolis_security_room_8017D6AC } },
    { DIRECTION_MESSAGE_ROOM_ACTION, { .call2 = func_acropolis_security_room_8017D740 } },
    { ROOM_MESSAGE_COMMAND, { .call1 = func_acropolis_security_room_8017D708 } },
    { 5105, { .call1 = func_acropolis_security_room_8017D6D4 } },
    { TASK_MESSAGE_TABLE_END, { .call0 = NULL } },
};

AnimationPlayRequest D_acropolis_security_room_80182604 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

TaskDesc D_acropolis_security_room_80182618[3] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_security_room_8017D77C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_security_room_8017D834, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskDesc D_acropolis_security_room_8018263C = { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_8017E9D8, { .value = 0 } };

OverlayHotspot D_acropolis_security_room_80182648[9] = {
    { -30, 81, 12, 11, 8, 1, 0 },
    { -7, 81, 12, 11, 9, 1, 0 },
    { 16, 81, 12, 11, 10, 1, 0 },
    { 39, 81, 12, 11, 11, 1, 0 },
    { 61, 81, 12, 11, 12, 1, 0 },
    { 82, 81, 12, 11, 13, 1, 0 },
    { -141, -76, 12, 11, -0x8000, 1, 0 },
    { -141, -58, 12, 11, -0x7FFF, 1, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

s16 D_acropolis_security_room_801826B4[6] = {
    4,
    66,
    128,
    190,
    252,
    8481,
};

TaskDesc D_acropolis_security_room_801826C0[1] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_8017F9C8, { .value = 0 } },
};

TaskMessageEntry D_acropolis_security_room_801826CC[2] = {
    { 5105, func_acropolis_security_room_8017FE24 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

OverlayHotspot D_acropolis_security_room_801826DC[3] = {
    { -68, 20, 24, 24, 1, 0, 0 },
    { 54, 20, 24, 24, 2, 0, 0 },
    { 0, 0, 0, 0, -1, 0, 0 },
};

TaskDesc D_acropolis_security_room_80182700[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_80180368, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_security_room_801804CC, { .value = 0 } },
};

u16 D_acropolis_security_room_80182718[256] = { 0 };

u16 D_acropolis_security_room_80182918[256] = {
    0,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8421,
    0x8422,
    0x8422,
    0x8422,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8443,
    0x8443,
    0x8443,
    0x8443,
    0x8463,
    0x8463,
    0x8463,
    0x8463,
    0x8463,
    0x8463,
    0x8863,
    0x8863,
    0x8863,
    0x8463,
    0x8863,
    0x8484,
    0x8864,
    0x8864,
    0x8884,
    0x8884,
    0x8484,
    0x8884,
    0x8884,
    0x8884,
    0x8885,
    0x8885,
    0x8885,
    0x84A5,
    0x88A5,
    0x88A5,
    0x88A5,
    0x88A5,
    0x8CA5,
    0x84C6,
    0x8CA5,
    0x8CA6,
    0x8CA6,
    0x8CA6,
    0x8CC6,
    0x88C6,
    0x84C6,
    0x8CC6,
    0x8CC7,
    0x8CC7,
    0x84E7,
    0x8CC7,
    0x8CE6,
    0x8CE7,
    0x8CE7,
    0x8CE7,
    0x8CE7,
    0x8CE7,
    0x88E7,
    0x90E7,
    0x90E7,
    0x90E8,
    0x9108,
    0x9108,
    0x9108,
    0x8908,
    0x9108,
    0x9108,
    0x9108,
    0x8908,
    0x9108,
    0x9108,
    0x9128,
    0x9129,
    0x8929,
    0x9129,
    0x9129,
    0x9129,
    0x9129,
    0x8929,
    0x9529,
    0x9529,
    0x9529,
    0x954A,
    0x894A,
    0x954A,
    0x954A,
    0x954A,
    0x894A,
    0x954A,
    0x954A,
    0x954A,
    0x896B,
    0x954A,
    0x956B,
    0x956B,
    0x896B,
    0x956B,
    0x956B,
    0x996B,
    0x996B,
    0x8D8C,
    0x996B,
    0x996B,
    0x998C,
    0x8D8C,
    0x998C,
    0x998C,
    0x998C,
    0x8D8C,
    0x998C,
    0x998C,
    0x8DAD,
    0x998C,
    0x99AD,
    0x99AD,
    0x8DAD,
    0x99AD,
    0x99AD,
    0x8DCE,
    0x9DAD,
    0x9DAD,
    0x8DCE,
    0x9DAE,
    0x9DCE,
    0x8DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x8DEF,
    0x9DCE,
    0x9DEF,
    0x8DEF,
    0x9DEF,
    0x9DEF,
    0x9DEF,
    0x9DEF,
    0x9210,
    0xA210,
    0x9210,
    0x9231,
    0x9231,
    0xA231,
    0xA231,
    0x9252,
    0xA231,
    0x9252,
    0x9252,
    0x9E52,
    0x9273,
    0xA652,
    0x9673,
    0x9294,
    0x9694,
    0xAA73,
    0xAA94,
    0x9694,
    0xAA94,
    0x96B5,
    0xA2B5,
    0x96B5,
    0xAAB5,
    0x96D6,
    0xAEB5,
    0xA6D6,
    0x96D6,
    0xAED6,
    0x96F7,
    0xAEF7,
    0x9717,
    0xAEF7,
    0xAEF7,
    0xB2F7,
    0xB318,
    0x9B39,
    0xB318,
    0x9F39,
    0xB339,
    0xB739,
    0x9F5A,
    0xB75A,
    0x9F7B,
    0xB75A,
    0xA37B,
    0xB77A,
    0xB77B,
    0xA39C,
    0xBB9C,
    0xAB9C,
    0xBB9C,
    0xBBBC,
    0xBFBD,
    0xB3DE,
    0xBFDE,
    0xB3DE,
    0xC3FF,
    0xC3FF,
    0xCBFF,
    0xD3FF,
    0xDBFF,
    0xFFFF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_security_room_80182B18[256] = {
    0,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8020,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8421,
    0x8421,
    0x8421,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8441,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8461,
    0x8462,
    0x8462,
    0x8462,
    0x8862,
    0x8862,
    0x8862,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x8882,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88A2,
    0x88C2,
    0x88C2,
    0x88C2,
    0x88C2,
    0x88C3,
    0x8CC3,
    0x8CC3,
    0x8CC3,
    0x8CC3,
    0x8CC3,
    0x88C3,
    0x8CC3,
    0x8CC3,
    0x8CE3,
    0x8CE3,
    0x8CE3,
    0x88E2,
    0x8CE3,
    0x8CE3,
    0x8902,
    0x8CE3,
    0x8CE3,
    0x8CE3,
    0x8903,
    0x8CE4,
    0x90E4,
    0x9104,
    0x8923,
    0x9104,
    0x9104,
    0x9104,
    0x9104,
    0x8924,
    0x9104,
    0x9104,
    0x9104,
    0x9124,
    0x8942,
    0x9124,
    0x9124,
    0x9124,
    0x9143,
    0x9124,
    0x8D42,
    0x9525,
    0x9545,
    0x8D62,
    0x9545,
    0x9545,
    0x9545,
    0x9545,
    0x8D82,
    0x9545,
    0x9545,
    0x9545,
    0x9545,
    0x9565,
    0x8D83,
    0x9565,
    0x9565,
    0x9565,
    0x9565,
    0x91A3,
    0x9565,
    0x9565,
    0x9566,
    0x9966,
    0x9986,
    0x9986,
    0x91A3,
    0x9986,
    0x9986,
    0x9986,
    0x9986,
    0x91C3,
    0x9986,
    0x9986,
    0x99A6,
    0x99A6,
    0x91E3,
    0x99A6,
    0x99A7,
    0x91E4,
    0x9DA7,
    0x9DC7,
    0x9DC7,
    0x9DC7,
    0x9204,
    0x9DC7,
    0x9DC7,
    0x9DC7,
    0x9624,
    0x9DC7,
    0x9DE7,
    0x9DE7,
    0x9624,
    0x9DE7,
    0x9DE8,
    0xA1E8,
    0x9644,
    0xA1E8,
    0xA208,
    0xA208,
    0x9644,
    0xA208,
    0xA208,
    0xA208,
    0x9664,
    0xA228,
    0xA228,
    0x9A65,
    0xA228,
    0x9A85,
    0xA649,
    0x9A85,
    0x9AA5,
    0xA649,
    0x9AA5,
    0xAA6A,
    0x9AC5,
    0x9AC5,
    0xAA8A,
    0x9AE5,
    0xAA8A,
    0xAE8B,
    0x9F06,
    0x9F06,
    0xAEAB,
    0x9F26,
    0xAEEB,
    0xB2EC,
    0xB30C,
    0xA367,
    0xB70D,
    0xB32C,
    0xA788,
    0xB74D,
    0xBB2E,
    0xB76D,
    0xBB4E,
    0xB76D,
    0xAFCA,
    0xBB8E,
    0xC370,
    0xBB8E,
    0xB7CC,
    0xC390,
    0xBFAF,
    0xBFCF,
    0xC3B0,
    0xBFCF,
    0xBFCF,
    0xBFEF,
    0xC3F0,
    0xBFF0,
    0xC3F0,
    0xC7F1,
    0xC7F1,
    0xC7F1,
    0xCBF2,
    0xCFF3,
    0xCFF3,
    0xD7F4,
    0xFFFF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_security_room_80182D18[256] = {
    0,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8021,
    0x8421,
    0x8421,
    0x8422,
    0x8422,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8442,
    0x8443,
    0x8443,
    0x8443,
    0x8443,
    0x8463,
    0x8463,
    0x8863,
    0x8863,
    0x8863,
    0x8864,
    0x8864,
    0x8884,
    0x8884,
    0x8884,
    0x8884,
    0x8884,
    0x8884,
    0x8885,
    0x8885,
    0x8885,
    0x88A5,
    0x88A5,
    0x84A5,
    0x88A5,
    0x88A5,
    0x8CA5,
    0x8CA5,
    0x8CA6,
    0x8CA6,
    0x8CA6,
    0x8CA6,
    0x8CC6,
    0x8CC6,
    0x88C6,
    0x8CC6,
    0x8CC6,
    0x8CC6,
    0x84E7,
    0x8CE7,
    0x8CE7,
    0x90E7,
    0x90E7,
    0x90E7,
    0x8508,
    0x90E7,
    0x9107,
    0x9108,
    0x9108,
    0x9108,
    0x9108,
    0x8908,
    0x9108,
    0x9108,
    0x9108,
    0x8929,
    0x9129,
    0x9129,
    0x9129,
    0x9129,
    0x8929,
    0x9129,
    0x9529,
    0x9529,
    0x9529,
    0x894A,
    0x9529,
    0x954A,
    0x954A,
    0x954A,
    0x954A,
    0x894A,
    0x954A,
    0x954A,
    0x896B,
    0x954A,
    0x954A,
    0x956B,
    0x956B,
    0x996B,
    0x8D6B,
    0x996B,
    0x996B,
    0x996B,
    0x8D8C,
    0x996B,
    0x998C,
    0x998C,
    0x998C,
    0x998C,
    0x998C,
    0x918C,
    0x8DAC,
    0x998C,
    0x998C,
    0x8DAD,
    0x998C,
    0x99AD,
    0x99AD,
    0x8DAD,
    0x99AD,
    0x99AD,
    0x9DAD,
    0x9DAD,
    0x8DCE,
    0x9DAD,
    0x9DAE,
    0x9DCE,
    0x8DCE,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x91EF,
    0x9DCE,
    0x9DCE,
    0x9DCE,
    0x9DEE,
    0x9DEF,
    0x91EF,
    0x9DEF,
    0x9DEF,
    0xA1EF,
    0xA1EF,
    0xA1EF,
    0x9210,
    0xA20F,
    0xA210,
    0x9210,
    0xA210,
    0xA210,
    0x9211,
    0xA210,
    0xA210,
    0x9231,
    0xA210,
    0xA211,
    0xA231,
    0xA231,
    0x9231,
    0xA231,
    0xA631,
    0x9252,
    0xA651,
    0x9252,
    0xA652,
    0x9273,
    0xA673,
    0x9693,
    0x9694,
    0xAA73,
    0x9694,
    0x96B5,
    0xAA94,
    0x96B5,
    0xAE95,
    0x96D6,
    0xAAB5,
    0x96D6,
    0xAED6,
    0x96D6,
    0x96F7,
    0xAED6,
    0x9AF7,
    0x9B18,
    0xB2F7,
    0x9B39,
    0xB318,
    0xB739,
    0x9F5A,
    0xB739,
    0x9F5A,
    0xB75A,
    0xB75A,
    0x9F7B,
    0xA39C,
    0xBB7B,
    0xBB9C,
    0xA7BD,
    0xBB9C,
    0xBFBD,
    0xC39D,
    0xC3BD,
    0xBFBD,
    0xBFDE,
    0xB3DE,
    0xC7DE,
    0xBFDE,
    0xBFFF,
    0xCBFF,
    0xC3FF,
    0xC7FF,
    0xCBFF,
    0xCFFF,
    0xDBFF,
    0xEFFF,
    0xFFFF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

u16 D_acropolis_security_room_80182F18[256] = {
    0,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8001,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8002,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8004,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8003,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8005,
    0x8004,
    0x8004,
    0x8004,
    0x8004,
    0x8006,
    0x8005,
    0x8005,
    0x8005,
    0x8005,
    0x8005,
    0x8006,
    0x8005,
    0x8005,
    0x8005,
    0x8007,
    0x8006,
    0x8006,
    0x8006,
    0x8007,
    0x8006,
    0x8006,
    0x8006,
    0x8006,
    0x8006,
    0x8007,
    0x8008,
    0x8006,
    0x8007,
    0x8008,
    0x8007,
    0x8007,
    0x8007,
    0x8009,
    0x8007,
    0x8007,
    0x8008,
    0x8009,
    0x8008,
    0x8008,
    0x8008,
    0x8008,
    0x800A,
    0x8008,
    0x8008,
    0x8428,
    0x8009,
    0x8009,
    0x800B,
    0x8009,
    0x8009,
    0x800B,
    0x8029,
    0x8029,
    0x800A,
    0x8029,
    0x800C,
    0x840A,
    0x802A,
    0x802A,
    0x8429,
    0x800C,
    0x802A,
    0x800A,
    0x800D,
    0x842A,
    0x800B,
    0x802A,
    0x800D,
    0x802B,
    0x802B,
    0x800B,
    0x842B,
    0x802B,
    0x800E,
    0x842B,
    0x802B,
    0x802C,
    0x842C,
    0x800F,
    0x842C,
    0x842C,
    0x800D,
    0x842C,
    0x800F,
    0x842C,
    0x842C,
    0x842C,
    0x800F,
    0x842D,
    0x842D,
    0x842D,
    0x8010,
    0x842D,
    0x842D,
    0x8010,
    0x842D,
    0x842E,
    0x8010,
    0x842E,
    0x8011,
    0x842E,
    0x842F,
    0x8012,
    0x8012,
    0x844E,
    0x8013,
    0x842F,
    0x8013,
    0x8430,
    0x8014,
    0x8014,
    0x8450,
    0x8431,
    0x884F,
    0x8015,
    0x8013,
    0x8432,
    0x8016,
    0x8014,
    0x8851,
    0x8016,
    0x8433,
    0x8453,
    0x8017,
    0x8017,
    0x8434,
    0x8853,
    0x8435,
    0x8018,
    0x8854,
    0x8019,
    0x8436,
    0x8855,
    0x8436,
    0x801A,
    0x8019,
    0x8457,
    0x8439,
    0x8875,
    0x8856,
    0x8458,
    0x8C76,
    0x8858,
    0x8C77,
    0x843A,
    0x8859,
    0x843B,
    0x8C78,
    0x845B,
    0x843D,
    0x885B,
    0x8879,
    0x885C,
    0x843E,
    0x885D,
    0x883F,
    0x8C7B,
    0x885E,
    0x8C7C,
    0x8C7C,
    0x885F,
    0x8C9B,
    0x885E,
    0x885F,
    0x8C7F,
    0x909D,
    0x8C7F,
    0x909E,
    0x8C7F,
    0x909F,
    0x909F,
    0x90BF,
    0x98DF,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
    0x8000,
};

AcropolisSecurityRoomPalette D_acropolis_security_room_80183118 = { 0 };

AcropolisSecurityRoomPalette D_acropolis_security_room_80183318 = { 0 };

AcropolisSecurityRoomPalette D_acropolis_security_room_80183518 = { 0 };

AcropolisSecurityRoomPalette D_acropolis_security_room_80183718 = { 0 };

GpuImageUpload D_acropolis_security_room_80183918[5] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 270, 256, 1 }, D_acropolis_security_room_80183118.words },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 271, 256, 1 }, D_acropolis_security_room_80183318.words },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 263, 256, 1 }, D_acropolis_security_room_80183518.words },
    { GPU_IMAGE_UPLOAD_COPY, 0, { 0, 264, 256, 1 }, D_acropolis_security_room_80183718.words },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

u16 D_acropolis_security_room_80183968[4] = {
    5,
    6,
    9,
    10,
};

AsrSpriteFrame D_acropolis_security_room_80183970[4] = {
    { 270, 0, { 0 }, 0, { 0 }, 0xFFA7, 0xFFF2 },
    { 271, 0, { 0 }, 128, { 0 }, 0xFFEB, 0xFFF3 },
    { 263, 128, { 0 }, 0, { 0 }, 29, 0xFFF3 },
    { 264, 128, { 0 }, 128, { 0 }, 100, 0xFFF1 },
};

SVECTOR D_acropolis_security_room_80183998[4] = {
    { 720, -2450, 420, 0 },
    { 720, -2450, 280, 0 },
    { 720, -2450, 350, 0 },
    { 720, -2450, 210, 0 },
};

s16 D_acropolis_security_room_801839B8[4] = {
    3,
    1,
    3,
    2,
};

EffectUnitQuadCorner D_acropolis_security_room_801839C0[4] = {
    { -1, 1 },
    { 1, 1 },
    { -1, -1 },
    { 1, -1 },
};

WorldCollisionRoomResources D_acropolis_security_room_801839D0[1] = {
    { D_acropolis_security_room_80183D94, D_acropolis_security_room_80183DB8, D_acropolis_security_room_80183EE8, NULL },
};

u8* D_acropolis_security_room_801839E0[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_security_room_801839E4[1] = { 16 };

WorldCoordRoomLighting D_acropolis_security_room_801839E8[1] = {
    { D_acropolis_security_room_801841C8, NULL },
};

DirectionWarpEntry D_acropolis_security_room_801839F0[1] = {
    { { { .word = 0 }, -46, -935, -2877 }, { 0, 0, 0, 0 }, { { .word = 0 }, -46, -935, -2877 }, { 0, 0, 0, 0 }, 0x51060005, 0x51060004, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 498 },
};

static SVECTOR _gAcropolisSecurityRoomCollision067D4Normals[11] = {
#include "assets/acropolis_security_room_collision_067D4_normals.inc"
};

static SVECTOR _gAcropolisSecurityRoomCollision067D4Verts[50] = {
#include "assets/acropolis_security_room_collision_067D4_verts.inc"
};

static WorldCollisionGridFace _gAcropolisSecurityRoomCollision067D4Faces[24] = {
#include "assets/acropolis_security_room_collision_067D4_faces.inc"
};

static s16 _gAcropolisSecurityRoomCollision067D4Cells[46] = {
#include "assets/acropolis_security_room_collision_067D4_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisSecurityRoomCollision067D4Cells[i])
static s16* _gAcropolisSecurityRoomCollision067D4Table[2] = {
#include "assets/acropolis_security_room_collision_067D4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_security_room_80183D94[1] = {
    { NULL, _gAcropolisSecurityRoomCollision067D4Normals, _gAcropolisSecurityRoomCollision067D4Verts, _gAcropolisSecurityRoomCollision067D4Faces, _gAcropolisSecurityRoomCollision067D4Table, 1250, 3250, 1, 2, 4000, 24 },
};

WorldCollisionTrigger D_acropolis_security_room_80183DB8[4] = {
    { NULL, NULL, NULL, { -64, -2080, -513, 0 }, { { -1472, -1664, 0, 0 }, { 1472, -1664, 0, 0 }, { -1472, 1664, 0, 0 }, { 1472, 1664, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 0, 0 }, 2217, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2, -2016, 1215, 0 }, { { -1471, -1664, 0, 0 }, { 1472, -1664, 0, 0 }, { -1471, 1664, 0, 0 }, { 1472, 1664, 0, 0 } }, { 0, 0, -4104, 0 }, { 0, 0, 0, 0 }, 2217, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1, -2176, 1087, 0 }, { { -1473, 1664, 0, 0 }, { 1472, 1664, 1, 0 }, { -1473, -1664, 0, 0 }, { 1472, -1664, 1, 0 } }, { -2, 0, 4103, 0 }, { 0, 0, 0, 0 }, 2217, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0, -2208, -1024, 0 }, { { -1472, 1664, 0, 0 }, { 1472, 1664, 0, 0 }, { -1472, -1664, 0, 0 }, { 1472, -1664, 0, 0 } }, { 0, 0, 4103, 0 }, { 0, 0, 0, 0 }, 2217, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_security_room_80183EE8[5] = {
    { NULL, NULL, NULL, { 128, -1024, -2896, 0 }, { { -544, 0, -240, 0 }, { 544, 0, -240, 0 }, { -544, 0, 240, 0 }, { 544, 0, 240, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 593, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1088, -1088, -704, 0 }, { { -464, 0, -752, 0 }, { 464, 0, -752, 0 }, { -464, 0, 752, 0 }, { 464, 0, 752, 0 } }, { 0, 4111, 0, 0 }, { 4096, 0, 0, 0 }, 882, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 192, -1088, 144, 0 }, { { -272, 0, -416, 0 }, { 272, 0, -416, 0 }, { -272, 0, 416, 0 }, { 272, 0, 416, 0 } }, { 0, 4095, 0, 0 }, { -4097, 0, -27, 0 }, 496, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -448, -1056, 1568, 0 }, { { -784, 0, -272, 0 }, { 784, 0, -272, 0 }, { -784, 0, 272, 0 }, { 784, 0, 272, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 829, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -880, -1056, -2880, 0 }, { { -432, 0, -304, 0 }, { 432, 0, -304, 0 }, { -432, 0, 304, 0 }, { 432, 0, 304, 0 } }, { 0, 4096, 0, 0 }, { 1189, 0, 3920, 0 }, 527, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_security_room_80184064[3] = {
    { 102, 119, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_8016EC0C },
    { 110, 119, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, D_8016EC00 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_security_room_80184088[4] = {
    { NULL, NULL },
    { D_map_akropolis_8017B01C, D_acropolis_security_room_80184064 },
    { NULL, NULL },
    { NULL, NULL },
};

/// The security room's three white point lights, contributing in every view.
///
/// Positions and falloff radii use integer world units; RGB intensity uses
/// 12 fractional bits (`ONE` is full strength). Each light is full strength
/// within 1280 units and fades to zero at 2128 units. The loaded room overlay
/// owns these writable records: coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not outlive the overlay.
static WorldCoordPointLight _gAcropolisSecurityRoomPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2267, -2365 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1280,
        .outer = 2128,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2267, -472 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1280,
        .outer = 2128,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, -2993, 1623 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 1280,
        .outer = 2128,
    },
};

WorldCoordRoomLights D_acropolis_security_room_801841C8[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisSecurityRoomPointLights), _gAcropolisSecurityRoomPointLights, 0, NULL },
};

SpriteBatch D_acropolis_security_room_801841E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_801841F0[18] = {
    { 143, 0x3FC0, { .fields = { 40, 96 } }, -160, -112, 550, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, -16, 550, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, 64, 550, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -120, -112, 575, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -120, -16, 575, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -120, 64, 575, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -104, 600, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -48, 600, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, 8, 600, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, 64, 600, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -104, 612, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -48, 612, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, 8, 612, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 64, 612, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -64, 625, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -64, -8, 625, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 48, 625, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -56, 40, 625, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184358[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_80184370[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_80184380[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_80184390[10] = {
    { 143, 0x3FC0, { .fields = { 56, 240 } }, -160, -120, 129, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 240 } }, 104, -120, 129, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -104, -120, 129, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -48, -120, 129, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 8, -120, 129, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 56, -120, 129, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 64 } }, -104, 56, 129, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 64 } }, -48, 56, 129, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, 8, 56, 129, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, 56, 56, 129, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184458[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_80184470[2] = {
    { 143, 0x3FC0, { .fields = { 120, 120 } }, -128, 0, 50, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 112, 120 } }, 8, 0, 50, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184498[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_801844B8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_security_room_801844C8[16] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_security_room_80184548[84] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 375, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 375, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 80, 375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 375, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 375, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, 64, 375, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 56, 375, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 56, 375, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 64, 96, 375, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 375, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 72, 375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, 80, 375, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 375, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 375, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -96, 375, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, -16, 375, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 56, 375, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -96, 375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -48, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 24, -120, 375, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 56, -120, 375, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 88, -120, 375, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 128, -120, 375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 0, 375, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -120, -120, 375, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, -80, 681, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -160, -48, 1883, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 24, 1475, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 32, 1425, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 1415, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, 32, 1312, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 72, 1312, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -40, 32, 1312, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -40, 72, 1312, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 32, 1400, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, 72, 1400, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 32, 1350, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 72, 1350, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 32, 72, 1312, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -8, 72, 1312, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -8, 32, 1312, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 32, 32, 1312, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 16, 1500, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -80, 1500, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -48, -80, 1500, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -56, 1500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -16, 1500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 1500, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -72, 24, 1500, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, -24, 1875, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -16, -16, 1875, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -16, 1875, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -16, -48, 1875, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 1875, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 24, 1875, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1875, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, -104, 1212, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, -104, 1212, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 40, 1212, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 128, 64, 1212, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 112, 32, 1212, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 0, 1212, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -40, 1212, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -80, 1212, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 88, -112, 1212, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 24, 1625, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 96, -8, 1625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -40, 1625, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 88, -72, 1625, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 375, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 88, 104, 375, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, 48, 104, 375, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -32, 104, 375, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 8, 104, 375, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, 104, 375, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 375, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -144, 88, 375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 375, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -72, -104, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 2212, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 2162, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 2100, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_security_room_80184BD8[15] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 5, 0 } },
    { 8, 5, 0, 0, { 7, 0 } },
    { 13, 7, 0, 0, { 1, 0 } },
    { 20, 4, 0, 0, { 8, 0 } },
    { 24, 5, 0, 0, { 0, 0 } },
    { 29, 15, 0, 0, { 9, 0 } },
    { 44, 7, 0, 0, { 6, 0 } },
    { 51, 7, 0, 0, { 10, 0 } },
    { 58, 9, 0, 0, { 3, 0 } },
    { 67, 4, 0, 0, { 11, 0 } },
    { 71, 9, 0, 0, { 2, 0 } },
    { 80, 1, 0, 0, { 12, 0 } },
    { 81, 3, 0, 0, { 4, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_security_room_80184C50[16] = {
    { { .empty = D_acropolis_security_room_801841E0 }, D_acropolis_security_room_801841E0, NULL },
    { { .elements = D_acropolis_security_room_801841F0 }, D_acropolis_security_room_80184358, NULL },
    { { .empty = D_acropolis_security_room_80184370 }, D_acropolis_security_room_80184370, NULL },
    { { .empty = D_acropolis_security_room_80184380 }, D_acropolis_security_room_80184380, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184470 }, D_acropolis_security_room_80184498, NULL },
    { { .empty = D_acropolis_security_room_801844B8 }, D_acropolis_security_room_801844B8, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .elements = D_acropolis_security_room_80184390 }, D_acropolis_security_room_80184458, NULL },
    { { .empty = D_acropolis_security_room_80184380 }, D_acropolis_security_room_80184380, NULL },
    { { .empty = D_acropolis_security_room_80184380 }, D_acropolis_security_room_80184380, NULL },
    { { .elements = D_acropolis_security_room_80184548 }, D_acropolis_security_room_80184BD8, NULL },
};

ViewCamera D_acropolis_security_room_80184D10[16] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 911 },
    { { { { -3929, 0, -1154 }, { -300, 3954, 1023 }, { 1114, 1067, -3794 } }, { 670, 2800, -1160 } }, 207 },
    { { { { 3895, 0, -1265 }, { -445, 3833, -1371 }, { 1184, 1441, 3646 } }, { 780, 3090, 2600 } }, 207 },
    { { { { 3698, 0, -1759 }, { -1442, 2345, -3032 }, { 1007, 3357, 2118 } }, { 770, 3450, -750 } }, 207 },
    { { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 780, 2260, -1930 } }, 207 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { -490, 2430, -320 } }, 207 },
    { { { { 0, 0, 4096 }, { 0, 4096, 0 }, { -4096, 0, 0 } }, { 910, 2500, 880 } }, 207 },
    { { { { 505, 0, 4064 }, { 822, 4011, -102 }, { -3980, 829, 495 } }, { -7580, 2600, 290 } }, 230 },
    { { { { -4073, 0, 425 }, { 115, 3943, 1101 }, { -409, 1107, -3922 } }, { -4735, 2711, -4651 } }, 257 },
    { { { { -1392, 0, 3852 }, { 1053, 3939, 380 }, { -3705, 1120, -1339 } }, { -1390, 2540, 250 } }, 297 },
    { { { { -3750, 0, 1646 }, { 1161, 2905, 2643 }, { -1168, 2887, -2659 } }, { 3260, 3610, 2030 } }, 230 },
    { { { { -169, 0, 4092 }, { 1090, 3948, 45 }, { -3944, 1091, -163 } }, { -5964, 2300, 6357 } }, 230 },
    { { { { -1332, 0, -3873 }, { -1542, 3757, 530 }, { 3552, 1631, -1222 } }, { -2580, 5310, -0x3660 } }, 230 },
    { { { { -3, 0, -4095 }, { 199, 4091, 0 }, { 4091, -199, -3 } }, { -6100, 1240, 5740 } }, 235 },
    { { { { 3815, 0, 1489 }, { 981, 3080, -2514 }, { -1120, 2699, 2869 } }, { -920, 3010, 0x48A8 } }, 230 },
    { { { { 3982, 0, 958 }, { -16, 4095, 69 }, { -958, -71, 3981 } }, { -1500, 1380, 7850 } }, 225 },
};

GpAreaApplyRec D_acropolis_security_room_80184F50[10] = {
    { 1, 8, 2, 17 },
    { 1, 8, 7, 33 },
    { 1, 9, 4, 16 },
    { 1, 9, 8, 32 },
    { 1, 10, 3, 0 },
    { 1, 11, 3, 17 },
    { 1, 11, 8, 33 },
    { 1, 13, 2, 17 },
    { 1, 13, 8, 33 },
    { 255, 0, 0, 0 },
};

GpAreaApplyRec D_acropolis_security_room_80184F78[1] = {
    { 255, 0, 0, 0 },
};

GpAreaApplyRec D_acropolis_security_room_80184F7C[1] = {
    { 255, 0, 0, 0 },
};

GpAreaApplyRec D_acropolis_security_room_80184F80[1] = {
    { 255, 0, 0, 0 },
};

WorldCollisionFootstepSounds D_acropolis_security_room_80184F84 = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

WorldCollisionSurfaceProperties D_acropolis_security_room_80184F90[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_security_room_80184F98[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_security_room_80184F84 },
};

WorldCollisionSurfaceProperties* D_acropolis_security_room_80184FA0[8] = {
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F98,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
    D_acropolis_security_room_80184F90,
};

static TmdBone _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Skeleton[3] = {
#include "assets/acropolis_sanctuary_model_090F0_skeleton.inc"
};

static u32 _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0PartVerts[3] = {
#include "assets/acropolis_sanctuary_model_090F0_partVerts.inc"
};

static SVECTOR _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Verts[56] = {
#include "assets/acropolis_sanctuary_model_090F0_verts.inc"
};

static SVECTOR _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Normals[6] = {
#include "assets/acropolis_sanctuary_model_090F0_normals.inc"
};

static u32 _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Stream[215] = {
#include "assets/acropolis_sanctuary_model_090F0_stream.inc"
};

TmdSource gAcropolisSecurityRoomAcropolisSanctuaryModel090F0 = {
    0,
    1768,
    0,
    3,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0PartVerts,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Verts,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Normals,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Skeleton,
    _gAcropolisSecurityRoomAcropolisSanctuaryModel090F0Stream,
};

Task* D_acropolis_security_room_801855A8;

Task* D_acropolis_security_room_801855AC;

SVECTOR ActorContact_ScratchPosition;

static void func_acropolis_security_room_8017DE80(RoomRect* rect, u8 r, u8 g, u8 b);
static void func_acropolis_security_room_8017F1BC(Task* task);
static void func_acropolis_security_room_8017F300(Task* task);
static void func_acropolis_security_room_80182574(Task* task);

/// Message 0x13EE handler: copies the incoming location record onto the
/// outgoing one and answers 1.
s32 func_acropolis_security_room_8017D6AC(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

/// Message 0x13F1 handler: forwards the message unchanged to the task
/// `func_acropolis_security_room_8017D834` spawns and keeps in
/// `D_acropolis_security_room_801855AC`, answering 0 while it is not alive.
s32 func_acropolis_security_room_8017D6D4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    Task* target;
    s32   ret;

    target = D_acropolis_security_room_801855AC;
    if (target == NULL) {
        ret = 0;
    } else {
        ret = taskMessageDispatch(target, msgId, arg2, arg3);
    }
    return ret;
}

s32 func_acropolis_security_room_8017D708(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Task_SpawnFromTable(D_acropolis_security_room_80182618, 1, 0, 0);
    }
    return 0;
}

void func_acropolis_security_room_8017D740(Task* arg0, s32 arg1, DirectionActionRequest* request)
{
    if (request->actionId == 0) {
        Task_SpawnFromTable(D_acropolis_security_room_80182618, 0, 0, 0);
    }
}
/// State table of the room's message task: register the room's message table,
/// idle, then kill the task.
static const TaskFuncTable3 D_acropolis_security_room_8017D5C4 = { {
    func_acropolis_security_room_8017D930,
    func_acropolis_security_room_8017D97C,
    taskKill,
} };

void func_acropolis_security_room_8017D77C(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf("monitor\n");
            D_acropolis_security_room_801855A8 = Task_Spawn(2, 9, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855A8, &sp10) != 0) {
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                taskKill(arg0);
            }
            return;
    }
}

/// The debug line `func_acropolis_security_room_8017D834` prints. The two bytes
/// after its terminator are non-zero in the ROM (0x40, 0x11), so the array is
/// declared at 16 bytes to carry them.
static const char PowerSupplyMsg[16] = "power supply\n\0@\021";

void func_acropolis_security_room_8017D834(Task* arg0)
{
    s32 sp10;
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            printf(PowerSupplyMsg);
            D_acropolis_security_room_801855AC = Task_Spawn(2, 0xA, 0, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(2);
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Task_PollKill(D_acropolis_security_room_801855AC, &sp10) != 0) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
                D_acropolis_security_room_801855AC = NULL;
                taskKill(arg0);
            }
            return;
    }
}

static void func_acropolis_security_room_8017D930(Task* arg0)
{
    arg0->msgTable = D_acropolis_security_room_801825DC;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state                        = arg0->state + 1;
    D_acropolis_security_room_801855AC = NULL;
}

static void func_acropolis_security_room_8017D97C(Task* task)
{
}

/// States of the security-monitor task, dispatched by
/// `func_acropolis_security_room_8017ED68`: set up the work block, run the
/// camera list, redraw the panel, confirm a camera, and leave the monitor.
static const TaskFuncTable7 D_acropolis_security_room_8017D5EC = { {
    func_acropolis_security_room_8017D9DC,
    func_acropolis_security_room_8017EA28,
    func_acropolis_security_room_8017DB30,
    func_acropolis_security_room_8017EA5C,
    func_acropolis_security_room_8017DC7C,
    func_acropolis_security_room_8017EADC,
    func_acropolis_security_room_8017EB9C,
} };

/// Runs the room's message task's current state through a stack copy of its
/// three-entry state table.
void func_acropolis_security_room_8017D984(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_security_room_8017D5C4;
    sp.funcs[task->state](task);
}

/// Entry state of the security-monitor task: allocates the `AsrMonitorWork`
/// block into the `Task::work` slot, spawns the monitor's companion task,
/// seeds `cameraId` from the `GameFlag_GetNibble(0x2A)` camera table, and picks
/// the next state from the `GameFlag_GetNibble(1)` progress nibble (state+1 and
/// prompt kind 8 before chapter 3, state 6 and prompt kind 5 after). Finally it
/// clears every hotspot's `hit` flag so the first hit test starts clean.
static void func_acropolis_security_room_8017D9DC(Task* task)
{
    AsrMonitorWork* work;
    OverlayHotspot* hs;
    s16             flag;
    s32             state;
    s16             stateElse;

    work = memCalloc(sizeof(AsrMonitorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = Task_SpawnFromTable(&D_acropolis_security_room_8018263C, 0, 1, 0);
    task->work              = work;
    work->blinkTimer        = 0;
    stateElse               = 6;
    flag                    = GameFlag_GetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA);
    if ((u16)flag < 5) {
        work->cameraId = D_acropolis_security_room_801826B4[flag];
    } else {
        work->cameraId = D_acropolis_security_room_801826B4[0];
    }
    if (GameFlag_GetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) < 3) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 8;
        /* Without this the scheduler hoists the `task->state` load above the
           `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` byte store to fill its load-delay slot. */
        state = task->state;
        state++;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 5;
        state                                                      = stateElse;
    }
    task->state = state;
    Display_AcquireRef();
    gGameSession->hideHud      = 1;
    gGameSession->cutsceneHold = 1;
    gGameSession->eventState   = 1;
    hs                         = D_acropolis_security_room_80182648;
    if (hs->id != -1) {
        do {
            hs->hit = 0;
            hs++;
        } while (hs->id != -1);
    }
}

/// Runs the hotspot-hit state of the security monitor: redraws the panel and
/// cursor, then hit-tests the action cursor against the room's hotspot table.
/// A miss leaves the prompt highlighted (`mode` 1); a hit with the prompt
/// confirmed (`buttons[0].state` 2) scans the table for the raised entry and hands its
/// `id` / `promptKind` to the work block, advancing to state 3. Otherwise the
/// task advances to state 5 once the prompt has been dismissed.
static void func_acropolis_security_room_8017DB30(Task* task)
{
    AsrMonitorWork*   work;
    OverlayHotspot*   hs;
    RoomActionPrompt* prompt;

    hs     = D_acropolis_security_room_80182648;
    prompt = D_80114D28;
    work   = (AsrMonitorWork*)task->work;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if ((prompt->buttons.slots[0].state == 2) && (hs->id != -1)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->selection  = hs->id;
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

/// Runs the camera-list state of the security monitor: mirrors the highlighted
/// row into `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` (with a click), scrolls the panel by a page
/// when the row is one of the two 0x8000/0x8001 scroll commands, and fires the
/// two one-shot cap sequences the room gates on the `0xA` game-flag nibble.
/// Then redraws the panel plus cursor overlay and advances to state 2.
static void func_acropolis_security_room_8017DC7C(Task* task)
{
    AsrMonitorWork* work;
    McSaveData*     save;
    s32             sfx;
    s16             sel;
    u16             usel;

    work                   = (AsrMonitorWork*)task->work;
    D_80114D28[0].mode     = 0;
    D_80114D28[0].targetId = 0;
    if (func_800D4EC0() != 0) {
        sel = work->selection;
        if (sel >= 0) {
            save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.location.loc.view != sel) {
                save->state.location.loc.view = work->selection;
                SndEvt_EnqueueType6(SOUND_ACROPOLIS_SECURITY_ROOM_MONITOR_SELECT, 0, 0);
                if ((work->selection == 0xA) && !(GameFlag_GetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) & 2)) {
                    work->field_7 = 1;
                }
            }
        }
        usel = work->selection;
        if (usel == 0x8000) {
            if (((s16)work->cameraId + 0x3E) < 0xFE) {
                work->cameraId += 0x3E;
                sfx             = 0x51060006;
                goto play;
            }
        } else if (usel == 0x8001) {
            if (((s16)work->cameraId - 0x3E) > 0) {
                work->cameraId -= 0x3E;
                sfx             = 0x51060007;
            play:
                SndEvt_EnqueueType6(sfx, 0, 0);
            }
        }
        if (((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 0xB) && ((s16)work->cameraId != 4) && !(GameFlag_GetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) & 1)) {
            Gp_StartCapSlot(0xD, 0, 0);
            GameFlag_SetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN, GameFlag_GetNibble(GAME_FLAG_SECURITY_MONITOR_SCENES_SEEN) | 1);
        }
        if (((u8)gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 0xA) && ((s16)work->cameraId != 4) && (work->field_7 != 0) &&
            (work->field_8 == 0) && (GameFlag_GetNibble(GAME_FLAG_SECURITY_MONITOR_CAM_A_SCENE_DONE) == 0)) {
            Gp_StartCapSlot(0xC, 0, 0);
            work->field_8 = 1;
        }
    }
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    task->state = 2;
}

/// Outlines `rect` on screen in the colour (`r`, `g`, `b`) with four
/// unconnected flat lines -- top, right, bottom and left edge of the rectangle
/// spanning (`x`, `y`) to (`x + w`, `y + h`) -- each linked into
/// `gGpuCurrentOt[3]`. Nothing in the overlay calls it; it is the debug box
/// drawer for the hotspot rectangles.
static void func_acropolis_security_room_8017DE80(RoomRect* rect, u8 r, u8 g, u8 b)
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
    addPrim(gGpuCurrentOt + 3, line);

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
    addPrim(gGpuCurrentOt + 3, line);

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
    addPrim(gGpuCurrentOt + 3, line);

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
    addPrim(gGpuCurrentOt + 3, line);
}

/// Washes the security-monitor panel with the grey level `id` -- the work
/// block's `cameraId` biased by -0x7F -- as a semi-transparent `POLY_F4`
/// covering (-0x66, -0x5F) to (0x6C, 0x3C) in `gGpuCurrentOt[0xC]`, followed by
/// the drawing-mode packet that restores the panel's texture page. A negative
/// `id` selects `GPU_BLEND_SUBTRACT`, darkening the "no signal" panel.
/// The strip below the panel (y 0x3C to 0x38)
/// is then blacked out with an opaque quad in `gGpuCurrentOt[0xB]`.
static void func_acropolis_security_room_8017E0C4(s16 id)
{
    POLY_F4* poly;
    DR_MODE* dr;
    u16      c;

    if (id >= 0) {
        c              = id & 0x7F;
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 5);
        setcode(poly, 0x2A);
        poly->r0 = c;
        poly->g0 = c;
        poly->b0 = c;
        poly->x0 = -0x66;
        poly->y0 = -0x5F;
        poly->x1 = 0x6C;
        poly->y1 = -0x5F;
        poly->x2 = -0x66;
        poly->y2 = 0x3C;
        poly->x3 = 0x6C;
        poly->y3 = 0x3C;
        addPrim(gGpuCurrentOt + 0xC, poly);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setlen(dr, 1);
        dr->code[0] = 0xE100002A;
        addPrim(gGpuCurrentOt + 0xC, dr);
    } else {
        c              = (~id + 1) & 0xFF;
        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        setlen(poly, 5);
        setcode(poly, 0x2A);
        poly->r0 = c;
        poly->g0 = c;
        poly->b0 = c;
        poly->x0 = -0x66;
        poly->y0 = -0x5F;
        poly->x1 = 0x6C;
        poly->y1 = -0x5F;
        poly->x2 = -0x66;
        poly->y2 = 0x3C;
        poly->x3 = 0x6C;
        poly->y3 = 0x3C;
        addPrim(gGpuCurrentOt + 0xC, poly);

        dr             = gGpuPrimCursor;
        gGpuPrimCursor = dr + 1;
        setlen(dr, 1);
        dr->code[0] = _get_mode(false, false, getTPage(0, GPU_BLEND_SUBTRACT, 640, 0));
        addPrim(gGpuCurrentOt + 0xC, dr);
    }

    poly           = gGpuPrimCursor;
    gGpuPrimCursor = poly + 1;
    setlen(poly, 5);
    setcode(poly, 0x28);
    poly->r0 = 0;
    poly->g0 = 0;
    poly->b0 = 0;
    poly->x0 = -0x66;
    poly->y0 = 0x3C;
    poly->x1 = 0x6C;
    poly->y1 = 0x3C;
    poly->x2 = -0x66;
    poly->y2 = 0x38;
    poly->x3 = 0x6C;
    poly->y3 = 0x38;
    addPrim(gGpuCurrentOt + 0xB, poly);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100000A;
    addPrim(gGpuCurrentOt + 0xB, dr);
}

/// Draws the blinking cursor overlay on top of the monitor panel: a 0x6C-wide
/// grey `TILE` whose top edge and height both track `AsrMonitorWork::blinkTimer`,
/// followed by the drawing-mode packet that restores the panel's texture page.
/// The timer wraps at 0x97, which is what makes the bar sweep and restart.
static void func_acropolis_security_room_8017E37C(Task* task)
{
    AsrMonitorWork* work;
    TILE*           tile;
    DR_MODE*        dr;
    s16             y;

    tile           = gGpuPrimCursor;
    work           = (AsrMonitorWork*)task->work;
    gGpuPrimCursor = tile + 1;
    setlen(tile, 3);
    setcode(tile, 0x42);
    tile->r0 = 0x60;
    tile->g0 = 0x60;
    tile->b0 = 0x60;
    tile->x0 = -0x66;
    tile->w  = 0x6C;
    y        = work->blinkTimer - 0x5F;
    tile->h  = y;
    tile->y0 = y;
    addPrim(gGpuCurrentOt + 0xE, tile);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE100000A;
    addPrim(gGpuCurrentOt + 0xE, dr);
    work->blinkTimer++;
    if (work->blinkTimer >= 0x97) {
        work->blinkTimer = 0;
    }
}

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Two-state dispatcher whose handler table is built on the stack rather than
/// read from `.data`: state 0 runs `actionPromptReset` and
/// state 1 runs `actionPromptMoveCursors`.
void func_acropolis_security_room_8017E9D8(Task* task)
{
    TaskFunc funcs[2] = {
        actionPromptReset,
        actionPromptMoveCursors,
    };

    funcs[task->state](task);
}

/// Arms the action prompt for the monitor's hotspot and steps the caller on one
/// state: highlights (`mode` 1) the fixed target id 0x80 and clears the
/// prompt's on-screen position, which `func_800D4E78` fills in again when the
/// prompt is actually spawned. The room carries a second copy of this body at
/// `func_acropolis_security_room_8017FB20`.
static void func_acropolis_security_room_8017EA28(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Confirms the camera the player picked on the security monitor: clears the
/// action prompt, redraws the panel for the selected camera plus its cursor
/// overlay, spawns the prompt at the panel's coordinates and advances the task.
static void func_acropolis_security_room_8017EA5C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    AsrMonitorWork*   work   = (AsrMonitorWork*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_acropolis_security_room_8017E0C4(work->cameraId - 0x7F);
    func_acropolis_security_room_8017E37C(task);
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Leaves the security monitor: records which camera was on screen as the
/// `0x2A` nibble (index into `D_acropolis_security_room_801826B4`, 0 if the id
/// is not in the table), restores the room's normal display state and kills the
/// monitor task along with the child task it spawned.
static void func_acropolis_security_room_8017EADC(Task* task)
{
    AsrMonitorWork* work;
    s16*            camera;
    s32             index;
    s32             cameraId;

    index      = 0;
    camera     = D_acropolis_security_room_801826B4;
    work       = (AsrMonitorWork*)task->work;
    D_80114D08 = 0xA;
    cameraId   = (s16)work->cameraId;
loop:
    if (cameraId != *camera) {
        index  += 1;
        camera += 1;
        if (index >= 5) {
            GameFlag_SetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA, 0);
            goto done;
        }
        goto loop;
    }
    GameFlag_SetNibble(GAME_FLAG_SECURITY_MONITOR_LAST_CAMERA, index);
done:
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 4;
    Display_ReleaseRef();
    gGameSession->cutsceneHold = 0;
    gGameSession->hideHud      = 0;
    gGameSession->eventState   = 0;
    taskKill(task->spawnArg2.pointer);
    Task_RequestKill(task, 0);
}

/// Idle state of the security monitor: hit-tests the action cursor against the
/// monitor's hotspot table and mirrors the result into the room's action
/// prompt. A hit that the player confirms (`buttons[0].state == 2`) on a raised hotspot
/// clears the prompt and runs cap command 0xE; `buttons[1].state == 2` leaves the
/// monitor by advancing to state 5.
static void func_acropolis_security_room_8017EB9C(Task* task)
{
    RoomActionPrompt* prompt  = D_80114D28;
    OverlayHotspot*   hotspot = D_acropolis_security_room_80182648;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (actionPromptHitTest(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons.slots[0].state == 2) {
            for (; hotspot->id != -1; hotspot++) {
                if (hotspot->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    Gp_RunCapCmd(0xE, 0);
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

#include "../../shared/action_prompt_hit_test.inc.c"

/// The sixteen state handlers of the room's cap script.
static const TaskFuncTable16 D_acropolis_security_room_8017D63C = { {
    func_acropolis_security_room_8017FA18,
    func_acropolis_security_room_8017FB20,
    func_acropolis_security_room_8017EE44,
    func_acropolis_security_room_8017FB54,
    func_acropolis_security_room_8017FBA4,
    func_acropolis_security_room_8017FC30,
    func_acropolis_security_room_801800A4,
    func_acropolis_security_room_8018014C,
    func_acropolis_security_room_801801C4,
    func_acropolis_security_room_80180218,
    func_acropolis_security_room_8017FE6C,
    func_acropolis_security_room_8017FF0C,
    func_acropolis_security_room_8017FF84,
    func_acropolis_security_room_8017FFD0,
    func_acropolis_security_room_80180010,
    func_acropolis_security_room_80180030,
} };

/// Runs the security-monitor task's current state. The seven handlers are
/// copied onto the stack first, so the call goes through a local table rather
/// than through `.rodata`.
void func_acropolis_security_room_8017ED68(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_acropolis_security_room_8017D5EC;
    sp.funcs[task->state](task);
}

#include "../../shared/action_prompt_reset.inc.c"

/// Idle state of the security room's cap script: the same hotspot scan
/// `func_acropolis_security_room_8017EB9C` runs for the monitor, but against
/// the script's own table and with the hit recorded in the script's state
/// block instead of dispatched as a cap command. A confirmed
/// (`buttons[0].state == 2`) hit copies the hotspot's `id` and `promptKind` into the
/// state block and advances to state 3; with nothing under the cursor the
/// pending sub-step is cleared and the prompt merely highlights (`mode` 1).
/// `buttons[1].state == 2` leaves the scan by advancing to state 5.
static void func_acropolis_security_room_8017EE44(Task* task)
{
    RoomActionPrompt*           prompt = D_80114D28;
    OverlayHotspot*             hs     = D_acropolis_security_room_801826DC;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->work;

    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_acropolis_security_room_8017FCB0(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons.slots[0].state == 2) {
            for (; hs->id != -1; hs++) {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    st->variant      = hs->id;
                    st->promptKind   = hs->promptKind;
                    task->state      = 3;
                    return;
                }
            }
        }
    } else {
        st->field_0  = 0;
        prompt->mode = 1;
    }
    if (prompt->buttons.slots[1].state == 2) {
        task->state = 5;
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

static void func_acropolis_security_room_8017F1BC(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED);
    if ((flag == 0) || (flag == 2)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(3, 1, 0);
        } else if (step == 1) {
            Gp_ClearCollectedBit(0x104);
            SndEvt_EnqueueType6(SOUND_ACROPOLIS_SECURITY_ROOM_SHUTTER_UNLOCK, 0, 0);
            GameFlag_SetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED, GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) | 1);
            GameFlag_SetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 0xFF);
            st->field_0 = 0;
            task->state = 6;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F80);
            taskKill(task->spawnArg2.pointer);
            return;
        } else {
            Gp_StartCapSlot(3, 1, 2);
        }
    } else if ((flag == 1) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(3, 1, 1);
        } else {
            Gp_StartCapSlot(3, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

static void func_acropolis_security_room_8017F300(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    s32                         flag;
    s32                         step;

    flag = GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED);
    if ((flag == 0) || (flag == 1)) {
        step = st->field_0;
        if (step == 0) {
            Gp_StartCapSlot(4, 1, 0);
        } else if (step == 2) {
            Gp_ClearCollectedBit(0x103);
            SndEvt_EnqueueType6(SOUND_ACROPOLIS_SECURITY_ROOM_SHUTTER_UNLOCK, 0, 0);
            GameFlag_SetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED, GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) | 2);
            func_acropolis_security_room_8017FD64(GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 0xFF);
            st->field_0              = 0;
            task->state              = 0xA;
            gGameSession->eventState = 1;
            func_800E9BDC(1, 0xF9FF);
            Gp_ApplyAreaRecs(D_acropolis_security_room_80184F50);
            if (GameFlag_GetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE) < 3) {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F78);
            } else {
                Gp_ApplyAreaRecs(D_acropolis_security_room_80184F7C);
            }
            taskKill(task->spawnArg2.pointer);
            return;
        } else {
            Gp_StartCapSlot(4, 1, 2);
            task->state = 2;
            return;
        }
    } else if ((flag == 2) || (flag == 3)) {
        if (st->field_0 == 0) {
            Gp_StartCapSlot(4, 1, 1);
        } else {
            Gp_StartCapSlot(4, 1, 3);
        }
    } else {
        return;
    }
    task->state = 2;
}

/// The second prompt's copy.
#define actionPromptMoveCursors func_acropolis_security_room_8017F480
#define actionPromptDrawCursor  func_acropolis_security_room_8017F8E0
#include "../../shared/action_prompt_move_cursors.inc.c"
#undef actionPromptMoveCursors
#undef actionPromptDrawCursor

/// The second prompt's copy.
#define actionPromptDrawCursor func_acropolis_security_room_8017F8E0
#include "../../shared/action_prompt_draw_cursor.inc.c"
#undef actionPromptDrawCursor

/// Task callback of the descriptor at `D_acropolis_security_room_801826C0`:
/// a two-state dispatcher whose handler table is built on the stack rather
/// than read from `.data`, so state 0 runs
/// `func_acropolis_security_room_80180308` and state 1 runs
/// `func_acropolis_security_room_8017F480`.
void func_acropolis_security_room_8017F9C8(Task* task)
{
    TaskFunc funcs[2] = {
        func_acropolis_security_room_80180308,
        func_acropolis_security_room_8017F480,
    };

    funcs[task->state](task);
}

/// State 0 of the security-room cap script: allocates the 0x10 state block into
/// `Task::work`, spawns the script's child task, publishes the message table
/// and the current pair-flag nibble, takes a display reference and clears every
/// hotspot's `hit` flag before the first cursor scan. A failed allocation kills
/// the task instead.
static void func_acropolis_security_room_8017FA18(Task* task)
{
    AcropolisSecurityRoomState* st;
    OverlayHotspot*             hs;

    st = memCalloc(sizeof(AcropolisSecurityRoomState), 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer                                    = Task_SpawnFromTable(D_acropolis_security_room_801826C0, 0, 1, 0);
    task->msgTable                                             = D_acropolis_security_room_801826CC;
    task->work                                                 = st;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
    task->state++;
    st->field_0 = 0;
    st->frames  = 0;
    func_acropolis_security_room_8017FD64(GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & 0xFF);
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    Display_AcquireRef();
    for (hs = D_acropolis_security_room_801826DC; hs->id != -1; hs++) {
        hs->hit = 0;
    }
}

/// Arms the action prompt for the script's hotspot and steps the caller on one
/// state: highlights (`mode` 1) the fixed target id 0x80 and clears the prompt's
/// on-screen position, which `func_800D4E78` fills in again when the prompt is
/// actually spawned. The room carries a second copy of this body at
/// `func_acropolis_security_room_8017EA28`.
static void func_acropolis_security_room_8017FB20(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode this state picked.
static void func_acropolis_security_room_8017FB54(Task* task)
{
    RoomActionPrompt*           prompt = D_80114D28;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, st->promptKind);
    task->state = 4;
}

/// Runs one step of whichever cap script this task family is driving, once the
/// gameplay side reports the action prompt has been dismissed: clears the
/// prompt, then hands the task to `func_acropolis_security_room_8017F1BC` or
/// `func_acropolis_security_room_8017F300` and resets the sub-step. While
/// `func_800D4EC0` still reports a prompt up and no sub-step is pending, the
/// task instead parks on state 2. `variant` is never written in this overlay --
/// the state block is calloc'd -- so the `func_acropolis_security_room_8017F300`
/// arm is the one this room actually takes.
static void func_acropolis_security_room_8017FBA4(Task* task)
{
    RoomActionPrompt*           prompt = D_80114D28;
    AcropolisSecurityRoomState* st     = (AcropolisSecurityRoomState*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    if ((func_800D4EC0() != 0) || (st->field_0 != 0)) {
        if (st->variant == 1) {
            func_acropolis_security_room_8017F1BC(task);
            st->field_0 = 0;
            return;
        }
        func_acropolis_security_room_8017F300(task);
        st->field_0 = 0;
        return;
    }
    task->state = 2;
}

static void func_acropolis_security_room_8017FC30(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    gGameSession->eventState                                   = 0;
    gGameSession->hideHud                                      = 0;
    gGameSession->cutsceneHold                                 = 0;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    Display_ReleaseRef();
    taskKill(task->spawnArg2.pointer);
    Task_RequestKill(task, 0);
}

/// The second prompt's copy.
#define actionPromptHitTest func_acropolis_security_room_8017FCB0
#include "../../shared/action_prompt_hit_test.inc.c"
#undef actionPromptHitTest

/// Repaints the two security-monitor sprites for the current state of game
/// flag nibble 9, whose low two bits say which of the two shutters has been
/// opened. The nibble selects, for each of the two sprite commands of view 6
/// in this room's sprite record, whether `Gp_LinkViewSprts` skips linking it
/// (`field_4` non-zero) or draws it.
static void func_acropolis_security_room_8017FD64(s32 flags)
{
    GameSession*     g    = gGameSession;
    GameLocationKey* sess = &g->location.loc;
    SpriteBatch*     batches;

    batches = Gp_SprtTables[sess->stage - 1][g->spriteVariant - 1].areaViews[sess->area - 1][5].batches;
    switch (flags & 0xFF) {
        case 0:
            batches[1].hidden = 1;
            batches[2].hidden = 1;
            break;
        case 1:
            batches[1].hidden = 0;
            batches[2].hidden = 1;
            break;
        case 2:
            batches[1].hidden = 1;
            batches[2].hidden = 0;
            break;
        case 3:
            batches[1].hidden = 0;
            batches[2].hidden = 0;
            break;
    }
}

/// `TaskMessageEntry` handler for message 0x13F1, the "can this key item be used
/// here?" query `Gp_UseKeyItemRow` sends to slot 7. `item` is the key item the
/// player highlighted; the three ids this room accepts each select a sub-step
/// of the cap script, recorded in the state block's `field_0` for
/// `func_acropolis_security_room_8017FA18` to pick up. Any other item stores 0
/// and answers 0, which is the "cannot use that now" reply.
s32 func_acropolis_security_room_8017FE24(Task* task, s32 msgId, s32 item, s32 arg3)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;

    if (item == 0x101) {
        st->field_0 = 3;
        return 1;
    }
    if (item == 0x103) {
        st->field_0 = 2;
        return 1;
    }
    if (item == 0x104) {
        st->field_0 = 1;
        return 1;
    }
    st->field_0 = 0;
    return 0;
}

/// Fades the screen to white over 0x40 frames, then steps the caller on one
/// state: `frames` doubles as the fade level here, rising by 4 a frame and
/// driving `Fade_DrawOverlay`'s three colour channels together. At the halfway
/// point (0x80) the door chime is queued; once the level passes 0xFF the
/// counter is reset for the next state and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` is set to 0x10.
static void func_acropolis_security_room_8017FE6C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    u8                          level;

    level = st->frames;
    Fade_DrawOverlay(level, level, level, GPU_BLEND_SUBTRACT);
    st->frames = st->frames + 4;
    if (st->frames == 0x80) {
        SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 2), 0, 0);
    }
    if (st->frames >= 0x100) {
        st->frames                                                 = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x10;
        /* Without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store, dropping the load-delay `nop`. */
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_8017FF0C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;

    if (st->frames == 1) {
        st->child   = Task_SpawnFromTable(D_acropolis_security_room_80182700, 0, 0, 0);
        task->state = task->state + 1;
    }
    st->frames = st->frames + 1;
}

static void func_acropolis_security_room_8017FF84(Task* task)
{
    s32 killArg;

    if (Task_PollKill(((AcropolisSecurityRoomState*)task->work)->child, &killArg) != 0) {
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_8017FFD0(Task* arg0)
{
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayer3F3(0);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_acropolis_security_room_80180010(Task* task)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    /* Without the barrier GCC hoists the `lw` of `task->state` above the byte
     * store, dropping the load-delay `nop` and making the body one instruction
     * short. */
    task->state = task->state + 1;
}

static void func_acropolis_security_room_80180030(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
    func_800E9BDC(0, 0xF9FF);
    Task_RequestKill(task, 0);
}

static void func_acropolis_security_room_801800A4(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;
    s32                         level;
    s16                         frames;

    GameFlag_SetNibble(GAME_FLAG_MAP_MARK_SECURITY_ROOM, 0);
    level = (u8)st->frames;
    Fade_DrawOverlay(level, level, level, GPU_BLEND_SUBTRACT);
    frames     = st->frames + 4;
    st->frames = frames;
    if ((u16)frames == 0x80) {
        SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SECURITY_ROOM, 2), 0, 0);
    }
    if (st->frames >= 0x100) {
        st->frames                                                 = 0;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xE;
        /* Same load-delay shape as `func_acropolis_security_room_80180010`:
         * without the barrier GCC hoists the `lw` of `task->state` above the
         * byte store and drops the delay `nop`. */
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_8018014C(Task* task)
{
    AcropolisSecurityRoomState* st = (AcropolisSecurityRoomState*)task->work;

    if (st->frames == 1) {
        st->child   = Task_SpawnFromTable(D_acropolis_security_room_80182700, 1, 0, 0);
        task->state = task->state + 1;
    }
    st->frames = st->frames + 1;
}

static void func_acropolis_security_room_801801C4(Task* task)
{
    s32 killArg;

    if (Task_PollKill(((AcropolisSecurityRoomState*)task->work)->child, &killArg) != 0) {
        Gp_MsgPlayer3F3(1);
        task->state = task->state + 1;
    }
}

static void func_acropolis_security_room_80180218(Task* task)
{
    D_80114D08                                                 = 0xA;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 3;
    Display_ReleaseRef();
    func_800E9BDC(0, 0xF9FF);
    Task_RequestKill(task, 0);
    gGameSession->hideHud      = 0;
    gGameSession->cutsceneHold = 0;
    gGameSession->eventState   = 0;
}

/// Runs the cap script's current state. The sixteen handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void func_acropolis_security_room_80180294(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_acropolis_security_room_8017D63C;
    sp.funcs[task->state](task);
}

/// The second prompt's copy.
#define actionPromptReset func_acropolis_security_room_80180308
#include "../../shared/action_prompt_reset.inc.c"
#undef actionPromptReset

/// First `TaskDesc` of `D_acropolis_security_room_80182700`: starts the room's
/// looping ambience, then rides alongside the cutscene task
/// (`func_acropolis_security_room_801804CC`) until the CD queue reaches its cue
/// or the player skips, fading the loop out exactly once either way, and asks
/// the task system to kill itself.
void func_acropolis_security_room_80180368(Task* task)
{
    CdCmdQueue*       queue;
    s32               state;
    AsrAmbienceState* st;
    AsrAmbienceState* alloc;

    queue = &gCdCmdQueue;
    state = task->state;
    st    = (AsrAmbienceState*)task->work;

    switch (state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
    }
    return;

L_case0:
    alloc      = memCalloc(sizeof(AsrAmbienceState), 0);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    memFillBytes(alloc, 0, sizeof(AsrAmbienceState));
    SndEvt_EnqueueType6(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0, 0);
    goto advance;

L_case1:
    if (queue->movieFrame >= 0x46 && st->fadeStarted == 0) {
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0x14);
        st->fadeStarted = state;
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = task->state + 1;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    if (st->fadeStarted == 0) {
        SndEvt_EnqueueType7(SOUND_ACROPOLIS_SECURITY_ROOM_MOVIE_LOOP, 0x14);
    }
advance:
    task->state = task->state + 1;
    return;

L_case2:
    Task_RequestKill(task, 0);
}

/// Second `TaskDesc` of `D_acropolis_security_room_80182700`: kicks off the
/// streamed cutscene for the security room, waits for the CD queue to go idle
/// (or for the player to skip it), then asks the task system to kill itself.
void func_acropolis_security_room_801804CC(Task* arg0)
{
    u8          slotParam[4];
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
    }
    return;

L_case0:
    queue->movieFrame = 1;
    slotParam[0]      = Stream_FindSlot((u8*)&gGameSession->location.loc, 0, 0);
    CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case1:
    if (CdCmd_IsIdle() & 0xFFFF) {
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
advance:
    task->state = task->state + 1;
    return;

L_case2:
    Task_RequestKill(task, 0);
}

/// Per-frame update of the security-room's four monitor feeds: state 0 seeds
/// the four screen CLUTs from the unlit palette, state 1 re-blends each of
/// them towards its lit palette by that feed's brightness and spawns the
/// flash effects. `Task::spawnArg2` is the `EffectWork` holding the lit-feed
/// bitmask (`index`) and the four per-feed brightnesses
/// (`scale` .. `step`).
void func_acropolis_security_room_801805A4(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    s32         i;

    work  = (EffectWork*)task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;

    switch (task->state) {
        case 0: {
            u16* base = D_acropolis_security_room_80182718;
            u16* pal  = D_acropolis_security_room_80182918;
            u16* out  = D_acropolis_security_room_80183118.colors;

            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182B18;
            out = D_acropolis_security_room_80183318.colors;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182D18;
            out = D_acropolis_security_room_80183518.colors;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            pal = D_acropolis_security_room_80182F18;
            out = D_acropolis_security_room_80183718.colors;
            for (i = 0; i < 0x100; i += 0x10) {
                Gp_BlendRgb555Clut(&pal[i], &base[i], 0, &out[i]);
            }
            Gp_LoadImages(D_acropolis_security_room_80183918);
            task->state = task->state + 1;
            break;
        }

        case 1:
            work->index = D_acropolis_security_room_80183968[GameFlag_GetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED)];
            if ((Gp_GetViewIndex() & 0xFF) == 6) {
                u16* pal  = D_acropolis_security_room_80182918;
                u16* base = D_acropolis_security_room_80182718;
                u16* out  = D_acropolis_security_room_80183118.colors;
                s32  limit;

                // The cap flickers by one step every other frame.
                limit        = 0x1000 - ((gDisplayState.animFrame & 1) << 9);
                work->scale  = (work->index & 1) ? ((work->scale < limit) ? work->scale + 0x200 : limit) : 0;
                work->angle  = (work->index & 2) ? ((work->angle < limit) ? work->angle + 0x200 : limit) : 0;
                work->period = (work->index & 4) ? ((work->period < limit) ? work->period + 0x200 : limit) : 0;
                work->step   = (work->index & 8) ? ((work->step < limit) ? work->step + 0x200 : limit) : 0;

                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->scale, &out[i]);
                }
                pal = D_acropolis_security_room_80182B18;
                out = D_acropolis_security_room_80183318.colors;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->angle, &out[i]);
                }
                pal = D_acropolis_security_room_80182D18;
                out = D_acropolis_security_room_80183518.colors;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->period, &out[i]);
                }
                pal = D_acropolis_security_room_80182F18;
                out = D_acropolis_security_room_80183718.colors;
                for (i = 0; i < 0x100; i += 0x10) {
                    Gp_BlendRgb555Clut(&pal[i], &base[i], work->step, &out[i]);
                }
                Gp_LoadImages(D_acropolis_security_room_80183918);

                for (i = 0; i < 4; i++) {
                    Gp_SpawnEff(EFFECT_ACROPOLIS_SECURITY_MONITOR_FEED, coord, i, NULL);
                }
            } else if (((Gp_GetViewIndex() & 0xFF) != 8) && ((Gp_GetViewIndex() & 0xFF) != 0x10)) {
                for (i = 0; i < 4; i++) {
                    if ((work->index >> i) & 1) {
                        Gp_SpawnEff(EFFECT_ACROPOLIS_SECURITY_MONITOR_GLOW, coord, (s32)(D_acropolis_security_room_801839B8[i]),
                                    &D_acropolis_security_room_80183998[i]);
                    }
                }
            }
            break;
    }

    if (GameFlag_GetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) < 3) {
        func_acropolis_security_room_80180A78(task);
    }
}

/// Draws the security room's sweeping laser beam: two points in the emitter's
/// local frame are rotated into world space by the emitter coordinate's
/// `workm`, projected through `GsWSMATRIX`, and linked into the current OT as
/// one semi-transparent flat `LINE_F2`. The beam only exists in the two camera
/// views selected by the `0xC` bitmask over `GameSession::location.loc.view`, its far
/// endpoint sweeps with the frame counter (`gDisplayState.animFrame * 6` folded
/// into a 406-step range), and nothing is queued when the near endpoint
/// projects closer than an OTZ of 0x11.
static void func_acropolis_security_room_80180A78(Task* task)
{
    void**          scratch;
    u8*             head;
    AsrBeamScratch* blk;
    GfxCoord*       coord;
    LINE_F2*        prim;

    coord = task->extra.coordBody->coord;
    if ((0xC >> (gGameSession->location.loc.view - 1)) & 1) {
        scratch   = SCRATCH_STACK_CURSOR_SLOT;
        head      = *scratch;
        blk       = (AsrBeamScratch*)(head - 0x14);
        blk->a.vx = -0x427;
        blk->a.vy = (gDisplayState.animFrame * 6) % 406 + 0xF633;
        *scratch  = blk;
        blk->a.vz = 0x9AF;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->a);
        gte_rtv0();
        gte_stsv(&((AsrBeamScratch*)(head - 0x14))->a);
        blk->a.vx = (u16)blk->a.vx + (u16)coord->workm.t[0];
        blk->a.vy = (u16)blk->a.vy + (u16)coord->workm.t[1];
        blk->a.vz = (u16)blk->a.vz + (u16)coord->workm.t[2];
        blk->b.vx = -0x1F0;
        blk->b.vy = (gDisplayState.animFrame * 6) % 406 + 0xF633;
        blk->b.vz = 0x9AF;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->b);
        gte_rtv0();
        gte_stsv(&((AsrBeamScratch*)(head - 0x14))->b);
        blk->b.vx = (u16)blk->b.vx + (u16)coord->workm.t[0];
        blk->b.vy = (u16)blk->b.vy + (u16)coord->workm.t[1];
        blk->b.vz = (u16)blk->b.vz + (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->a);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setLineF2(prim);
        gte_stsxy(&prim->x0);
        gte_ldv0(&((AsrBeamScratch*)(head - 0x14))->b);
        gte_rtps();
        prim->code |= 2;
        gte_stsxy(&prim->x1);
        gte_stszotz(&blk->otz);
        if (((AsrBeamScratch*)(head - 0x14))->otz > 0x10) {
            setRGB0(prim, 0x10, 0x10, 0x10);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((AsrBeamScratch*)(head - 0x14))->otz << gDisplayState.otDepthShift) >> 2) &
                                                             GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_SUBTRACT, ((AsrBeamScratch*)(head - 0x14))->otz);
        }
        SCRATCH_STACK_RELEASE_BYTES(0x14);
    }
}

/// Per-frame draw for the security-room's flash sprite: refreshes the task's
/// coordinate frame, loads it into the GTE, then queues one 128x128 textured
/// quad from `D_acropolis_security_room_80183970` -- picked by the low two bits
/// of `Task::spawnArg1` -- into the current OT before releasing its `EffectWork`.
void func_acropolis_security_room_80180E34(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    POLY_FT4*   prim;
    s16         x;
    s16         y;
    u16         cx;
    u16         cy;

    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    Gp_UpdateCoord(coord);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    mem->scale  = arg0->spawnArg1.value & 3;
    prim->tpage = 0xAB;
    prim->code |= 3;
    prim->clut  = D_acropolis_security_room_80183970[mem->scale].clut << 6;
    cx          = D_acropolis_security_room_80183970[mem->scale].x;
    cy          = D_acropolis_security_room_80183970[mem->scale].y;
    prim->u0    = D_acropolis_security_room_80183970[mem->scale].u;
    prim->v0    = D_acropolis_security_room_80183970[mem->scale].v;
    prim->u1    = D_acropolis_security_room_80183970[mem->scale].u + 0x7F;
    prim->v1    = D_acropolis_security_room_80183970[mem->scale].v;
    prim->u2    = D_acropolis_security_room_80183970[mem->scale].u;
    prim->v2    = D_acropolis_security_room_80183970[mem->scale].v + 0x7F;
    prim->u3    = D_acropolis_security_room_80183970[mem->scale].u + 0x7F;
    prim->v3    = D_acropolis_security_room_80183970[mem->scale].v + 0x7F;
    x           = cx - 0x40;
    prim->x2    = x;
    prim->x0    = x;
    x           = cx + 0x3F;
    prim->x3    = x;
    prim->x1    = x;
    y           = cy - 0x40;
    prim->y1    = y;
    prim->y0    = y;
    y           = cy + 0x3F;
    prim->y3    = y;
    prim->y2    = y;
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)0x30 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    effectKillTask(mem, arg0);
}

/// Draws a rotating textured quad and updates its drift until it settles.
void func_acropolis_security_room_80181108(Task* arg0)
{
    RoomQuadScratch* blk;
    GfxCoord*        coord;
    EffectWork*      mem;
    POLY_FT4*        prim;
    s32              i;
    SVECTOR*         sv;
    s32              ty;
    s32              tx;
    s32              tz;

    SCRATCH_STACK_RESERVE_BLOCK(RoomQuadScratch);
    blk   = SCRATCH_STACK_CURSOR(RoomQuadScratch);
    coord = arg0->extra.coordBody->coord;
    mem   = arg0->spawnArg2.pointer;
    Gp_UpdateCoord(coord);

    if (mem->age == 0) {
        mem->scale      = 0x20;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->period     = 0x100 - ((gRandomLcgState >> 16) & 0x1F0);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->step       = 0x80 - ((gRandomLcgState >> 16) & 0xF0);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vx    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vy    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->move.vz    = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
    }

    for (i = 0; i < ARRAY_SIZE(D_acropolis_security_room_801839C0); i++) {
        /* Spelled as a shifted block rather than `&blk->v[i]`, which is the same
           address: the member form lets CSE share one register with the GTE
           macros' `&blk->v[i]`, and the original keeps two. */
        sv           = ((RoomQuadScratch*)((SVECTOR*)blk + i))->v;
        blk->v[i].vx = D_acropolis_security_room_801839C0[i].axis0Sign * mem->scale;
        sv->vy       = 0;
        sv->vz       = D_acropolis_security_room_801839C0[i].axis1Sign * mem->scale;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx = (u16)blk->v[i].vx + (u16)coord->workm.t[0];
        sv->vy       = (u16)sv->vy + (u16)coord->workm.t[1];
        sv->vz       = (u16)sv->vz + (u16)coord->workm.t[2];
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    prim->u0 = 0;
    prim->v0 = 0;
    prim->u1 = 7;
    prim->v1 = 0;
    prim->u2 = 0;
    prim->v2 = 7;
    prim->u3 = 7;
    prim->v3 = 7;
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz > 0x10) {
        prim->tpage = 0x2D;
        prim->clut  = 0x4390;
        prim->code |= 1;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomQuadScratch);

    if (mem->index == 0) {
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        coord->coord.t[2] += mem->move.vz;
        gfxRotMatrixX(&coord->coord, mem->period, GRAPHICS_ROTATION_COMPOSE);
        gfxRotMatrixZ(&coord->coord, mem->step, GRAPHICS_ROTATION_COMPOSE);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;

        ty = mem->move.vy;
        if (ty >= 0x1D) {
            ty--;
        } else {
            ty++;
        }
        mem->move.vy = ty;

        tx = mem->move.vx;
        if (tx == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vx   += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
        } else {
            if (tx > 0) {
                tx--;
            } else {
                tx++;
            }
            mem->move.vx = tx;
        }

        tz = mem->move.vz;
        if (tz == 0) {
            mem->move.vz   += mem->step % 32;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->move.vz   += (2 - (u16)((gRandomLcgState >> 16) % 5U)) * 8;
        } else {
            if (tz > 0) {
                tz--;
            } else {
                tz++;
            }
            mem->move.vz = tz;
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->period    += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 16;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->step      += (1 - (u16)((gRandomLcgState >> 16) % 3U)) * 8;
        if (coord->coord.t[1] >= -0x1A3) {
            mem->index = 1;
        }
    }

    mem->age = mem->age + 1;
    if (gGameSession->location.loc.view != 0xF) {
        effectKillTask(mem, arg0);
    }
}

/// Draws a flash at the object's projected position: two gouraud quads and two
/// lines meeting at the centre, sized `0xC00 / otz` and skipped when the point
/// is too near (otz <= 0x10). `spawnArg1` bit 1 lights the red channel and bit 0
/// the green one, both at one random brightness.
void func_acropolis_security_room_801817A4(Task* task)
{
    GfxCoord*        coord;
    void*            mem;
    AsrFlashScratch* scratch;
    POLY_G4*         quad;
    LINE_G3*         line;
    s16              lum;
    s16              red;
    s16              green;
    s32              i;

    coord = task->extra.coordBody->coord;
    mem   = task->spawnArg2.pointer;
    Gp_UpdateCoord(coord);
    scratch       = SCRATCH_STACK_RESERVE_BLOCK(AsrFlashScratch);
    scratch->v.vx = coord->workm.t[0];
    scratch->v.vy = coord->workm.t[1];
    scratch->v.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->v);
    gte_rtps();
    gte_stsxy(&scratch->x);
    gte_stszotz(&scratch->otz);
    if (scratch->otz >= 0x11) {
        red             = (task->spawnArg1.value >> 1) & 1;
        green           = task->spawnArg1.value & 1;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        lum             = ((gRandomLcgState >> 16) & 0x70) + 0x40;
        scratch->step   = 0xC00 / scratch->otz;
        for (i = 0; i < 2; i++) {
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, 0, 0, 0);
            setRGB1(quad, 0, 0, 0);
            setRGB2(quad, lum * red, green * lum, 0);
            setRGB3(quad, 0, 0, 0);
            quad->x0 = scratch->x - scratch->step;
            quad->x1 = quad->x2 = scratch->x;
            quad->x3            = scratch->x + scratch->step;
            quad->y0 = quad->y2 = quad->y3 = scratch->y;
            quad->y1                       = (scratch->y - scratch->step) + scratch->step * (i + i);
            addPrim(&gGpuCurrentOt[((u32)scratch->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF], quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        }
        for (i = 0; i < 2; i++) {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, lum * red, green * lum, 0);
            setRGB2(line, 0, 0, 0);
            line->x0 = scratch->x + scratch->step * (i * 2 - 1);
            line->y0 = scratch->y - scratch->step * (i + 1);
            line->x1 = scratch->x;
            line->y1 = scratch->y;
            line->x2 = scratch->x - scratch->step * (i * 2 - 1);
            line->y2 = scratch->y + scratch->step * (i + 1);
            addPrim(&gGpuCurrentOt[((u32)scratch->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF], line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, scratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(AsrFlashScratch);
    effectKillTask(mem, task);
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

/// Per-frame visibility hook for a pick-up prop: the model is drawn with flags
/// 8 at OT offset 0 until the item's 2-bit flag reaches 2, after which it is
/// hidden (flags 0x80).
static void func_acropolis_security_room_80182574(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2.pointer;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    Gp_GetViewIndex();
    if (flag == 2) {
        tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
    }
}
