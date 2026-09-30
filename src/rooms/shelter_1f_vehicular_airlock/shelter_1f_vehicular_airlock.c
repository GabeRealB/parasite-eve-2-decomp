#include "rooms/shelter_1f_vehicular_airlock.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern s8 D_shelter_1f_vehicular_airlock_80182AB0[4];
// Scalar symbol view preserves the original byte/halfword address formation.
extern s8 D_shelter_1f_vehicular_airlock_80182AB0_value __asm__("D_shelter_1f_vehicular_airlock_80182AB0");

extern TaskDesc D_shelter_1f_vehicular_airlock_80182028;

/// The room's message table, which its cap scripts index.
extern GpMsgEntry D_shelter_1f_vehicular_airlock_80182034[];

extern SVECTOR D_shelter_1f_vehicular_airlock_8018205C[];
extern SVECTOR D_shelter_1f_vehicular_airlock_8018206C[];

/// The trail's two ends as offsets from the parent coordinate: `[0]` places
/// the object itself and `[1]` the trail's far end.

/// The far end's offset, `RoomFx_TrailOffsets[1]` reached
/// by its own name, as the task does on every tick after the first.

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage D_shelter_1f_vehicular_airlock_80182AA0;

/// The message and event the message handler latched for the room's event
/// task.
extern RoomEventMsg     D_shelter_1f_vehicular_airlock_80182AA8;
extern RoomLatchedEvent D_shelter_1f_vehicular_airlock_80182AB4;

static void func_shelter_1f_vehicular_airlock_8017DC80(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_1f_vehicular_airlock_8017E468(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor);
static void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

void func_shelter_1f_vehicular_airlock_8017D644(Task*);
s32  func_shelter_1f_vehicular_airlock_8017D7DC(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_1f_vehicular_airlock_8017D988(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_1f_vehicular_airlock_8017D990(Task*, s32, s32, TaskMessageArg);
s32  func_shelter_1f_vehicular_airlock_8017D9F4(Task*, s32, TaskMessageArg, TaskMessageArg);

extern u32     D_shelter_1f_vehicular_airlock_80180C74[1];
extern SVECTOR D_shelter_1f_vehicular_airlock_80180C78[116];
extern TmdBone D_shelter_1f_vehicular_airlock_80180C50[1];
extern u32     D_shelter_1f_vehicular_airlock_80181018[1019];

extern GpGridParams               D_shelter_1f_vehicular_airlock_80182438[1];
extern GpObj4C                    D_shelter_1f_vehicular_airlock_80182714[2];
extern GpObj4C                    D_shelter_1f_vehicular_airlock_801827AC[7];
extern WorldCoordRoomAmbientEntry D_shelter_1f_vehicular_airlock_801829C0[4];
extern GpRoomCoordSet             D_shelter_1f_vehicular_airlock_801826FC[1];

TmdBone D_shelter_1f_vehicular_airlock_80180C50[1] = {
#include "assets/shelter_1f_vehicular_airlock_model_04A44_skeleton.inc"
};

u32 D_shelter_1f_vehicular_airlock_80180C74[1] = {
#include "assets/shelter_1f_vehicular_airlock_model_04A44_partVerts.inc"
};

SVECTOR D_shelter_1f_vehicular_airlock_80180C78[116] = {
#include "assets/shelter_1f_vehicular_airlock_model_04A44_verts.inc"
};

u32 D_shelter_1f_vehicular_airlock_80181018[1019] = {
#include "assets/shelter_1f_vehicular_airlock_model_04A44_stream.inc"
};

TmdSource D_shelter_1f_vehicular_airlock_80182004 = {
    0,
    6672,
    0,
    1,
    D_shelter_1f_vehicular_airlock_80180C74,
    D_shelter_1f_vehicular_airlock_80180C78,
    &D_shelter_1f_vehicular_airlock_80180C78[116],
    D_shelter_1f_vehicular_airlock_80180C50,
    D_shelter_1f_vehicular_airlock_80181018,
};

TaskDesc D_shelter_1f_vehicular_airlock_80182028 = { 0, 32, func_shelter_1f_vehicular_airlock_8017D644, { .model = NULL } };

GpMsgEntry D_shelter_1f_vehicular_airlock_80182034[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_1f_vehicular_airlock_8017D7DC },
    { 5105, func_shelter_1f_vehicular_airlock_8017D988 },
    { 5103, func_shelter_1f_vehicular_airlock_8017D9F4 },
    { 5104, func_shelter_1f_vehicular_airlock_8017D990 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_1f_vehicular_airlock_8018205C[2] = {
    { -0x2774, -3090, 1790, 0 },
    { -8900, -3090, 1790, 0 },
};

SVECTOR D_shelter_1f_vehicular_airlock_8018206C[16] = {
    { -6600, -3090, 1790, 0 },
    { -5400, -3090, 1790, 0 },
    { -3100, -3090, 1790, 0 },
    { -1900, -3090, 1790, 0 },
    { -0x2774, -3090, -1790, 0 },
    { -8900, -3090, -1790, 0 },
    { -6600, -3090, -1790, 0 },
    { -5400, -3090, -1790, 0 },
    { -3100, -3090, -1790, 0 },
    { -1900, -3090, -1790, 0 },
    { -9400, -1960, -2250, 0 },
    { -8600, -1960, -2250, 0 },
    { -5000, -2130, 2240, 0 },
    { -0x28AA, -3490, 100, 0 },
    { -0x2882, -3610, 900, 0 },
    { -0x2882, -3610, -900, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

GpRoomObjRec D_shelter_1f_vehicular_airlock_801820FC[1] = {
    { D_shelter_1f_vehicular_airlock_80182438, D_shelter_1f_vehicular_airlock_80182714, D_shelter_1f_vehicular_airlock_801827AC, NULL },
};

GpRoomCoordRec D_shelter_1f_vehicular_airlock_8018210C[1] = {
    { D_shelter_1f_vehicular_airlock_801826FC, D_shelter_1f_vehicular_airlock_801829C0 },
};

u8* D_shelter_1f_vehicular_airlock_80182114[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_1f_vehicular_airlock_80182118[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_shelter_1f_vehicular_airlock_8018211C[3] = {
    { { .words = { 2048, -5000, 0, 1570 } }, { 0, 0, 0, 0 }, { .words = { 2048, -4960, 0, 1380 } }, { 0, 0, 0, 0 }, 0x55020002, 0x55020001, 0, 2, 0, 0 },
    { { .words = { 0, -8900, 0, -1730 } }, { 0, 0, 0, 0 }, { .words = { 2048, -4960, 0, 1380 } }, { 0, 0, 0, 0 }, 0x55020002, 0x55020001, 0, 3, 0, 0 },
    { { .words = { 1024, -0x2710, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 1024, -0x2710, 0, 0 } }, { 0, 0, 0, 0 }, 0x55020004, 0x55020003, 0, 3, 0, 428 },
};

SVECTOR D_shelter_1f_vehicular_airlock_801821C4[10] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_normals.inc"
};

SVECTOR D_shelter_1f_vehicular_airlock_80182214[33] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_verts.inc"
};

GpGridFace D_shelter_1f_vehicular_airlock_8018231C[12] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_faces.inc"
};

s16 D_shelter_1f_vehicular_airlock_801823AC[52] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_1f_vehicular_airlock_801823AC[i])
s16* D_shelter_1f_vehicular_airlock_80182414[9] = {
#include "assets/shelter_1f_vehicular_airlock_collision_04E78_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_1f_vehicular_airlock_80182438[1] = {
    { NULL, D_shelter_1f_vehicular_airlock_801821C4, D_shelter_1f_vehicular_airlock_80182214, D_shelter_1f_vehicular_airlock_8018231C, D_shelter_1f_vehicular_airlock_80182414, 0x2CEC, 3000, 3, 3, 4000, 12 },
};

GpViewRec D_shelter_1f_vehicular_airlock_8018245C[3] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { 723, 0, -4031 }, { -887, 3995, -159 }, { 3932, 901, 706 } }, { 9160, 2690, 790 } }, 257 },
    { { { { 596, 0, 4052 }, { -95, 4094, 14 }, { -4051, -97, 596 } }, { 1760, 1260, 790 } }, 257 },
};

SpriteBatch D_shelter_1f_vehicular_airlock_801824C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_vehicular_airlock_801824D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_1f_vehicular_airlock_801824E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_1f_vehicular_airlock_801824F8[3] = {
    { { .empty = D_shelter_1f_vehicular_airlock_801824C8 }, D_shelter_1f_vehicular_airlock_801824C8, NULL },
    { { .empty = D_shelter_1f_vehicular_airlock_801824D8 }, D_shelter_1f_vehicular_airlock_801824D8, NULL },
    { { .empty = D_shelter_1f_vehicular_airlock_801824E8 }, D_shelter_1f_vehicular_airlock_801824E8, NULL },
};

GpPointLight D_shelter_1f_vehicular_airlock_8018251C[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -2130, 1990 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 1638, 1638 }, { 0, 0 } }, 500, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2500, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6000, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9500, -2000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 1638, 1064 }, { 0, 0 } }, 2000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -9000, -1635, -1950 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3276, 3276 }, { 0, 0 } }, 500, 1000 },
};

GpRoomCoordSet D_shelter_1f_vehicular_airlock_801826FC[1] = {
    { 0, NULL, 5, D_shelter_1f_vehicular_airlock_8018251C, 0, NULL },
};

GpObj4C D_shelter_1f_vehicular_airlock_80182714[2] = {
    { NULL, NULL, NULL, { -5709, -3248, -16, 0 }, { { -255, -3712, -3633, 0 }, { 254, -3712, 3632, 0 }, { -255, 3712, -3633, 0 }, { 254, 3712, 3632, 0 } }, { 4087, 0, -287, 0 }, { 0, 0, 4096, 0 }, 5196, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -5519, -3312, -32, 0 }, { { 238, -3712, 3884, 0 }, { -245, -3712, -3892, 0 }, { 238, 3712, 3884, 0 }, { -245, 3712, -3892, 0 } }, { -4090, 0, 253, 0 }, { 0, 0, 4096, 0 }, 5369, 0, 3, 2, 129, 0 },
};

GpObj4C D_shelter_1f_vehicular_airlock_801827AC[7] = {
    { NULL, NULL, NULL, { -4800, -48, 2080, 0 }, { { -576, 0, -416, 0 }, { 576, 0, -416, 0 }, { -576, 0, 416, 0 }, { 576, 0, 416, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 709, 0, 5, 18, 2, 0 },
    { NULL, NULL, NULL, { -8864, -48, -2112, 0 }, { { -800, 0, -416, 0 }, { 800, 0, -416, 0 }, { -800, 0, 416, 0 }, { 800, 0, 416, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 900, 0, 6, 33, 2, 0 },
    { NULL, NULL, NULL, { -0x2820, -48, 16, 0 }, { { 416, 0, -2512, 0 }, { 416, 0, 2512, 0 }, { -416, 0, -2512, 0 }, { -416, 0, 2512, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 2534, 0, 3, 49, 2, 0 },
    { NULL, NULL, NULL, { -9072, -64, 704, 0 }, { { -1520, 0, -928, 0 }, { 1520, 0, 96, 0 }, { -1520, 0, -96, 0 }, { 1520, 0, 928, 0 } }, { 0, 4116, 0, 0 }, { 1380, 0, -3857, 0 }, 1778, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { -7168, -64, 1736, 0 }, { { -416, 0, -936, 0 }, { 416, 0, -648, 0 }, { -416, 0, 792, 0 }, { 416, 0, 792, 0 } }, { 0, 4100, 0, 0 }, { 4052, 0, 601, 0 }, 1024, 2, 3, 255, 2, 0 },
    { NULL, NULL, NULL, { -1920, -64, 0, 0 }, { { 416, 0, -2512, 0 }, { 416, 0, 2512, 0 }, { -416, 0, -2512, 0 }, { -416, 0, 2512, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 2534, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -7312, -64, 1072, 0 }, { { -880, 0, -1080, 0 }, { 880, 0, -696, 0 }, { -880, 0, 840, 0 }, { 880, 0, 936, 0 } }, { 0, 4097, 0, 0 }, { 3166, 0, -2599, 0 }, 1390, 2, 3, 255, 130, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_1f_vehicular_airlock_801829C0[4] = {
    { .viewCount = ARRAY_SIZE(D_shelter_1f_vehicular_airlock_801829C0) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 700, 700, 700, 700 } },
    { .color = { 700, 700, 700, 700 } },
};

GpAreaTmdRec D_shelter_1f_vehicular_airlock_801829E0[3] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_shelter_1f_vehicular_airlock_80182A04[12] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AED0, D_shelter_1f_vehicular_airlock_801829E0 },
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

s32 D_shelter_1f_vehicular_airlock_80182A64[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

GpRoomParamRec D_shelter_1f_vehicular_airlock_80182A70[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_1f_vehicular_airlock_80182A78[1] = {
    { 0, 0, 1, 0, D_shelter_1f_vehicular_airlock_80182A64 },
};

GpRoomParamRec* D_shelter_1f_vehicular_airlock_80182A80[8] = {
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A78,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
    D_shelter_1f_vehicular_airlock_80182A70,
};

RoomFadeStorage D_shelter_1f_vehicular_airlock_80182AA0 = { 0 };

RoomEventMsg D_shelter_1f_vehicular_airlock_80182AA8 = { 0 };

s8 D_shelter_1f_vehicular_airlock_80182AB0[4] = {
    0,
    2,
    -16,
    65,
};

RoomLatchedEvent D_shelter_1f_vehicular_airlock_80182AB4 = { 0 };

static __inline__ s32 _shelter1fVehicularAirlockStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_1f_vehicular_airlock_8017D9FC(Task* task);
static void           func_shelter_1f_vehicular_airlock_8017DA40(Task* task);

/// Sets bit 0x80 of the task's model flags while the 2-bit game flag its spawn
/// argument names reads 2, and clears it otherwise.
void func_shelter_1f_vehicular_airlock_8017D5E4(Task* task)
{
    TmdObject* obj = task->extra.tmd;

    if (Gp_GetCurBit2Flag(((RoomFlagModelArg*)task->spawnArg2.pointer)->flagId) == 2) {
        obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
}

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_1f_vehicular_airlock_8017D644(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_vehicular_airlock_80182AB4.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_vehicular_airlock_80182AB4.fade != 0) {
                    D_shelter_1f_vehicular_airlock_80182AA0.fade.field_0 = 0;
                    D_shelter_1f_vehicular_airlock_80182AA0.fade.field_1 = 0;
                    D_shelter_1f_vehicular_airlock_80182AA0.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_1f_vehicular_airlock_80182AA0.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_vehicular_airlock_80182AB4.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_vehicular_airlock_80182AB4.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_vehicular_airlock_80182AB4.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_1f_vehicular_airlock_80182AA8.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_1f_vehicular_airlock_80182AA8.warp;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_1f_vehicular_airlock_80182AA8.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

static __inline__ s32 _shelter1fVehicularAirlockStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_1f_vehicular_airlock_80182AB0_value = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_1f_vehicular_airlock_80182AA8 = *dst;
            D_shelter_1f_vehicular_airlock_80182AB4 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_vehicular_airlock_80182028, 0, 0, 0);
            D_shelter_1f_vehicular_airlock_80182AB0_value = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D7DC(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent event;

    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if (in->areaId == 3) {
        if (GameFlag_GetNibble(0xB2) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(2);
            }
            return 0;
        }
        event.capCmd   = 4;
        event.stageSnd = 0x55020003;
        event.flagId   = 0x15B;
        event.fade     = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    if (in->areaId == 5) {
        event.capCmd   = 6;
        event.stageSnd = 0x55020001;
        event.flagId   = 0x15A;
        event.fade     = 0;
        return _shelter1fVehicularAirlockStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_1f_vehicular_airlock_8017D988(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D990(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 3) {
        if (Gp_GetCurBit2Flag(6) == 2 && GameFlag_GetNibble(0x7A) >= 6) {
            arg2 = 5;
        }
        Gp_SpawnIfCapIdle(arg2, 0);
    }
    return 0;
}

s32 func_shelter_1f_vehicular_airlock_8017D9F4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// State 0 of the room's message task: parks the room's message table in
/// `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
static void func_shelter_1f_vehicular_airlock_8017D9FC(Task* task)
{
    task->msgTable = D_shelter_1f_vehicular_airlock_80182034;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's message task: does nothing.
static void func_shelter_1f_vehicular_airlock_8017DA40(Task* task)
{
}

/// The message task's three state handlers: publishing the room's message
/// table, idling and `taskKill`.
static const TaskFuncTable3 D_shelter_1f_vehicular_airlock_8017D5D8 = {
    { func_shelter_1f_vehicular_airlock_8017D9FC, func_shelter_1f_vehicular_airlock_8017DA40, taskKill },
};

/// Runs the room's message task through its three states: publishing the
/// room's message table (`func_shelter_1f_vehicular_airlock_8017D9FC`), idling
/// (`func_shelter_1f_vehicular_airlock_8017DA40`) and `taskKill`. The table is
/// copied onto the stack first, so the call goes through a local copy rather
/// than the rodata.
void func_shelter_1f_vehicular_airlock_8017DA48(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_vehicular_airlock_8017D5D8;
    sp.funcs[task->state](task);
}

void func_shelter_1f_vehicular_airlock_8017DAA0(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_80115758  = 0x601D7;
        D_8011572C  = 0x601F3;
        D_80115750  = 0x6020F;
        task->state = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* p = D_shelter_1f_vehicular_airlock_8018206C;

            func_shelter_1f_vehicular_airlock_8017DC80(&p[0], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[2], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[6], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[8], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017E468(&p[12], 0x200, 0x200);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_1f_vehicular_airlock_8018205C;

            func_shelter_1f_vehicular_airlock_8017DC80(&p[0], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[2], 0x200, 0x800, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[6], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[8], 0x200, 0, 0x210);
            func_shelter_1f_vehicular_airlock_8017DC80(&p[12], 0x200, 0, 0x111);
            if (GameFlag_GetNibble(0xB2) == 1) {
                func_shelter_1f_vehicular_airlock_8017E80C(&p[15], 0x804, 0x140, 0x21);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[16], 0xC0, 0x120, 0x210);
                func_shelter_1f_vehicular_airlock_8017E80C(&p[17], -0xC0, 0x120, 0x210);
            } else {
                func_shelter_1f_vehicular_airlock_8017E468(&p[15], 0x180, 0x21);
                func_shelter_1f_vehicular_airlock_8017E468(&p[16], 0x140, 0x210);
                func_shelter_1f_vehicular_airlock_8017E468(&p[17], 0x140, 0x210);
            }
            break;
        }
    }
}

/// Draws a gouraud capsule between the view-space points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, radius `(s16)arg1 * 64` over the end's
/// OTZ, joined by a band, all rotated by `(s16)arg2`. The inner vertices take
/// the colour coded in `arg3` (red from bits 8-15, green from bits 4-5, blue
/// from bits 0-1), each scaled by a blend byte that alternates with the frame
/// counter, and the outer rim is black. Nothing is drawn when the second
/// point's OTZ is below 0x11.
static void func_shelter_1f_vehicular_airlock_8017DC80(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                packed;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        packed    = arg3 << 16;
        blend     = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        r         = blend * (packed >> 24);
        g         = blend * ((packed >> 20) & 3);
        base      = (s16)arg2;
        b         = blend * (arg3 & 3);
        ang       = 0;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = r;
            p->g2    = g;
            prim->b2 = b;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, r, g, b);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws a fan of four gouraud quads around the view-space point `worldPoint` when
/// its OTZ is at least 0x11: the centre takes the colour coded in `packedColor` (red
/// from bits 8-15, green from bits 4-5, blue from bits 0-1), each scaled by a
/// blend byte that alternates with the frame counter, and the rim, at radius
/// `(s16)radiusScale * 64` over the OTZ, is black.
///
/// `worldPoint` uses world coordinates; `radiusScale` is narrowed to signed 16 bits
/// before division by camera depth/4. Angles use 4096 units per turn and the
/// trigonometric coordinates use a 12-bit fractional scale. Colour bytes wrap.
static void func_shelter_1f_vehicular_airlock_8017E468(SVECTOR* worldPoint, s32 radiusScale, s32 packedColor)
{
    enum {
        ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH       = 17,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE = 0x20,
        ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP = 8,
        ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT      = 12,
        ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN       = 0x1000,
    };

    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    DisplayState*      displayBase;
    DisplayState*      ds;
    s32                radius;
    s32                angle;
    s32                halfStepAngle;
    s32                nextAngle;
    s32                shiftedColor;
    u8                 brightness;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - sizeof(*block));
        block   = (RoomDraw25Scratch*)tmp;
    }

    // Project the world point before allocating its glow packets.
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(worldPoint);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - sizeof(*block)))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - sizeof(*block)))->otz >= ROOM_VISUAL_EFFECTS_GLOW_MIN_DEPTH) {
        radius        = ((s16)radiusScale * 64) / ((RoomDraw25Scratch*)(head - sizeof(*block)))->otz;
        displayBase   = &gDisplayState;
        shiftedColor  = packedColor << 16;
        brightness    = (((u8)displayBase->animFrame & 1) * ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_STEP) | ROOM_VISUAL_EFFECTS_GLOW_BRIGHTNESS_BASE;
        r             = brightness * (shiftedColor >> 24);
        g             = brightness * ((shiftedColor >> 20) & 3);
        b             = brightness * (packedColor & 3);
        angle         = 0;
        ds            = displayBase;
        block->radius = radius;
        // Build four glow wedges and quantize their shared camera depth.
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0      = block->sx + ((block->radius * rsin(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            halfStepAngle = angle + 0x200;
            prim->y0      = block->sy + ((block->radius * rcos(angle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->x1      = block->sx + ((block->radius * rsin(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y1      = block->sy + ((block->radius * rcos(halfStepAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            nextAngle     = angle + 0x400;
            prim->x2      = block->sx;
            prim->y2      = block->sy;
            prim->x3      = block->sx + ((block->radius * rsin(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            prim->y3      = block->sy + ((block->radius * rcos(nextAngle)) >> ROOM_VISUAL_EFFECTS_GLOW_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (angle < ROOM_VISUAL_EFFECTS_GLOW_FULL_TURN);
    }
    SCRATCH_POP_BYTES(sizeof(*block));
}

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, if
/// the resulting OTZ is at least 0x11, queues two gouraud `POLY_G4` diamonds
/// and two gouraud `LINE_G3` diagonals around the projected centre. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 32 / otz`. `arg1`
/// scales the display frame counter into `rsin`, giving a pulse of
/// `rsin(...) / 68 + 0x3C`; `arg3` packs the lit vertex's colour as per-channel
/// multipliers of that pulse, red in bits 8 and up, green in bits 4-5 and blue
/// in bits 0-1.
static void func_shelter_1f_vehicular_airlock_8017E80C(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u8*               head;
    RoomShaftScratch* block;
    POLY_G4*          prim;
    LINE_G3*          line;
    s32               sine;
    u8                pulse;
    s32               r;
    s32               g;
    s32               b;
    s32               radius;
    s32               i;
    s32               t1;
    s32               t2;
    s32               twice;
    u16               sx;
    u16               sy;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        head    = *scratch;
        tmp     = (*scratch = head - 0x14);
        block   = (RoomShaftScratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomShaftScratch*)(head - 0x14))->sx);
    gte_stszotz(&block->otz);
    if (((RoomShaftScratch*)(head - 0x14))->otz >= 0x11) {
        sine             = rsin(gDisplayState.animFrame * (s16)arg1);
        radius           = ((s16)arg2 * 32) / ((RoomShaftScratch*)(head - 0x14))->otz;
        pulse            = sine / 68 + 0x3C;
        r                = pulse * ((s16)arg3 >> 8);
        g                = pulse * (((s16)arg3 >> 4) & 3);
        b                = pulse * (arg3 & 3);
        i                = 0;
        block->halfWidth = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx - (u16)block->halfWidth;
            sx       = block->sx;
            prim->x2 = sx;
            prim->x1 = sx;
            prim->x3 = block->sx + (u16)block->halfWidth;
            sy       = block->sy;
            prim->y3 = sy;
            prim->y2 = sy;
            prim->y0 = sy;
            twice    = i * 2;
            prim->y1 = (block->sy - (u16)block->halfWidth) + (block->halfWidth * twice);
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
            setRGB1(line, r, g, b);
            setRGB2(line, 0, 0, 0);
            t1       = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->sx + (block->halfWidth * t1);
            line->y0 = block->sy - (block->halfWidth * t2);
            line->x1 = block->sx;
            line->y1 = block->sy;
            line->x2 = block->sx - (block->halfWidth * t1);
            line->y2 = block->sy + (block->halfWidth * t2);
            addPrim(((u_long*)((((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) + (uintptr)gGpuCurrentOt)),
                    line);
            Gp_AddTpageShift((P_TAG*)line, 1, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_POP_BYTES(0x14);
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_1f_vehicular_airlock_8017ECBC(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_1f_vehicular_airlock_8017F720(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_1f_vehicular_airlock_80180008(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
