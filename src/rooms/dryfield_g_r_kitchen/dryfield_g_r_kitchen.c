#include "rooms/dryfield_g_r_kitchen.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#define ROOM_EVENT_ACTIVE gRoomEventActive[0]
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"

#define D_dryfield_g_r_kitchen_8017EBF0 (D_dryfield_g_r_kitchen_8017EBE8 + 1)
#define D_dryfield_g_r_kitchen_8017EC08 (D_dryfield_g_r_kitchen_8017EBE8 + 4)

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 gRoomEventActive[4];

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The room's message table, installed on the room task by its entry state.
extern GpMsgEntry D_dryfield_g_r_kitchen_8017EBC0[];

/// Endpoints of the two beams drawn in view 2. The code forms this address,
/// but the table starts one entry earlier, so the beams run from `[0]` to
/// `[-1]` and from `[2]` to `[1]`.

/// Endpoints of the two beams drawn in view 3: `[0]` to `[1]` and `[2]` to
/// `[3]`.

static void func_dryfield_g_r_kitchen_8017D958(Task* task);
static void func_dryfield_g_r_kitchen_8017D99C(Task* task);
static void func_dryfield_g_r_kitchen_8017E27C(GfxCoord* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3);

// Indexed views below share one contiguous table.
s32 func_dryfield_g_r_kitchen_8017D8BC(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_g_r_kitchen_8017D8C4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_g_r_kitchen_8017D948(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_g_r_kitchen_8017D950(Task*, s32, TaskMessageArg, TaskMessageArg);

extern GpGridParams   D_dryfield_g_r_kitchen_8017EEC0[1];
extern GpObj4C        D_dryfield_g_r_kitchen_8017F038[2];
extern GpObj4C        D_dryfield_g_r_kitchen_8017F0D0[7];
extern GpRoomCoordSet D_dryfield_g_r_kitchen_8017F464[1];

TaskDesc gRoomEventTaskDesc = { 0, 32, roomEventTask, { .model = NULL } };

GpMsgEntry D_dryfield_g_r_kitchen_8017EBC0[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_g_r_kitchen_8017D8C4 },
    { 5105, func_dryfield_g_r_kitchen_8017D8BC },
    { 5103, func_dryfield_g_r_kitchen_8017D950 },
    { 5104, func_dryfield_g_r_kitchen_8017D948 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_dryfield_g_r_kitchen_8017EBE8[8] = {
    { -1473, -2181, 2023, 0 },
    { -1473, -2181, 2416, 0 },
    { -197, -2161, 2973, 0 },
    { 197, -2161, 2973, 0 },
    { -70, -2790, -370, 0 },
    { -70, -2790, 370, 0 },
    { 70, -2790, -370, 0 },
    { 70, -2790, 370, 0 },
};

GpRoomObjRec D_dryfield_g_r_kitchen_8017EC28[1] = {
    { D_dryfield_g_r_kitchen_8017EEC0, D_dryfield_g_r_kitchen_8017F038, D_dryfield_g_r_kitchen_8017F0D0, NULL },
};

u8* D_dryfield_g_r_kitchen_8017EC38[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_g_r_kitchen_8017EC3C[1] = {
    { { .bytes = { 3, 0 } } },
};

GpRoomCoordRec D_dryfield_g_r_kitchen_8017EC40[1] = {
    { D_dryfield_g_r_kitchen_8017F464, NULL },
};

GpWarpRec D_dryfield_g_r_kitchen_8017EC48[2] = {
    { { .words = { 1024, -1168, 0, 2135 } }, { 0, 0, 0, 0 }, { .words = { 1024, -1168, 0, 2135 } }, { 0, 0, 0, 0 }, 0x52130002, 0x52130001, 0, 2, 0, 478 },
    { { .words = { 2048, 0, 0, 2512 } }, { 0, 0, 0, 0 }, { .words = { 2048, 0, 0, 2512 } }, { 0, 0, 0, 0 }, 0x52130002, 0x52130001, 0, 2, 0, 477 },
};

SVECTOR D_dryfield_g_r_kitchen_8017ECB8[6] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_normals.inc"
};

SVECTOR D_dryfield_g_r_kitchen_8017ECE8[26] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_verts.inc"
};

GpGridFace D_dryfield_g_r_kitchen_8017EDB8[16] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_faces.inc"
};

s16 D_dryfield_g_r_kitchen_8017EE78[32] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_g_r_kitchen_8017EE78[i])
s16* D_dryfield_g_r_kitchen_8017EEB8[2] = {
#include "assets/dryfield_g_r_kitchen_collision_01900_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_g_r_kitchen_8017EEC0[1] = {
    { NULL, D_dryfield_g_r_kitchen_8017ECB8, D_dryfield_g_r_kitchen_8017ECE8, D_dryfield_g_r_kitchen_8017EDB8, D_dryfield_g_r_kitchen_8017EEB8, 1800, 3000, 1, 2, 4000, 16 },
};

GpViewRec D_dryfield_g_r_kitchen_8017EEE4[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 1053 },
    { { { { 3955, 0, 1064 }, { 591, 3405, -2198 }, { -885, 2276, 3288 } }, { -400, 2700, 1000 } }, 230 },
    { { { { -3988, 0, 933 }, { -276, 3911, -1183 }, { -891, -1215, -3808 } }, { -500, 400, -2700 } }, 230 },
};

SpriteBatch D_dryfield_g_r_kitchen_8017EF50[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_g_r_kitchen_8017EF60[7] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -88, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, -40, 675, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -112, -16, 725, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -104, 0, 750, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -96, 16, 750, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 32, 700, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, 40, 675, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_g_r_kitchen_8017EFEC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_g_r_kitchen_8017F004[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_g_r_kitchen_8017F014[3] = {
    { { .empty = D_dryfield_g_r_kitchen_8017EF50 }, D_dryfield_g_r_kitchen_8017EF50, NULL },
    { { .elements = D_dryfield_g_r_kitchen_8017EF60 }, D_dryfield_g_r_kitchen_8017EFEC, NULL },
    { { .empty = D_dryfield_g_r_kitchen_8017F004 }, D_dryfield_g_r_kitchen_8017F004, NULL },
};

GpObj4C D_dryfield_g_r_kitchen_8017F038[2] = {
    { NULL, NULL, NULL, { -2, -1167, 381, 0 }, { { 1974, -1520, 391, 0 }, { -1973, -1520, -390, 0 }, { 1974, 1520, 391, 0 }, { -1973, 1520, -390, 0 } }, { -796, 0, 4017, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -1, -1152, 479, 0 }, { { -1973, -1520, -390, 0 }, { 1974, -1520, 391, 0 }, { -1973, 1520, -390, 0 }, { 1974, 1520, 391, 0 } }, { 794, 0, -4019, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 3, 2, 129, 0 },
};

GpObj4C D_dryfield_g_r_kitchen_8017F0D0[7] = {
    { NULL, NULL, NULL, { -1248, -48, 2336, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, 0, 18, 18, 2, 0 },
    { NULL, NULL, NULL, { 96, -48, 2784, 0 }, { { 736, 0, -320, 0 }, { 736, 0, 320, 0 }, { -736, 0, -320, 0 }, { -736, 0, 320, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 801, 0, 20, 33, 2, 0 },
    { NULL, NULL, NULL, { -640, -64, -415, 0 }, { { -320, 0, -576, 0 }, { 320, 0, -576, 0 }, { -320, 0, 576, 0 }, { 320, 0, 576, 0 } }, { 0, 4104, 0, 0 }, { 4096, 0, 0, 0 }, 658, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 416, -64, -2400, 0 }, { { -608, 0, -576, 0 }, { 608, 0, -576, 0 }, { -608, 0, 576, 0 }, { 608, 0, 576, 0 } }, { 0, 4099, 0, 0 }, { -401, 0, 4076, 0 }, 836, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -544, -64, 944, 0 }, { { -320, 0, -592, 0 }, { 320, 0, -592, 0 }, { -320, 0, 592, 0 }, { 320, 0, 592, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 671, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { -640, -64, -1696, 0 }, { { -320, 0, -592, 0 }, { 320, 0, -592, 0 }, { -320, 0, 592, 0 }, { 320, 0, 592, 0 } }, { 0, 4106, 0, 0 }, { 4096, 0, 0, 0 }, 671, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 736, -64, -1024, 0 }, { { -416, 0, -1232, 0 }, { 416, 0, -1232, 0 }, { -416, 0, 1232, 0 }, { 416, 0, 1232, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1299, 2, 4, 0, 130, 0 },
};

WorldCoordPointLight D_dryfield_g_r_kitchen_8017F2E4[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2400, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 2867, 2867 }, { 0, 0 } }, 1000, 3200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1800, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1300, -1800, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 3686 }, { 0, 0 } }, 300, 1200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1638, 1638 }, { 0, 0 } }, 0x186A0, 0x186A0 },
};

GpRoomCoordSet D_dryfield_g_r_kitchen_8017F464[1] = {
    { 0, NULL, 4, D_dryfield_g_r_kitchen_8017F2E4, 0, NULL },
};

GpAreaTmdRec D_dryfield_g_r_kitchen_8017F47C[2] = {
    { 15, 15, 0, 0, { 0, 0 }, D_8013BE28 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_g_r_kitchen_8017F494[3] = {
    { 15, 15, 0, 0, { 0, 0 }, D_8013BE28 },
    { 7, 7, 1, 0, { 0, 0 }, D_80150C80 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_g_r_kitchen_8017F4B8[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B474, D_dryfield_g_r_kitchen_8017F47C },
    { D_map_dryfield_8017B4B4, D_dryfield_g_r_kitchen_8017F494 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_dryfield_g_r_kitchen_8017F520[3] = {
    0x10000035,
    0x10000037,
    0x10000035,
};

GpRoomParamRec D_dryfield_g_r_kitchen_8017F52C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_g_r_kitchen_8017F534[1] = {
    { 0, 0, 1, 0, D_dryfield_g_r_kitchen_8017F520 },
};

GpRoomParamRec* D_dryfield_g_r_kitchen_8017F53C[8] = {
    D_dryfield_g_r_kitchen_8017F52C,
    D_dryfield_g_r_kitchen_8017F534,
    D_dryfield_g_r_kitchen_8017F52C,
    D_dryfield_g_r_kitchen_8017F52C,
    D_dryfield_g_r_kitchen_8017F52C,
    D_dryfield_g_r_kitchen_8017F52C,
    D_dryfield_g_r_kitchen_8017F52C,
    D_dryfield_g_r_kitchen_8017F52C,
};

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive[4] = {
    0,
    34,
    223,
    253,
};

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_g_r_kitchen_8017D9A4`: the entry state
/// `func_dryfield_g_r_kitchen_8017D958`, the idle state
/// `func_dryfield_g_r_kitchen_8017D99C`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_g_r_kitchen_8017D5DC = {
    { func_dryfield_g_r_kitchen_8017D958, func_dryfield_g_r_kitchen_8017D99C, taskKill },
};

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_g_r_kitchen_8017D8BC(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`, and for area 0x14 passes the warp
/// through the event gate with the room's own request - nibble 0x34, no collected bit,
/// cap command 3 and the two sound ids 0x52130001 and 0x52130004 - answering
/// with the gate's result. Any other area answers 1.
s32 func_dryfield_g_r_kitchen_8017D8C4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    if (in->areaId == 0x14) {
        req.capCmd        = 3;
        req.missingCapCmd = 3;
        req.firstSnd      = 0x52130001;
        req.secondSnd     = 0x52130004;
        req.flagId        = 0x34;
        req.collectedBit  = 0;
        ret               = roomEventGate(&req, in);
    } else {
        ret = 1;
    }
    return ret;
}

/// Handler for message 0x13F0 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_g_r_kitchen_8017D948(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_g_r_kitchen_8017D950(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7 and advances to the idle state.
static void func_dryfield_g_r_kitchen_8017D958(Task* task)
{
    task->msgTable = D_dryfield_g_r_kitchen_8017EBC0;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room task.
static void func_dryfield_g_r_kitchen_8017D99C(Task* task)
{
}

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_g_r_kitchen_8017D9A4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_g_r_kitchen_8017D5DC;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_tapered_beam.inc.c"

/// The same tapered light beam as `glowDrawTaperedBeam`,
/// between two points of `arg0`'s local space, with every angle turned back a
/// quarter turn. The two
/// `RTPS` projections, the drop when the far end's `otz` is below 0x11, the
/// clamp of the near end's `otz` to 0x10 and the radii `(s16)arg3 * 64 / otz`
/// are unchanged.
///
/// The near cap's wedges cover -0x400..0x400, the far cap's the opposite half
/// walked backwards from 0xC00 to 0x400, and the side quads join the two
/// circles at -0x400 and 0x400. The centre colour is 0x10 or 0x20 on the
/// parity of `gDisplayState.animFrame`, one step darker than
/// `glowDrawTaperedBeam`'s.
static void func_dryfield_g_r_kitchen_8017E27C(GfxCoord* arg0, SVECTOR* arg1, SVECTOR* arg2, s32 arg3)
{
    u8*                head;
    RoomDraw24Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x28;
        *scratch = tmp;
        block    = (RoomDraw24Scratch*)tmp;
    }

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg1);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    (u16) block->vec0.vx = (u16)block->vec0.vx + (u16)arg0->workm.t[0];
    (u16) block->vec0.vy = (u16)block->vec0.vy + (u16)arg0->workm.t[1];
    (u16) block->vec0.vz = (u16)block->vec0.vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&arg0->workm);
    gte_ldv0(arg2);
    gte_rtv0();
    gte_stsv(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    (u16) block->vec1.vx = (u16)block->vec1.vx + (u16)arg0->workm.t[0];
    (u16) block->vec1.vy = (u16)block->vec1.vy + (u16)arg0->workm.t[1];
    (u16) block->vec1.vz = (u16)block->vec1.vz + (u16)arg0->workm.t[2];

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec0);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(&((RoomDraw24Scratch*)(head - 0x28))->vec1);
    gte_rtps();
    gte_stsxy(&((RoomDraw24Scratch*)(head - 0x28))->sx1);
    gte_stszotz(&((RoomDraw24Scratch*)(head - 0x28))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw24Scratch*)(head - 0x28))->otz0 < 0x10) {
            ((RoomDraw24Scratch*)(head - 0x28))->otz0 = 0x10;
        }
        extent    = (s16)arg3 * 64;
        r0        = extent / ((RoomDraw24Scratch*)(head - 0x28))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x10;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang - 0x400)) >> 12);
            t        = ang - 0x200;
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang - 0x400)) >> 12);
            prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(ang * 2 - 0x400)) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(ang * 2 - 0x400)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(ang * 2 - 0x400)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(ang * 2 - 0x400)) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(0xC00 - ang)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(0xC00 - ang)) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(0xA00 - ang)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(0xA00 - ang)) >> 12);
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            prim->x3 = block->sx1 + ((block->r1 * rsin(0x800 - ang)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(0x800 - ang)) >> 12);
            ang     += 0x400;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

/// Draws two light beams under the task's coordinate in `arg0->extra.coordBody->coord`,
/// picked by the current view `gGameSession->location.loc.view`: in view 2 the
/// beams of `D_dryfield_g_r_kitchen_8017EBF0` through
/// `glowDrawTaperedBeam`, in view 3 those of
/// `D_dryfield_g_r_kitchen_8017EC08` through
/// `func_dryfield_g_r_kitchen_8017E27C`. Any other view draws nothing.
void func_dryfield_g_r_kitchen_8017EB04(Task* arg0)
{
    GfxCoord* coord;

    coord = arg0->extra.coordBody->coord;
    if (gGameSession->location.loc.view == 2) {
        glowDrawTaperedBeam(coord, &D_dryfield_g_r_kitchen_8017EBF0[0], &D_dryfield_g_r_kitchen_8017EBF0[-1], 0x100);
        glowDrawTaperedBeam(coord, &D_dryfield_g_r_kitchen_8017EBF0[2], &D_dryfield_g_r_kitchen_8017EBF0[1], 0x100);
    } else if (gGameSession->location.loc.view == 3) {
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[0], &D_dryfield_g_r_kitchen_8017EC08[1], 0x100);
        func_dryfield_g_r_kitchen_8017E27C(coord, &D_dryfield_g_r_kitchen_8017EC08[2], &D_dryfield_g_r_kitchen_8017EC08[3], 0x100);
    }
}
