#include "rooms/dryfield_night_main_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
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
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"

#define DRYFIELD_NIGHT_MAIN_STREET_RAND()     ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)
#define D_dryfield_night_main_street_801821B8 (D_dryfield_night_main_street_801821A8[2])
#define D_dryfield_night_main_street_801821C8 (D_dryfield_night_main_street_801821A8[4])
#define D_dryfield_night_main_street_801821D8 (D_dryfield_night_main_street_801821A8[6])
#define D_dryfield_night_main_street_801821E8 (D_dryfield_night_main_street_801821A8[8])

/// Advances the gameplay LCG and yields the high half of the new state.

/// Applies the patch list `table[GameFlag_GetNibble(nibble)]` to the current
/// area's view sprite commands. The list is a stream of byte pairs ended by a
/// 0xFF first byte: `(view, 0xFF)` selects that view's command list, and any
/// other `(cmd, value)` stores `value` in that command's `field_4`. The list
/// starts on the view named by its first byte.
#define DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(table, nibble)      \
    {                                                                   \
        GpAreaKey* sess;                                                \
        GpSprtRec* rec;                                                 \
        GpSprtCmd* cmd;                                                 \
        u8*        p;                                                   \
        s16        idx;                                                 \
        u8**       tbl;                                                 \
                                                                        \
        idx  = GameFlag_GetNibble(nibble);                              \
        tbl  = table;                                                   \
        p    = tbl[idx];                                                \
        sess = &gGameSession->at4.loc;                                  \
        rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1]; \
        cmd  = rec[p[0]].field_4;                                       \
        if (p[0] != 0xFF) {                                             \
            do {                                                        \
                if (p[1] == 0xFF) {                                     \
                    cmd = rec[p[0]].field_4;                            \
                    p  += 2;                                            \
                }                                                       \
                cmd[p[0]].field_4 = p[1];                               \
                p                += 2;                                  \
            } while (p[0] != 0xFF);                                     \
        }                                                               \
    }

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc D_dryfield_night_main_street_8018208C;

/// Descriptor of the event task the event gate spawns.
extern TaskDesc D_dryfield_night_main_street_80182098;

/// Descriptor of the task `func_dryfield_night_main_street_8017DE78` runs as.
extern TaskDesc D_dryfield_night_main_street_801820A4;

/// Message table installed at `Task::msgTable` by the room task's state 0
/// (ids `0x13EE`-`0x13F1`).
extern GpMsgEntry D_dryfield_night_main_street_801820B0[];

/// Per-nibble-value sprite patch lists, one table per game-flag nibble.
extern u8** D_dryfield_night_main_street_80182168;
extern u8** D_dryfield_night_main_street_8018216C;
extern u8** D_dryfield_night_main_street_80182170;
extern u8** D_dryfield_night_main_street_80182174;

/// The room effect mode of each view, indexed by view - 1.
extern u16 D_dryfield_night_main_street_80182178[];

/// Anchors of the room task's glows; the entries after the first are also
/// reached by their own names, and entry 16 is the spawn position scratch.

/// View masks of the room task's anchors.
extern s32 D_dryfield_night_main_street_80182230[];

/// Per-tint channel shifts for the halo task, indexed by the tint the spawn
/// argument selects.
// The retained trailing word is present in the exported data after the three shades.
typedef struct {
    RoomHaloShade shades[3];
    u16           trailing;
} NightMainStreetHaloShades;
STATIC_ASSERT_SIZEOF(NightMainStreetHaloShades, 20);

extern NightMainStreetHaloShades D_dryfield_night_main_street_80182270;

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage D_dryfield_night_main_street_80188BA4;

/// The message and event the message handler latched for the room's event
/// task, and the flag it raises once it has spawned that task.
extern RoomEventMsg D_dryfield_night_main_street_80188BAC;
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    s8 value;
    u8 retained[7];
} DryfieldNightMainStreetStorage8BB4;
STATIC_ASSERT_SIZEOF(DryfieldNightMainStreetStorage8BB4, 8);

extern DryfieldNightMainStreetStorage8BB4 D_dryfield_night_main_street_80188BB4;
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    RoomLatchedEvent value;
    u8               retained[4];
} DryfieldNightMainStreetStorage8BC8;
STATIC_ASSERT_SIZEOF(DryfieldNightMainStreetStorage8BC8, 16);

extern DryfieldNightMainStreetStorage8BC8 D_dryfield_night_main_street_80188BC8;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg D_dryfield_night_main_street_80188BBC;
extern RoomEventReq D_dryfield_night_main_street_80188BD8;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_night_main_street_80188BC4;

static void func_dryfield_night_main_street_8017E064(Task* arg0);
static void func_dryfield_night_main_street_8017E0B8(Task* task);
static void func_dryfield_night_main_street_8017E118(void);
static void func_dryfield_night_main_street_8017E940(SVECTOR* arg0, s32 arg1);
static void func_dryfield_night_main_street_8017F128(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_dryfield_night_main_street_8017F608(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_dryfield_night_main_street_8017FD34(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void func_dryfield_night_main_street_80180CF4(GfxCoord* coord, s16 size);
static void func_dryfield_night_main_street_80181220(GfxCoord* arg0, s32 arg1);
static void func_dryfield_night_main_street_80181598(GfxCoord* arg0, s16 arg1, u8* arg2);

s32  func_dryfield_night_main_street_8017DA6C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_night_main_street_8017DEF0(Task*, s32, s32, s32);
s32  func_dryfield_night_main_street_8017DFC8(Task*, s32, s32, GpMessageArg);
s32  func_dryfield_night_main_street_8017E054(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_dryfield_night_main_street_8017E05C(Task*, s32, GpMessageArg, GpMessageArg);
void func_dryfield_night_main_street_8017D600(Task*);
void func_dryfield_night_main_street_8017D8FC(Task*);
void func_dryfield_night_main_street_8017DE78(Task*);

extern GpGridParams D_dryfield_night_main_street_801833D0[1];
extern GpGridParams D_dryfield_night_main_street_80184540[1];
extern GpObj4C      D_dryfield_night_main_street_80187704[26];
extern GpObj4C      D_dryfield_night_main_street_80187EBC[12];

extern GpRoomBoundVec D_dryfield_night_main_street_80188A70[25];
extern GpRoomCoordSet D_dryfield_night_main_street_8018899C[1];

extern GpSprtCmd  D_dryfield_night_main_street_801848C4[2];
extern GpSprtCmd  D_dryfield_night_main_street_80184CD0[8];
extern GpSprtCmd  D_dryfield_night_main_street_80184F18[3];
extern GpSprtCmd  D_dryfield_night_main_street_80185084[3];
extern GpSprtCmd  D_dryfield_night_main_street_801852B8[3];
extern GpSprtCmd  D_dryfield_night_main_street_801854D8[6];
extern GpSprtCmd  D_dryfield_night_main_street_80185774[5];
extern GpSprtCmd  D_dryfield_night_main_street_8018579C[2];
extern GpSprtCmd  D_dryfield_night_main_street_801857AC[2];
extern GpSprtCmd  D_dryfield_night_main_street_801857BC[2];
extern GpSprtCmd  D_dryfield_night_main_street_801857CC[2];
extern GpSprtCmd  D_dryfield_night_main_street_801857DC[2];
extern GpSprtCmd  D_dryfield_night_main_street_80185AA8[6];
extern GpSprtCmd  D_dryfield_night_main_street_80185FB0[10];
extern GpSprtCmd  D_dryfield_night_main_street_8018626C[7];
extern GpSprtCmd  D_dryfield_night_main_street_801864C0[3];
extern GpSprtCmd  D_dryfield_night_main_street_80186834[8];
extern GpSprtCmd  D_dryfield_night_main_street_80186950[3];
extern GpSprtCmd  D_dryfield_night_main_street_80186968[2];
extern GpSprtCmd  D_dryfield_night_main_street_80186978[2];
extern GpSprtCmd  D_dryfield_night_main_street_80186C94[7];
extern GpSprtCmd  D_dryfield_night_main_street_801870F0[7];
extern GpSprtCmd  D_dryfield_night_main_street_8018759C[7];
extern GpSprtElem D_dryfield_night_main_street_801848D4[51];
extern GpSprtElem D_dryfield_night_main_street_80184D10[26];
extern GpSprtElem D_dryfield_night_main_street_80184F30[17];
extern GpSprtElem D_dryfield_night_main_street_8018509C[27];
extern GpSprtElem D_dryfield_night_main_street_801852D0[26];
extern GpSprtElem D_dryfield_night_main_street_80185508[31];
extern GpSprtElem D_dryfield_night_main_street_801857EC[35];
extern GpSprtElem D_dryfield_night_main_street_80185AD8[62];
extern GpSprtElem D_dryfield_night_main_street_80186000[31];
extern GpSprtElem D_dryfield_night_main_street_801862A4[27];
extern GpSprtElem D_dryfield_night_main_street_801864D8[43];
extern GpSprtElem D_dryfield_night_main_street_80186874[11];
extern GpSprtElem D_dryfield_night_main_street_80186988[39];
extern GpSprtElem D_dryfield_night_main_street_80186CCC[53];
extern GpSprtElem D_dryfield_night_main_street_80187128[57];
extern TaskDesc   D_8014D8A4;

TaskDesc D_dryfield_night_main_street_8018208C = { 0, 32, func_dryfield_night_main_street_8017D600, { .model = NULL } };

TaskDesc D_dryfield_night_main_street_80182098 = { 0, 32, func_dryfield_night_main_street_8017D8FC, { .model = NULL } };

TaskDesc D_dryfield_night_main_street_801820A4 = { 0, 32, func_dryfield_night_main_street_8017DE78, { .model = NULL } };

GpMsgEntry D_dryfield_night_main_street_801820B0[6] = {
    { 5102, func_dryfield_night_main_street_8017DA6C },
    { 5105, func_dryfield_night_main_street_8017E054 },
    { 5103, func_dryfield_night_main_street_8017E05C },
    { 5104, func_dryfield_night_main_street_8017DFC8 },
    { 5106, func_dryfield_night_main_street_8017DEF0 },
    { 0x7FFFFFFF, NULL },
};

u8 D_dryfield_night_main_street_801820E0[16] = {
    13,
    255,
    6,
    0,
    14,
    255,
    3,
    0,
    16,
    255,
    5,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_main_street_801820F0[16] = {
    13,
    255,
    6,
    1,
    14,
    255,
    3,
    1,
    16,
    255,
    5,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_main_street_80182100[2] = {
    D_dryfield_night_main_street_801820E0,
    D_dryfield_night_main_street_801820F0,
};

u8 D_dryfield_night_main_street_80182108[16] = {
    13,
    255,
    7,
    0,
    14,
    255,
    4,
    0,
    16,
    255,
    6,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_main_street_80182118[16] = {
    13,
    255,
    7,
    1,
    14,
    255,
    4,
    1,
    16,
    255,
    6,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_main_street_80182128[2] = {
    D_dryfield_night_main_street_80182108,
    D_dryfield_night_main_street_80182118,
};

u8 D_dryfield_night_main_street_80182130[12] = {
    13,
    255,
    8,
    0,
    14,
    255,
    5,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_main_street_8018213C[12] = {
    13,
    255,
    8,
    1,
    14,
    255,
    5,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_main_street_80182148[2] = {
    D_dryfield_night_main_street_80182130,
    D_dryfield_night_main_street_8018213C,
};

u8 D_dryfield_night_main_street_80182150[8] = {
    12,
    255,
    4,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_main_street_80182158[8] = {
    12,
    255,
    4,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_main_street_80182160[2] = {
    D_dryfield_night_main_street_80182150,
    D_dryfield_night_main_street_80182158,
};

u8** D_dryfield_night_main_street_80182168 = D_dryfield_night_main_street_80182100;

u8** D_dryfield_night_main_street_8018216C = D_dryfield_night_main_street_80182128;

u8** D_dryfield_night_main_street_80182170 = D_dryfield_night_main_street_80182148;

u8** D_dryfield_night_main_street_80182174 = D_dryfield_night_main_street_80182160;

u16 D_dryfield_night_main_street_80182178[24] = {
    0,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    0,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    2,
};

// Indexed views below share one contiguous table.
SVECTOR D_dryfield_night_main_street_801821A8[17] = {
    { -930, -2870, 10010, 0 },
    { -2130, -2870, 10010, 0 },
    { -6010, -2870, 3050, 0 },
    { -6010, -2870, 1850, 0 },
    { -6010, -2870, -2940, 0 },
    { -6010, -2870, -4150, 0 },
    { -160, -3100, 8360, 0 },
    { -160, -2800, 8790, 0 },
    { -300, -3250, -4100, 0 },
    { -300, -3190, -4610, 0 },
    { -4140, -1700, 10930, 0 },
    { -6910, -1700, 10650, 0 },
    { -6910, -1700, 6660, 0 },
    { -6910, -1700, -3150, 0 },
    { -3150, -4850, 10920, 0 },
    { -6930, -4850, 5150, 0 },
    { -850, -1320, 9770, 0 },
};

s32 D_dryfield_night_main_street_80182230[16] = {
    0x2C858,
    0,
    1096,
    64,
    0x1042484,
    0x40080,
    0xC818,
    0xC818,
    0x1E42084,
    0x1E02004,
    0x1C838,
    0x3C078,
    0x3C078,
    0x1043084,
    0x2C018,
    0x4008,
};

NightMainStreetHaloShades D_dryfield_night_main_street_80182270 = { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0xE0DC };

GpRoomObjRec D_dryfield_night_main_street_80182284[3] = {
    { D_dryfield_night_main_street_801833D0, D_dryfield_night_main_street_80187704, D_dryfield_night_main_street_80187EBC, NULL },
    { D_dryfield_night_main_street_80184540, D_dryfield_night_main_street_80187704, D_dryfield_night_main_street_8018824C, NULL },
    { D_dryfield_night_main_street_80184540, D_dryfield_night_main_street_80187704, D_dryfield_night_main_street_8018824C, NULL },
};

GpRoomCoordRec D_dryfield_night_main_street_801822B4[3] = {
    { D_dryfield_night_main_street_8018899C, D_dryfield_night_main_street_80188A70 },
    { D_dryfield_night_main_street_8018899C, D_dryfield_night_main_street_80188A70 },
    { D_dryfield_night_main_street_8018899C, D_dryfield_night_main_street_80188A70 },
};

u8 D_dryfield_night_main_street_801822CC[24] = {
    1,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    22,
    21,
    23,
    24,
};

u8 D_dryfield_night_main_street_801822E4[24] = {
    1,
    24,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    23,
    21,
    23,
    24,
};

u8* D_dryfield_night_main_street_801822FC[3] = {
    D_8010CAF8,
    D_dryfield_night_main_street_801822CC,
    D_dryfield_night_main_street_801822E4,
};

GpViewCountRec D_dryfield_night_main_street_80182308[3] = {
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
    { { .bytes = { 24, 0 } } },
};

GpWarpRec D_dryfield_night_main_street_80182310[7] = {
    { { .words = { 3072, -450, 0, -3435 } }, { 0, 0, 0, 0 }, { .words = { 3072, -450, 0, -3435 } }, { 0, 0, 0, 0 }, 0x53020011, 0x53020010, 0, 21, 0, 488 },
    { { .words = { 1024, -6588, 0, -2500 } }, { 0, 0, 0, 0 }, { .words = { 1024, -6588, 0, -2500 } }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 7, 0, 487 },
    { { .words = { 1024, -6673, 0, 2155 } }, { 0, 0, 0, 0 }, { .words = { 1024, -6673, 0, 2155 } }, { 0, 0, 0, 0 }, 0x53020004, 0x53020003, 0x53020007, 6, 0, 486 },
    { { .words = { 1024, -6588, 0, 5900 } }, { 0, 0, 0, 0 }, { .words = { 1024, -6588, 0, 5900 } }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 5, 0, 485 },
    { { .words = { 1024, -6652, 0, 0x2956 } }, { 0, 0, 0, 0 }, { .words = { 1024, -6652, 0, 0x2956 } }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 5, 0, 484 },
    { { .words = { 2048, -3478, 0, 0x29B5 } }, { 0, 0, 0, 0 }, { .words = { 2048, -3478, 0, 0x29B5 } }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 4, 0, 483 },
    { { .words = { 3072, -488, 0, 8495 } }, { 0, 0, 0, 0 }, { .words = { 3072, -488, 0, 8495 } }, { 0, 0, 0, 0 }, 0x53020002, 0x53020001, 0, 9, 0, 482 },
};

SVECTOR D_dryfield_night_main_street_80182498[20] = {
#include "assets/dryfield_night_main_street_collision_05E10_normals.inc"
};

SVECTOR D_dryfield_night_main_street_80182538[180] = {
#include "assets/dryfield_night_main_street_collision_05E10_verts.inc"
};

GpGridFace D_dryfield_night_main_street_80182AD8[85] = {
#include "assets/dryfield_night_main_street_collision_05E10_faces.inc"
};

s16 D_dryfield_night_main_street_80182ED4[554] = {
#include "assets/dryfield_night_main_street_collision_05E10_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_main_street_80182ED4[i])
s16* D_dryfield_night_main_street_80183328[42] = {
#include "assets/dryfield_night_main_street_collision_05E10_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_main_street_801833D0[1] = {
    { NULL, D_dryfield_night_main_street_80182498, D_dryfield_night_main_street_80182538, D_dryfield_night_main_street_80182AD8, D_dryfield_night_main_street_80183328, 0x2EE6, 0x2C4C, 6, 7, 4000, 85 },
};

SVECTOR D_dryfield_night_main_street_801833F4[23] = {
#include "assets/dryfield_night_main_street_collision_06F80_normals.inc"
};

SVECTOR D_dryfield_night_main_street_801834AC[196] = {
#include "assets/dryfield_night_main_street_collision_06F80_verts.inc"
};

GpGridFace D_dryfield_night_main_street_80183ACC[89] = {
#include "assets/dryfield_night_main_street_collision_06F80_faces.inc"
};

s16 D_dryfield_night_main_street_80183EF8[720] = {
#include "assets/dryfield_night_main_street_collision_06F80_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_main_street_80183EF8[i])
s16* D_dryfield_night_main_street_80184498[42] = {
#include "assets/dryfield_night_main_street_collision_06F80_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_night_main_street_80184540[1] = {
    { NULL, D_dryfield_night_main_street_801833F4, D_dryfield_night_main_street_801834AC, D_dryfield_night_main_street_80183ACC, D_dryfield_night_main_street_80184498, 0x2EE6, 0x2C4C, 6, 7, 4000, 89 },
};

GpViewRec D_dryfield_night_main_street_80184564[24] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 4112, 0x6D60, -2748 } }, 289 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
    { { { { 3809, 0, 1504 }, { 102, 4086, -259 }, { -1500, 279, 3800 } }, { 1205, 1259, 2882 } }, 230 },
    { { { { 3962, 0, 1037 }, { 65, 4087, -250 }, { -1035, 258, 3954 } }, { 1414, 1228, -1924 } }, 230 },
    { { { { 3997, 0, -893 }, { -382, 3702, -1708 }, { 807, 1750, 3613 } }, { 6425, 2237, -3705 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6489, 989, 2493 } }, 230 },
    { { { { -3989, 0, -928 }, { 63, 4086, -271 }, { 926, -278, -3980 } }, { 6440, 837, -1624 } }, 230 },
    { { { { 1028, 0, -3964 }, { -2496, 3181, -647 }, { 3079, 2579, 798 } }, { 2261, 1919, -9779 } }, 230 },
    { { { { 3973, 0, -993 }, { -593, 3285, -2373 }, { 796, 2446, 3187 } }, { 1213, 2874, -5701 } }, 230 },
    { { { { -3415, 0, 2261 }, { -341, 4049, -515 }, { -2235, -618, -3376 } }, { 173, 317, -8972 } }, 230 },
    { { { { 4058, 0, -551 }, { 57, 4073, 425 }, { 548, -429, 4036 } }, { 1906, 617, -5254 } }, 230 },
    { { { { -3455, 0, 2198 }, { -65, 4094, -103 }, { -2197, -122, -3454 } }, { 4039, 880, 1740 } }, 230 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
    { { { { 3809, 0, 1504 }, { 102, 4086, -259 }, { -1500, 279, 3800 } }, { 1205, 1259, 2882 } }, 230 },
    { { { { 3962, 0, 1037 }, { 65, 4087, -250 }, { -1035, 258, 3954 } }, { 1414, 1228, -1924 } }, 230 },
    { { { { 3997, 0, -893 }, { -382, 3702, -1708 }, { 807, 1750, 3613 } }, { 6425, 2237, -3705 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6489, 989, 2493 } }, 230 },
    { { { { -3989, 0, -928 }, { 63, 4086, -271 }, { 926, -278, -3980 } }, { 6440, 837, -1624 } }, 230 },
    { { { { 1028, 0, -3964 }, { -2496, 3181, -647 }, { 3079, 2579, 798 } }, { 2261, 1919, -9779 } }, 230 },
    { { { { 3823, 0, -1469 }, { -867, 3305, -2257 }, { 1186, 2418, 3085 } }, { 1630, 2840, -5810 } }, 230 },
    { { { { -4089, 0, -223 }, { 23, 4073, -429 }, { 222, -430, -4067 } }, { 1450, 640, 520 } }, 230 },
    { { { { -4089, 0, -223 }, { 23, 4073, -429 }, { 222, -430, -4067 } }, { 1450, 640, 520 } }, 230 },
    { { { { -4089, 0, -223 }, { 23, 4073, -429 }, { 222, -430, -4067 } }, { 1450, 640, 520 } }, 230 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
};

GpSprtCmd D_dryfield_night_main_street_801848C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_801848D4[51] = {
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -160, -120, 1250, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, -160, -16, 1250, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 152 } }, -72, -120, 2350, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -112, -120, 2250, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 80 } }, -160, -24, 1250, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, -24, 1750, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -112, -24, 1875, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -104, -24, 2350, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, 16, 2375, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -24, 1131, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, -16, 2375, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 16, -16, 2500, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -16, 1133, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -8, 1136, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 2375, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 0, 1138, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -152, 8, 2375, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 1091, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 16, 1112, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -160, -32, 2375, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -136, -24, 2250, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 0, 2375, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 16, 2375, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -112, -24, 2375, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -96, -32, 2375, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -88, 0, 2375, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, -16, 2375, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -32, 2375, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -40, 8, 2375, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -24, 2500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 0, -24, 2500, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 2500, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -24, 2500, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -80, 2250, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 16, -72, 2275, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -88, 2250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 152 } }, 56, -120, 2344, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -8, 2425, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 152 } }, 96, -120, 2300, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -96, 1294, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 104, -112, 1237, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, 112, -112, 1190, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 96, 0, 1339, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 104, 0, 1311, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, 120, -112, 1166, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, 128, -112, 1128, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, 136, -120, 1130, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 144, -120, 1094, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 152, -120, 1006, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 144, 8, 1039, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 88, 32, 1250, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80184CD0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 7, 0, 0, { 0, 0 } },
    { 9, 24, 0, 0, { 5, 0 } },
    { 33, 6, 0, 0, { 1, 0 } },
    { 39, 11, 0, 0, { 4, 0 } },
    { 50, 1, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80184D10[26] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 920, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 1006, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, -120, 1060, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, -80, 931, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, -80, 936, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, -80, 991, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, -8, 938, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, -8, 967, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, -8, 1000, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, -8, 1088, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -8, 1117, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -8, 1132, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 16, 1142, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -32, -80, 2087, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -24, -72, 2128, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -16, -72, 2165, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -8, 2002, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, -8, 2138, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -8, 2178, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -56, 3120, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -56, 3144, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -56, 3099, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 3102, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -64, 2647, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, -16, 2685, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 48, 696, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80184F18[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80184F30[17] = {
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -160, -120, 960, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -120, 998, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, -120, 1037, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, -8, 968, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -152, -8, 1071, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -8, 1089, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 32, 1107, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -88, 1943, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -80, 1928, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -8, 1842, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, -8, 1952, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -48, -80, 1950, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -8, 1970, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -80, 1907, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, -88, 1620, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, -56, 1623, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 72, -8, 1706, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80185084[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_8018509C[27] = {
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -8, -120, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, -8, 1475, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -56, 1489, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -56, 1482, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 120, -120, 329, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -72, 366, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -72, 322, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, 24, 612, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 24, 600, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 24, 500, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 48, 625, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 48, 600, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 48, 500, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 64, 650, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, 64, 600, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 64, 500, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 80, 690, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 80, 675, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 80, 675, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, 80, 650, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 80, 650, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, -72, 1412, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, -40, 1475, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 0, 600, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 0, 550, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -40, 550, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 500, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_801852B8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_801852D0[26] = {
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -24, -64, 1801, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, -72, 1617, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, -80, 1450, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, -88, 1314, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, -96, 1201, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, -104, 1106, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, -112, 1026, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, -120, 956, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -40, -48, 2867, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 0, 2867, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -120, 750, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 224 } }, 48, -120, 775, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 112, 550, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 104, 0, 512, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 0, 512, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 88, 512, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 0, 797, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 40, 0, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 24, 875, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 48, 873, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 224 } }, 64, -120, 700, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 80, -120, 600, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 232 } }, 88, -120, 525, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 568, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 16, 1791, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 120, 56, 623, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_801854D8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 15, 0, 0, { 0, 0 } },
    { 23, 3, 0, 0, { 2, 0 } },
    { 26, 0, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80185508[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 0, 2025, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 64 } }, -80, -24, 2025, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 8, 2041, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -24, -24, 2025, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, 24, -24, 2025, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 32, 2000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 88, 2000, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, -80, 1875, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -152, -120, 2004, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 200 } }, -128, -104, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 136 } }, -160, -16, 358, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 128 } }, -136, -8, 402, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 240 } }, -128, -120, 468, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -88, -120, 565, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -72, -120, 574, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -16, 581, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 112 } }, -72, 0, 582, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 104 } }, -64, 0, 642, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 96 } }, -56, 0, 669, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 128, 16 } }, -48, -120, 1053, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 16 } }, -40, -104, 1206, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 16 } }, -24, -88, 1221, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -16, -72, 1400, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -8, -64, 1678, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, -56, 1653, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 0, -48, 1750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 16, -16, 1750, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 24, 8, 1750, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 40, 48, 1750, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 120 } }, 72, -56, 1750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 184 } }, 80, -120, 1750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80185774[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 5, 0, 0, { 2, 0 } },
    { 10, 21, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_8018579C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_801857AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_801857BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_801857CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_801857DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_801857EC[35] = {
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 16, -72, 2325, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 24, -72, 2300, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -72, 1915, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 32, -72, 2315, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 2205, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -64, 2575, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -80, 2575, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -120, 2575, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 64, -120, 2575, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 0, 2575, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -8, 2575, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 56, -32, 2576, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 64, -40, 2575, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -96, 1294, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -104, 1237, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, 112, -104, 1190, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 104, -24, 1310, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, 0, 1339, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 32, 1499, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, 120, -112, 1165, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, 128, -112, 1128, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, 136, -112, 1128, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 144, 8, 1068, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 152, 8, 1019, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, -120, 1043, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 152, -120, 1006, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 16, -72, 2379, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, -72, 2196, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, -80, 2033, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 40, -80, 1895, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 48, -88, 1773, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 56, -88, 1666, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 64, -96, 1571, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 72, -104, 1487, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 80, -104, 1411, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80185AA8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 3, 0 } },
    { 5, 8, 0, 0, { 0, 0 } },
    { 13, 13, 0, 0, { 2, 0 } },
    { 26, 9, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80185AD8[62] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -104, 1184, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -160, -120, 932, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -152, -120, 939, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -144, -120, 988, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -136, -120, 1024, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -128, -120, 1163, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -120, -120, 1378, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, -112, 1532, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -88, 1824, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -144, -40, 1001, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -136, -8, 1088, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -8, 1117, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -120, -8, 1137, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -72, 2467, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -40, 2814, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 2075, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -8, 2075, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -32, -80, 2119, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -24, -72, 2138, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -16, -72, 2174, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 8, 1987, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -72, 2130, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, -64, 2524, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, -56, 2626, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, -48, 2564, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -56, 3385, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 8, -56, 3124, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 16, -56, 3148, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -16, 3103, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -56, 3099, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, -56, 3470, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 16, 1830, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 16, 1972, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 16, 1979, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 24, 1506, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 24, 1506, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 24, 1544, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 24, 1591, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 24, 1556, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 24, 1625, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 24, 1625, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 24, 1625, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 24, 1625, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 32, 1286, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 32, 1335, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 32, 1341, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 32, 1379, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 32, 1379, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 32, 1379, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 32, 1405, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 32, 1433, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 40, 1155, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 40, 1149, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 40, 1148, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 40, 1207, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 24 } }, 64, -72, 2716, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 16 } }, 16, -64, 2943, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -16, -80, 2182, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -8, -80, 2365, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 0, -72, 2582, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, -72, 2842, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 16, -64, 2995, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80185FB0[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 6, 0 } },
    { 13, 8, 0, 0, { 1, 0 } },
    { 21, 4, 0, 0, { 4, 0 } },
    { 25, 6, 0, 0, { 0, 0 } },
    { 31, 24, 0, 0, { 5, 0 } },
    { 55, 1, 0, 0, { 3, 0 } },
    { 56, 1, 0, 0, { 7, 0 } },
    { 57, 5, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80186000[31] = {
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -160, -120, 1000, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -152, -8, 1071, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -8, 1089, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 32, 1151, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -120, 998, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -144, -120, 1037, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -8, 1956, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -88, 1563, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -88, 1633, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -64, -88, 1952, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -56, -80, 1936, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -48, -80, 2002, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -80, 1907, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -80, 1882, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -80, 1842, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -8, 1958, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 8, 1975, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 24 } }, 48, -104, 1634, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 24, -96, 1713, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 24 } }, -40, -96, 1799, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -136, -120, 1084, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -128, -120, 1125, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -120, -120, 1180, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -112, -120, 1219, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -104, -120, 1279, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -96, -120, 1346, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -88, -120, 1420, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -80, -112, 1503, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -72, -104, 1598, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -64, -104, 1704, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -56, -96, 1825, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_8018626C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 11, 0, 0, { 3, 0 } },
    { 17, 2, 0, 0, { 2, 0 } },
    { 19, 1, 0, 0, { 4, 0 } },
    { 20, 11, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_801862A4[27] = {
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -8, -120, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, -8, 1475, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -56, 1489, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -56, 1482, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 120, -120, 329, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -72, 366, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -72, 322, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, 24, 612, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 24, 600, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 24, 500, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 48, 625, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 48, 600, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 48, 500, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 64, 650, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, 64, 600, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 64, 500, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 80, 690, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 80, 675, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 80, 675, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, 80, 650, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 80, 650, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, -72, 1412, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, -40, 1475, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 0, 600, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 0, 550, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -40, 550, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 500, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_801864C0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_801864D8[43] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -40, 2877, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -32, -48, 2877, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 0, 2877, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -48, 2904, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -24, -72, 1800, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 24, 1786, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -16, -72, 1801, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -8, -72, 1810, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 0, -8, 1800, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 24, 1861, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 24, 1774, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -120, 667, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 64, 821, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 72, 591, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 8, 873, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 56, 873, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 32, 0, 800, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 40, -24, 756, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 216 } }, 48, -120, 775, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 224 } }, 56, -120, 750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 64, -120, 725, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, 72, -120, 700, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, 80, -120, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, 88, -120, 600, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, 96, -120, 491, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, 104, -48, 462, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 112, -8, 464, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 120, 0, 467, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 8, 464, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 128, 72, 462, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 72, 594, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, -120, 1063, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -56, -120, 1080, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, -120, 1061, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, -112, 1112, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, -104, 1228, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -96, 1333, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -88, 1356, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -88, -120, 1043, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 16, -56, 3564, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 16, -48, 3056, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 40, -48, 3135, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -8, -56, 2979, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80186834[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 7, 0, 0, { 0, 0 } },
    { 11, 20, 0, 0, { 5, 0 } },
    { 31, 8, 0, 0, { 1, 0 } },
    { 39, 3, 0, 0, { 4, 0 } },
    { 42, 1, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80186874[11] = {
    { 143, 0x3FC0, { .fields = { 24, 136 } }, -160, -16, 358, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -136, -8, 402, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 240 } }, -128, -120, 468, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -88, -120, 565, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -72, -120, 574, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -72, -8, 582, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -64, -8, 592, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -56, -8, 669, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 48, 669, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 64, 706, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 64, 706, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80186950[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_80186968[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_80186978[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80186988[39] = {
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -160, -72, 689, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -144, -72, 705, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, -136, -72, 731, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -128, -64, 766, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, -120, -48, 785, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, -112, -48, 835, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -40, 888, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -96, -40, 969, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -88, -40, 1029, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -80, -32, 1116, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -72, -32, 1151, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -64, -32, 1214, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, -24, 1269, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -48, -120, 1625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -120, 1625, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -8, 1625, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, 8, -120, 1625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -40, 1675, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 1675, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 48, 1675, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 64 } }, -160, -24, 1675, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -120, -32, 1675, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 72 } }, -96, -32, 1675, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 88 } }, -40, -32, 1675, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 8, -32, 1675, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, -8, 1675, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 8, 8, 1675, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 48, -40, 1675, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, 64, -32, 1675, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 88 } }, 128, -32, 1675, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, 40, 1675, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -24, 32, 1675, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 8, 24, 1675, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, 32, 1675, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 40, 1675, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 64, 687, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -112, 48, 687, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -144, 40, 687, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 48, 687, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_80186C94[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 4, 0, 0, { 3, 0 } },
    { 17, 13, 0, 0, { 2, 0 } },
    { 30, 5, 0, 0, { 4, 0 } },
    { 35, 4, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80186CCC[53] = {
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -160, -72, 589, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -144, -72, 605, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, -136, -72, 631, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -128, -64, 666, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, -120, -48, 685, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -112, -48, 736, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -40, 763, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -96, -40, 794, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -88, -40, 854, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -80, -32, 916, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -72, -32, 951, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -64, -32, 1064, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, -24, 1169, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -48, -120, 1637, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -120, 1625, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -56, -8, 1625, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -120, 1625, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -8, -120, 1625, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 0, -80, 1625, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 8, -32, 1625, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 24, 1625, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -16, 1675, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 1700, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 0, 1700, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 24, 1675, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 40, 1675, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 1675, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 1675, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, -24, 1675, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -8, 1675, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -160, 16, 1675, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 32, 1675, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -120, -32, 1675, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 8, 1675, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -104, -32, 1675, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -104, -8, 1675, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, 8, 1675, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 16, 1675, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -104, 32, 1675, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -40, -24, 1675, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -16, 1675, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, 0, 1700, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 24, 8, 1700, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 40, 24, 1700, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 8, 1700, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 104, 8, 1700, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 128, 8, 1700, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 16, 1700, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 88, 1700, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 88, 1700, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 32 } }, -24, 24, 1700, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 64 } }, 56, 24, 1700, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 136, 24, 1700, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_801870F0[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 8, 0, 0, { 3, 0 } },
    { 21, 26, 0, 0, { 2, 0 } },
    { 47, 6, 0, 0, { 4, 0 } },
    { 53, 0, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_night_main_street_80187128[57] = {
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -160, -72, 639, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, -144, -72, 655, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, -136, -72, 681, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -128, -64, 716, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, -120, -48, 785, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -112, -48, 836, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -40, 888, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -96, -40, 969, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -88, -40, 1004, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -80, -32, 1041, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -72, -32, 1076, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -64, -32, 1139, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -56, -24, 1219, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -56, -120, 1625, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -48, -120, 1625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -56, -8, 1625, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, -120, 1625, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -8, -120, 1625, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, 0, -80, 1625, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 8, -32, 1625, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 16, 24, 1625, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -16, 1700, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 1700, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, 0, 1700, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 8, 1700, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -48, 24, 1700, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 40, 1700, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 1700, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 48, 1700, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, -24, 1700, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, -8, 1700, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 16, 1700, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 32, 1700, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 88 } }, -120, -32, 1700, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -104, -32, 1700, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -104, -8, 1700, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -104, 8, 1700, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -104, 32, 1700, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -72, 16, 1700, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -40, -24, 1700, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -16, -16, 1700, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, 0, 1700, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 24, 8, 1700, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 40, 24, 1700, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 48 } }, 88, 8, 1700, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 16, 1675, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -24, 24, 1675, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, 32, 1675, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 24, 24, 1675, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, 56, 24, 1675, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 128, 24, 1675, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -32, 1675, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -160, -24, 1675, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -128, -24, 1675, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -104, 0, 1675, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, -88, 0, 1675, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, 8, 1675, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_night_main_street_8018759C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 8, 0, 0, { 3, 0 } },
    { 21, 24, 0, 0, { 2, 0 } },
    { 45, 6, 0, 0, { 4, 0 } },
    { 51, 6, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_night_main_street_801875D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_main_street_801875E4[24] = {
    { { .empty = D_dryfield_night_main_street_801848C4 }, D_dryfield_night_main_street_801848C4, NULL },
    { { .elements = D_dryfield_night_main_street_801848D4 }, D_dryfield_night_main_street_80184CD0, NULL },
    { { .elements = D_dryfield_night_main_street_80184D10 }, D_dryfield_night_main_street_80184F18, NULL },
    { { .elements = D_dryfield_night_main_street_80184F30 }, D_dryfield_night_main_street_80185084, NULL },
    { { .elements = D_dryfield_night_main_street_8018509C }, D_dryfield_night_main_street_801852B8, NULL },
    { { .elements = D_dryfield_night_main_street_801852D0 }, D_dryfield_night_main_street_801854D8, NULL },
    { { .elements = D_dryfield_night_main_street_80185508 }, D_dryfield_night_main_street_80185774, NULL },
    { { .empty = D_dryfield_night_main_street_8018579C }, D_dryfield_night_main_street_8018579C, NULL },
    { { .empty = D_dryfield_night_main_street_801857AC }, D_dryfield_night_main_street_801857AC, NULL },
    { { .empty = D_dryfield_night_main_street_801857BC }, D_dryfield_night_main_street_801857BC, NULL },
    { { .empty = D_dryfield_night_main_street_801857CC }, D_dryfield_night_main_street_801857CC, NULL },
    { { .empty = D_dryfield_night_main_street_801857DC }, D_dryfield_night_main_street_801857DC, NULL },
    { { .elements = D_dryfield_night_main_street_801857EC }, D_dryfield_night_main_street_80185AA8, NULL },
    { { .elements = D_dryfield_night_main_street_80185AD8 }, D_dryfield_night_main_street_80185FB0, NULL },
    { { .elements = D_dryfield_night_main_street_80186000 }, D_dryfield_night_main_street_8018626C, NULL },
    { { .elements = D_dryfield_night_main_street_801862A4 }, D_dryfield_night_main_street_801864C0, NULL },
    { { .elements = D_dryfield_night_main_street_801864D8 }, D_dryfield_night_main_street_80186834, NULL },
    { { .elements = D_dryfield_night_main_street_80186874 }, D_dryfield_night_main_street_80186950, NULL },
    { { .empty = D_dryfield_night_main_street_80186968 }, D_dryfield_night_main_street_80186968, NULL },
    { { .empty = D_dryfield_night_main_street_80186978 }, D_dryfield_night_main_street_80186978, NULL },
    { { .elements = D_dryfield_night_main_street_80186988 }, D_dryfield_night_main_street_80186C94, NULL },
    { { .elements = D_dryfield_night_main_street_80186CCC }, D_dryfield_night_main_street_801870F0, NULL },
    { { .elements = D_dryfield_night_main_street_80187128 }, D_dryfield_night_main_street_8018759C, NULL },
    { { .elements = D_dryfield_night_main_street_801857EC }, D_dryfield_night_main_street_80185AA8, NULL },
};

GpObj4C D_dryfield_night_main_street_80187704[26] = {
    { NULL, NULL, NULL, { -2385, -3392, 351, 0 }, { { -2863, -4416, -165, 0 }, { 2863, -4416, 165, 0 }, { -2863, 4416, -165, 0 }, { 2863, 4416, 165, 0 } }, { 236, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -2337, -3376, 239, 0 }, { { 2868, -4400, 137, 0 }, { -2867, -4400, -136, 0 }, { 2868, 4400, 137, 0 }, { -2867, 4400, -136, 0 } }, { -195, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -3665, -3232, 5071, 0 }, { { -1383, -4256, 519, 0 }, { 1381, -4256, -521, 0 }, { -1383, 4256, 519, 0 }, { 1381, 4256, -521, 0 } }, { -1447, 0, -3843, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -3764, -3216, 4924, 0 }, { { 1447, -4240, -514, 0 }, { -1449, -4240, 512, 0 }, { 1447, 4240, -514, 0 }, { -1449, 4240, 512, 0 } }, { 1368, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -6177, -3200, 5327, 0 }, { { 980, -4224, 41, 0 }, { -979, -4224, -40, 0 }, { 980, 4224, 41, 0 }, { -979, 4224, -40, 0 } }, { -171, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { -6210, -3280, 5598, 0 }, { { -979, -4304, 7, 0 }, { 980, -4304, -7, 0 }, { -979, 4304, 7, 0 }, { 980, 4304, -7, 0 } }, { -30, 0, -4117, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { -5988, -3552, -292, 0 }, { { -959, -4576, 198, 0 }, { 960, -4576, -198, 0 }, { -959, 4576, 198, 0 }, { 960, 4576, -198, 0 } }, { -829, 0, -4015, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { -6051, -3536, -452, 0 }, { { 949, -4560, -244, 0 }, { -949, -4560, 245, 0 }, { 949, 4560, -244, 0 }, { -949, 4560, 245, 0 } }, { 1025, 0, 3983, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { -5216, -3488, -2833, 0 }, { { -8, -4224, -2308, 0 }, { 8, -4224, 2308, 0 }, { -8, 4224, -2308, 0 }, { 8, 4224, 2308, 0 } }, { 4105, 0, -15, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 2, 7, 1, 0 },
    { NULL, NULL, NULL, { -5072, -3488, -2960, 0 }, { { 8, -4224, 2292, 0 }, { -8, -4224, -2292, 0 }, { 8, 4224, 2292, 0 }, { -8, 4224, -2292, 0 } }, { -4102, 0, 14, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 7, 2, 1, 0 },
    { NULL, NULL, NULL, { -5312, -2848, 0x28E0, 0 }, { { 235, -4224, -1330, 0 }, { -240, -4224, 1325, 0 }, { 235, 4224, -1330, 0 }, { -240, 4224, 1325, 0 } }, { 4031, 0, 720, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -5216, -3008, 0x2940, 0 }, { { -239, -4224, 1326, 0 }, { 236, -4224, -1329, 0 }, { -239, 4224, 1326, 0 }, { 236, 4224, -1329, 0 } }, { -4033, 0, -722, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -1314, -3296, 4926, 0 }, { { 1314, -4256, 675, 0 }, { -1313, -4256, -675, 0 }, { 1314, 4256, 675, 0 }, { -1313, 4256, -675, 0 } }, { -1878, 0, 3651, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1076, -3264, 5229, 0 }, { { -1235, -4256, -651, 0 }, { 1236, -4256, 651, 0 }, { -1235, 4256, -651, 0 }, { 1236, 4256, 651, 0 } }, { 1912, 0, -3632, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -5056, -3232, 2256, 0 }, { { 24, -4224, 2036, 0 }, { -24, -4224, -2036, 0 }, { 24, 4224, 2036, 0 }, { -24, 4224, -2036, 0 } }, { -4105, 0, 48, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 6, 3, 1, 0 },
    { NULL, NULL, NULL, { -5216, -2944, 2272, 0 }, { { -24, -4224, -2100, 0 }, { 24, -4224, 2100, 0 }, { -24, 4224, -2100, 0 }, { 24, 4224, 2100, 0 } }, { 4111, 0, -47, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 3, 6, 1, 0 },
    { NULL, NULL, NULL, { -5248, -2912, 6816, 0 }, { { -24, -4224, -1844, 0 }, { 24, -4224, 1844, 0 }, { -24, 4224, -1844, 0 }, { 24, 4224, 1844, 0 } }, { 4101, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -5040, -3008, 6912, 0 }, { { -24, -4224, 1844, 0 }, { 24, -4224, -1844, 0 }, { -24, 4224, 1844, 0 }, { 24, 4224, -1844, 0 } }, { -4103, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { -384, -2624, 7537, 0 }, { { -1800, -4416, 722, 0 }, { 1793, -4416, -730, 0 }, { -1800, 4416, 722, 0 }, { 1793, 4416, -730, 0 } }, { -1542, 0, -3815, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 4, 9, 1, 0 },
    { NULL, NULL, NULL, { -448, -2816, 7328, 0 }, { { 1841, -4416, -751, 0 }, { -1852, -4416, 741, 0 }, { 1841, 4416, -751, 0 }, { -1852, 4416, 741, 0 } }, { 1537, 0, 3806, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, 1, 0 },
    { NULL, NULL, NULL, { -2465, -2944, 9983, 0 }, { { 211, -4416, -1982, 0 }, { -215, -4416, 1978, 0 }, { 211, 4416, -1982, 0 }, { -215, 4416, 1978, 0 } }, { 4081, 0, 438, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, 1, 0 },
    { NULL, NULL, NULL, { -2370, -2976, 0x27BE, 0 }, { { -217, -4416, 1976, 0 }, { 209, -4416, -1985, 0 }, { -217, 4416, 1976, 0 }, { 209, 4416, -1985, 0 } }, { -4083, 0, -440, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 9, 1, 0 },
    { NULL, NULL, NULL, { -400, -3008, -2064, 0 }, { { -1759, -4416, -117, 0 }, { 1759, -4416, 117, 0 }, { -1759, 4416, -117, 0 }, { 1759, 4416, 117, 0 } }, { 271, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 4748, 0, 21, 2, 1, 0 },
    { NULL, NULL, NULL, { -224, -3296, -2176, 0 }, { { 1759, -4416, 117, 0 }, { -1759, -4416, -117, 0 }, { 1759, 4416, 117, 0 }, { -1759, 4416, -117, 0 } }, { -273, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 4748, 0, 2, 21, 1, 0 },
    { NULL, NULL, NULL, { -1889, -3232, -4353, 0 }, { { 265, -4416, -2218, 0 }, { -265, -4416, 2218, 0 }, { 265, 4416, -2218, 0 }, { -265, 4416, 2218, 0 } }, { 4078, 0, 486, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 21, 2, 1, 0 },
    { NULL, NULL, NULL, { -1696, -3136, -4480, 0 }, { { -265, -4416, 2218, 0 }, { 265, -4416, -2218, 0 }, { -265, 4416, 2218, 0 }, { 265, 4416, -2218, 0 } }, { -4079, 0, -488, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 2, 21, 129, 0 },
};

GpObj4C D_dryfield_night_main_street_80187EBC[12] = {
    { NULL, NULL, NULL, { -384, 0, -3712, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, 0, 1, 18, 2, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2816, 0 }, { { -416, 0, -864, 0 }, { 416, 0, -864, 0 }, { -416, 0, 864, 0 }, { 416, 0, 864, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 957, 0, 11, 33, 2, 0 },
    { NULL, NULL, NULL, { -6880, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, 0, 15, 49, 2, 0 },
    { NULL, NULL, NULL, { -6848, -48, 6000, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, 0, 12, 65, 2, 0 },
    { NULL, NULL, NULL, { -6912, -48, 0x27E0, 0 }, { { -416, 0, -800, 0 }, { 416, 0, -800, 0 }, { -416, 0, 800, 0 }, { 416, 0, 800, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 900, 0, 13, 81, 2, 0 },
    { NULL, NULL, NULL, { -3584, -48, 0x2A00, 0 }, { { 736, 0, -416, 0 }, { 736, 0, 416, 0 }, { -736, 0, -416, 0 }, { -736, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 844, 0, 14, 97, 2, 0 },
    { NULL, NULL, NULL, { -736, -64, 0x2842, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1402, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -225, -48, 8767, 0 }, { { -415, 0, -415, 0 }, { 417, 0, -415, 0 }, { -416, 0, 416, 0 }, { 416, 0, 416, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 586, 0, 25, 115, 2, 0 },
    { NULL, NULL, NULL, { -5600, -64, -3600, 0 }, { { -1600, 0, -1872, 0 }, { 1600, 0, -1872, 0 }, { -1600, 0, 1873, 0 }, { 1600, 0, 1873, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 2455, 0x8005, 16, 0, 3, 0 },
    { NULL, NULL, NULL, { -2305, -64, 8447, 0 }, { { -2192, 0, -2576, 0 }, { 2193, 0, -2576, 0 }, { -2192, 0, 2576, 0 }, { 2193, 0, 2576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 3376, 0x8005, 17, 0, 3, 0 },
    { NULL, NULL, NULL, { -5920, -64, -4064, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, 5, 18, 0, 2, 0 },
    { NULL, NULL, NULL, { -2962, -64, 7199, 0 }, { { -624, 0, -512, 0 }, { 625, 0, -512, 0 }, { -624, 0, 513, 0 }, { 625, 0, 513, 0 } }, { 0, 4096, 0, 0 }, { -2598, 0, -3166, 0 }, 807, 5, 19, 0, 132, 0 },
};

GpObj4C D_dryfield_night_main_street_8018824C[12] = {
    { NULL, NULL, NULL, { -384, 0, -3712, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, 0, 1, 18, 2, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2816, 0 }, { { -416, 0, -864, 0 }, { 416, 0, -864, 0 }, { -416, 0, 864, 0 }, { 416, 0, 864, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 957, 0, 11, 33, 2, 0 },
    { NULL, NULL, NULL, { -6880, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, 0, 15, 49, 2, 0 },
    { NULL, NULL, NULL, { -6848, -48, 6000, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, 0, 12, 65, 2, 0 },
    { NULL, NULL, NULL, { -6912, -48, 0x27E0, 0 }, { { -416, 0, -800, 0 }, { 416, 0, -800, 0 }, { -416, 0, 800, 0 }, { 416, 0, 800, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 900, 0, 13, 81, 2, 0 },
    { NULL, NULL, NULL, { -3584, -48, 0x2A00, 0 }, { { 736, 0, -416, 0 }, { 736, 0, 416, 0 }, { -736, 0, -416, 0 }, { -736, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 844, 0, 14, 97, 2, 0 },
    { NULL, NULL, NULL, { -736, -64, 0x2842, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1402, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { -929, -48, 8303, 0 }, { { -415, 0, -687, 0 }, { 417, 0, -687, 0 }, { -416, 0, 688, 0 }, { 416, 0, 688, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 801, 0, 25, 115, 2, 0 },
    { NULL, NULL, NULL, { -5600, -64, -3600, 0 }, { { -1600, 0, -1872, 0 }, { 1600, 0, -1872, 0 }, { -1600, 0, 1873, 0 }, { 1600, 0, 1873, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 2455, 0x8005, 16, 0, 3, 0 },
    { NULL, NULL, NULL, { -2305, -64, 8447, 0 }, { { -2192, 0, -2576, 0 }, { 2193, 0, -2576, 0 }, { -2192, 0, 2576, 0 }, { 2193, 0, 2576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 3376, 0x8005, 17, 0, 3, 0 },
    { NULL, NULL, NULL, { -5920, -64, -4064, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, 5, 18, 0, 2, 0 },
    { NULL, NULL, NULL, { -2962, -64, 7199, 0 }, { { -624, 0, -512, 0 }, { 625, 0, -512, 0 }, { -624, 0, 513, 0 }, { 625, 0, 513, 0 } }, { 0, 4096, 0, 0 }, { -2598, 0, -3166, 0 }, 807, 5, 19, 0, 132, 0 },
};

GpPointLight D_dryfield_night_main_street_801885DC[10] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4417, -2000, 2032 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4417, -2000, -1229 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -878, -2000, -2607 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 4096, 3686, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -878, -2000, 7297 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3686, 4096, 3686, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4253, -2000, 0x2A75 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 9652 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 6623 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1664, -2000, 9386 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1500, 4000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, -3140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3404 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 316 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 500, 2000 },
};

GpRoomCoordSet D_dryfield_night_main_street_8018899C[1] = {
    { 0, NULL, 10, D_dryfield_night_main_street_801885DC, 0, NULL },
};

GpAreaTmdRec D_dryfield_night_main_street_801889B4[3] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 8, 7, 2, 0, { 0, 0 }, D_80165B88 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_main_street_801889D8[2] = {
    { 106, 361, 0, 0, { 0, 0 }, D_80140744 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_night_main_street_801889F0[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_main_street_80188A08[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017ADF8, D_dryfield_night_main_street_801889B4 },
    { D_map_dryfield_full_8017AE78, D_dryfield_night_main_street_801889D8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017AE98, D_dryfield_night_main_street_801889F0 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

GpRoomBoundVec D_dryfield_night_main_street_80188A70[25] = {
    { 24, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 615, 617, 618, 616 },
    { 616, 615, 615, 615 },
    { 617, 618, 617, 617 },
    { 618, 618, 618, 618 },
    { 617, 615, 616, 615 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
};

s32 D_dryfield_night_main_street_80188B38[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

s32 D_dryfield_night_main_street_80188B44[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_dryfield_night_main_street_80188B50[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

GpRoomParamRec D_dryfield_night_main_street_80188B5C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_night_main_street_80188B64[1] = {
    { 0, 0, 1, 0, D_dryfield_night_main_street_80188B38 },
};

GpRoomParamRec D_dryfield_night_main_street_80188B6C[1] = {
    { 0, 0, 1, 0, D_dryfield_night_main_street_80188B44 },
};

GpRoomParamRec D_dryfield_night_main_street_80188B74[1] = {
    { 0, 0, 1, 0, D_dryfield_night_main_street_80188B50 },
};

GpRoomParamRec D_dryfield_night_main_street_80188B7C[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec* D_dryfield_night_main_street_80188B84[8] = {
    D_dryfield_night_main_street_80188B5C,
    D_dryfield_night_main_street_80188B7C,
    D_dryfield_night_main_street_80188B6C,
    D_dryfield_night_main_street_80188B74,
    D_dryfield_night_main_street_80188B64,
    D_dryfield_night_main_street_80188B5C,
    D_dryfield_night_main_street_80188B5C,
    D_dryfield_night_main_street_80188B5C,
};

RoomFadeStorage D_dryfield_night_main_street_80188BA4 = { 0 };

RoomEventMsg D_dryfield_night_main_street_80188BAC = { 0 };

DryfieldNightMainStreetStorage8BB4 D_dryfield_night_main_street_80188BB4 = { 0 };

RoomEventMsg D_dryfield_night_main_street_80188BBC = { 0 };

u8 D_dryfield_night_main_street_80188BC4 = 0;

DryfieldNightMainStreetStorage8BC8 D_dryfield_night_main_street_80188BC8;

RoomEventReq D_dryfield_night_main_street_80188BD8;

static s32  func_dryfield_night_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg);
static void func_dryfield_night_main_street_8017FFF8(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_dryfield_night_main_street_8018041C(GfxCoord* arg0, s16 arg1, u8* rgb);

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_dryfield_night_main_street_8017D600(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_dryfield_night_main_street_80188BC8.value.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_dryfield_night_main_street_80188BC8.value.fade != 0) {
                    D_dryfield_night_main_street_80188BA4.fade.field_0 = 0;
                    D_dryfield_night_main_street_80188BA4.fade.field_1 = 0;
                    D_dryfield_night_main_street_80188BA4.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_dryfield_night_main_street_80188BA4.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_dryfield_night_main_street_80188BC8.value.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_dryfield_night_main_street_80188BC8.value.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_dryfield_night_main_street_80188BC8.value.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_night_main_street_80188BAC.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_night_main_street_80188BAC.field_2;
            Mc_SaveData[0].state.at4.loc.room = D_dryfield_night_main_street_80188BAC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command and answers 0. Otherwise the message and
/// request are latched, the nibble is written, the event task is spawned and
/// the answer is 2. A non-zero `field_5` on the message only reports the
/// answer, with none of the side effects.
static s32 func_dryfield_night_main_street_8017D798(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                  = req->flagId;
    D_dryfield_night_main_street_80188BC4 = 0;
    neg                                   = flag < 0;
    got                                   = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_night_main_street_80188BBC = *msg;
                D_dryfield_night_main_street_80188BD8 = *req;
                id                                    = req->flagId;
                mode                                  = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_night_main_street_80182098, 0, 0, 0);
                D_dryfield_night_main_street_80188BC4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns: runs the latched request's CAP command,
/// plays its two sound events in turn and waits for each to finish, then
/// writes the latched message's destination into the save data and hands over
/// to task type 0x11 to load it.
void func_dryfield_night_main_street_8017D8FC(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_night_main_street_80188BD8.field_0);
            if (D_dryfield_night_main_street_80188BD8.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_main_street_80188BD8.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_night_main_street_80188BD8.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_night_main_street_80188BD8.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_night_main_street_80188BD8.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_night_main_street_80188BD8.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_night_main_street_80188BBC.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_night_main_street_80188BBC.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_dryfield_night_main_street_80188BBC.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_night_main_street_8017D5F4 = {
    { func_dryfield_night_main_street_8017E064, func_dryfield_night_main_street_8017E0B8, taskKill },
};

/// Message handler for the room. Messages 0x19, 1 and 0xF answer in the copy's
/// `field_3` from story nibbles. Messages 0xB and 0xC run the room's own event
/// gate: unless the event's nibble is already set, the message and event are
/// latched, the nibble is set and the room's event task is spawned. Messages
/// 0xD and 0xE go through the event gate `func_dryfield_night_main_street_8017D798`
/// and, when it fires, swap collected bits
/// 0x10F / 0x112 for 0x113. Anything else is not consumed.
s32 func_dryfield_night_main_street_8017DA6C(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent ev;
    s32              ret;

    *out = *msg;
    if (msg->prefix.packed == 0x19) {
        if (gGameSession->at4.loc.stage == 2) {
            if (msg->field_5 == 0) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->field_3 = 2;
                } else {
                    out->field_3 = 1;
                }
            }
        } else if (msg->field_5 == 0) {
            out->field_3 = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (msg->prefix.packed == 1 && msg->field_5 == 0) {
        if (GameFlag_GetNibble(0x63) == 0) {
            out->field_3 = 1;
        } else if (GameFlag_GetNibble(0x7A) >= 4) {
            out->field_3 = 4;
        } else {
            out->field_3 = GameFlag_GetNibble(0x61) + 2;
        }
    }
    if (msg->prefix.packed == 0xF && msg->field_5 == 0) {
        out->field_3 = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->prefix.packed == 0x19 && GameFlag_GetNibble(0x61) != 0) {
        if (msg->field_5 == 0) {
            Gp_SetNibbleIf(msg->field_6, 2);
            Gp_RunCapCmd1(0x13);
            return 2;
        }
        return 2;
    }
    if (msg->prefix.packed == 0xB) {
        ev.capCmd                                   = 3;
        ev.stageSnd                                 = 0x52020005;
        ev.flagId                                   = 0x57;
        ev.fade                                     = 0;
        D_dryfield_night_main_street_80188BB4.value = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_main_street_80188BAC       = *out;
                D_dryfield_night_main_street_80188BC8.value = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_main_street_8018208C, 0, 0, 0);
                D_dryfield_night_main_street_80188BB4.value = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->prefix.packed == 0xC) {
        ev.capCmd                                   = 4;
        ev.stageSnd                                 = 0x52020005;
        ev.flagId                                   = 0x58;
        ev.fade                                     = 0;
        D_dryfield_night_main_street_80188BB4.value = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->field_5 == 0) {
                D_dryfield_night_main_street_80188BAC       = *out;
                D_dryfield_night_main_street_80188BC8.value = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&D_dryfield_night_main_street_8018208C, 0, 0, 0);
                D_dryfield_night_main_street_80188BB4.value = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->prefix.packed == 0xD) {
        req.field_0 = 0xA;
        req.field_4 = 5;
        req.field_8 = Gp_PackStageSndId(0x5202000A);
        req.field_C = Gp_PackStageSndId(0x52020005);
        req.flagId  = 0x41;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_main_street_80188BC4 != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x93) == 0) {
            Gp_SetNibbleIf(msg->field_6, 0);
        }
        return ret;
    } else if (msg->prefix.packed == 0xE) {
        req.field_0 = 0xB;
        req.field_4 = 6;
        req.field_8 = Gp_PackStageSndId(0x5202000A);
        req.field_C = Gp_PackStageSndId(0x52020005);
        req.flagId  = 0x42;
        req.itemId  = 0x13;
        ret         = func_dryfield_night_main_street_8017D798(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_night_main_street_80188BC4 != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->field_5 == 0 && GameFlag_GetNibble(0x94) == 0) {
            Gp_SetNibbleIf(msg->field_6, 0);
        }
        return ret;
    }
    return 1;
}

/// Waits for the CAP script to go idle, then records the play time when the
/// script's event key is 1, drops collected bit 0x11A when 0x119 is also held,
/// and ends.
void func_dryfield_night_main_street_8017DE78(Task* task)
{
    if (Gp_CapBusy() == 0) {
        if (Gp_GetCapEventKey() == 1) {
            Gp_MarkPlayTime();
        }
        if (Gp_HasCollectedBit(0x119) != 0 && Gp_HasCollectedBit(0x11A) != 0) {
            Gp_ClearCollectedBit(0x11A);
        }
        taskKill(task);
    }
}

/// Plays the stage sound a CAP script cue asks for: cues 8, 9 and 0xC play
/// their own sound (9 also plays 0xC's), and cues 0x65 and 0x78 play sound
/// 0xD when the event key is 1 and 0 respectively.
s32 func_dryfield_night_main_street_8017DEF0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x52020008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x52020009, 0, 0);
            /* fallthrough */
        case 0xC:
            Gp_EnqueueStageSnd6(0x5202000C, 0, 0);
            break;
        case 0x65:
            if (Gp_GetCapEventKey() == 1) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
        case 0x78:
            if (Gp_GetCapEventKey() == 0) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
    }
    return 0;
}

/// Acts only on `arg2 == 1`. Once nibble 0x7B has reached 2 it ages flag
/// 0x119, sets current-bit flag 0x1B unless collected bit 0x119 is held,
/// spawns CAP entry 1 and the task above; before that it spawns CAP entry
/// 0x14 instead.
s32 func_dryfield_night_main_street_8017DFC8(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x7B) >= 2) {
            Gp_AgeFlag119();
            if (Gp_HasCollectedBit(0x119) == 0) {
                Gp_SetCurBit2Flag(0x1B, 1);
            }
            Gp_SpawnIfCapIdle(1, 1);
            Task_SpawnFromTable(&D_dryfield_night_main_street_801820A4, 0, 0, 0);
        } else {
            Gp_SpawnIfCapIdle(0x14, 1);
        }
    }
    return 0;
}

s32 func_dryfield_night_main_street_8017E054(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_main_street_8017E05C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// pointer slot 7, runs the room's message-table pass, then advances state and
/// raises the `D_80115598` flag.
static void func_dryfield_night_main_street_8017E064(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_main_street_801820B0;
    Game_SetPtrSlot(arg0, 7);
    func_dryfield_night_main_street_8017E118();
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// The room entry task's idle state.
static void func_dryfield_night_main_street_8017E0B8(Task* task)
{
}

/// Runs the room task's current state from its three-entry table, which it
/// copies onto the stack before the call.
void func_dryfield_night_main_street_8017E0C0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_main_street_8017D5F4;
    sp.funcs[task->state](task);
}

/// Applies the sprite-command patch lists selected by game-flag nibbles 0x88,
/// 0x89, 0x8A and 0x8C, one table of lists per nibble.
static void func_dryfield_night_main_street_8017E118(void)
{
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182168, 0x88);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_8018216C, 0x89);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182170, 0x8A);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182174, 0x8C);
}

/// Per-frame room task. On its first run it stores the ids 0x60286-0x60289 in
/// four gameplay globals. Each run it draws the anchors whose view mask in
/// `D_...80182230` contains the current view (entries 2 and 3 are cleared once
/// nibble 0x7F is set) and publishes the view's `roomEffectMode`. In views 8
/// and 0x13 it spawns 0x30 randomly placed 0x601B2 effects on entering the
/// view, and one more on each run with bit 0 of `gDisplayState.animFrame` set while it
/// stays. `spawnArg1` holds the view seen on the previous run.
void func_dryfield_night_main_street_8017E484(Task* task)
{
    s32 mask;
    s32 i;

    mask = 1 << (Gp_GetViewIndex() & 0xFF);
    if (task->state == 0) {
        D_80115728  = 0x60286;
        D_80115744  = 0x60287;
        D_8011573C  = 0x60288;
        D_80115720  = 0x60289;
        task->state = 1;
    }
    if (GameFlag_GetNibble(0x7F) != 0) {
        D_dryfield_night_main_street_80182230[3] = 0;
        D_dryfield_night_main_street_80182230[2] = 0;
    }
    if (mask & D_dryfield_night_main_street_80182230[0]) {
        func_dryfield_night_main_street_8017E940(D_dryfield_night_main_street_801821A8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[2]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821B8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[4]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821C8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[6]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821D8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[8]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821E8, 0x180);
    }
    for (i = 10; i < 16; i++) {
        if (mask & D_dryfield_night_main_street_80182230[i]) {
            func_dryfield_night_main_street_8017F128(&D_dryfield_night_main_street_801821A8[i], 1, 0x380);
        }
    }
    Gp_State1C->roomEffectMode = D_dryfield_night_main_street_80182178[(Gp_GetViewIndex() & 0xFF) - 1];
    if ((Gp_GetViewIndex() & 0xFF) == 8 || (Gp_GetViewIndex() & 0xFF) == 0x13) {
        if (task->spawnArg1.value != (Gp_GetViewIndex() & 0xFF)) {
            for (i = 0; i < 0x30; i++) {
                D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
                D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
                D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
                Gp_SpawnEff(0x601B2, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) + 0x103100,
                            &D_dryfield_night_main_street_801821A8[16]);
            }
        } else if (gDisplayState.animFrame & 1) {
            D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
            D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
            D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
            Gp_SpawnEff(0x601B2, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) | 0x82100,
                        &D_dryfield_night_main_street_801821A8[16]);
        }
    }
    task->spawnArg1.value = Gp_GetViewIndex() & 0xFF;
}

/// Draws a light shaft between the two world points `arg0[0]` and `arg0[1]`:
/// a fan of gouraud wedges around each projected point, joined by wedges
/// spanning the two, the sweep oriented along the screen-space line between
/// them. Each radius is `(s16)arg1 * 64` over that point's OTZ. Nothing is
/// drawn unless both points project. The lit vertices take a brightness that
/// flickers with the frame counter.
static void func_dryfield_night_main_street_8017E940(SVECTOR* arg0, s32 arg1)
{
    SVECTOR*                 p1;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    s32                      raw;
    s32                      ang;
    s32                      angEnd;
    s32                      limit;
    s32                      angStart;
    s32                      t;
    s32                      t2;
    s32                      t3;
    s32                      conn;
    s32                      scaled;
    s32                      blend;

    p1 = arg0 + 1;
    SCRATCH_PUSH(OverlayPointPairScratch);
    block = SCRATCH_HEAD(OverlayPointPairScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz0);
        gte_ldv0(p1);
        gte_rtps();
        gte_stsxy(&block->sx1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz1);
            scaled    = (s16)arg1 * 64;
            block->r0 = scaled / block->otz0;
            block->r1 = scaled / block->otz1;
            raw       = ratan2((s16)block->sy1 - (s16)block->sy0, (s16)block->sx0 - (s16)block->sx1);
            ds        = &gDisplayState;
            ang       = (s16)raw;
            blend     = (((u8)ds->animFrame & 1) * 0x10) | 0x20;
            angEnd    = ang + 0x800;
            if (ang < angEnd) {
                angStart = ang;
                limit    = angEnd;
                do {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(ang)) >> 12);
                    t        = ang + 0x200;
                    prim->y0 = block->sy0 + ((block->r0 * rcos(ang)) >> 12);
                    prim->x1 = block->sx0 + ((block->r0 * rsin(t)) >> 12);
                    prim->y1 = block->sy0 + ((block->r0 * rcos(t)) >> 12);
                    t2       = ang + 0x400;
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx0 + ((block->r0 * rsin(t2)) >> 12);
                    prim->y3 = block->sy0 + ((block->r0 * rcos(t2)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, blend, blend, blend);
                    prim->x0 = block->sx0 + ((block->r0 * rsin(conn)) >> 12);
                    prim->y0 = block->sy0 + ((block->r0 * rcos(conn)) >> 12);
                    prim->x1 = block->sx1 + ((block->r1 * rsin(conn)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(conn)) >> 12);
                    prim->x2 = block->sx0;
                    prim->y2 = block->sy0;
                    prim->x3 = block->sx1;
                    prim->y3 = block->sy1;
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);

                    prim           = (POLY_G4*)gGpuPrimCursor;
                    t3             = ang + 0x800;
                    t              = t3;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, blend, blend, blend);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y0 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xA00;
                    prim->x1 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y1 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    t        = ang + 0xC00;
                    prim->x2 = block->sx1;
                    prim->y2 = block->sy1;
                    prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
                    prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
                    addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP(OverlayPointPairScratch);
}

/// Draws a textured semi-transparent sprite centred on the world point `arg0`
/// when it projects. `arg1` picks the 40-texel column of the texture page and
/// its palette; `arg2` is the half-extent, scaled by 39 over the OTZ on
/// screen. The sprite's brightness flickers with the frame counter.
static void func_dryfield_night_main_street_8017F128(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    s32                idx;
    s32                blend;
    s16                xy;

    block = SCRATCH_PUSH(RoomDraw13Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        blend       = (((u8)gDisplayState.animFrame & 1) * 16) + 0x20;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        setUVWH(prim, idx * 40, 0, 0x27, 0x27);
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        xy            = block->sx - (u16)block->radius;
        prim->x2      = xy;
        prim->x0      = xy;
        xy            = block->sx + (u16)block->radius;
        prim->x3      = xy;
        prim->x1      = xy;
        xy            = block->sy - (u16)block->radius;
        prim->y1      = xy;
        prim->y0      = xy;
        xy            = block->sy + (u16)block->radius;
        prim->y3      = xy;
        prim->y2      = xy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

void func_dryfield_night_main_street_8017F3B0(Task* task)
{
    GpEffWork* work  = task->spawnArg2.pointer;
    GfxCoord*  coord = task->extra.tmd->coords;
    s32        vz;
    s16        f2a;
    u32        rng2;
    u32        rng3;

    work->age++;
    if (task->state == 0) {
        work->scale = task->spawnArg1.value & 0xFFF;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        work->angle = (Gp_LcgState >> 16) & 0xFFF;

        if (task->spawnArg1.value & 0xF000) {
            work->period = (task->spawnArg1.value >> 12) & 0x7;
        } else {
            work->period = 1;
        }

        work->age   = 0;
        task->state = 1;

        if (task->spawnArg1.value & 0xFF0000) {
            f2a = (task->spawnArg1.value >> 16) & 0xFF;
        } else {
            f2a = 0x40;
        }

        work->step    = f2a;
        work->move.vy = 0;
        rng2          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng2;
        work->move.vx = -(((u32)rng2 >> 16) & 0x7F);
        rng3          = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState   = rng3;
        vz            = 0x80 - (((u32)rng3 >> 16) & 0xFF);
        work->move.vz = vz;
        VectorNormalSS(&work->move, &work->move);

        gte_lddp(work->step);
        gte_ldsv(&work->move);
        gte_gpf12();
        gte_stsv(&work->move);
    }

    func_dryfield_night_main_street_8017F608(coord, (u16)work->index, work->scale, work->angle);

    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;

    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 0xA) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative and `otz` is at least 0x41, queues one
/// semi-transparent shade-tex `POLY_FT4` (tpage 0x2B, clut 0x4383) rotated
/// about the projected centre. `arg1` selects a 48-texel UV tile in a 5-wide
/// grid: u = `(arg1 % 5) * 48`, v = `(arg1 / 5) * 48 - 0x80`. `arg2` is a
/// signed half-extent; the on-screen radius is `(s16)arg2 * 47 / otz`.
/// `arg3` is the spin angle, applied at `arg3` and `arg3 + 0x400` through
/// `rsin`/`rcos`.
static void func_dryfield_night_main_street_8017F608(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**             scratch;
    GpEffFlareScratch* head;
    GpEffFlareScratch* block;
    s32*               otzp;
    POLY_FT4*          prim;
    s32                ang;
    s32                ang2;
    s32                sine;
    s32                span;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    u16                vz;
    u16                tex;

    scratch       = SCRATCH_HEAD_ADDR;
    head          = SCRATCH_HEAD_AT(scratch, GpEffFlareScratch);
    block         = head - 1;
    block->vec.vx = (u16)arg0->workm.t[0];
    block->vec.vy = (u16)arg0->workm.t[1];
    vz            = (u16)arg0->workm.t[2];
    otzp          = &block->otz;
    *scratch      = block;
    block->vec.vz = vz;
    tex           = arg1;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(otzp);
        if (block->otz >= 0x41) {
            ang         = (s16)arg3;
            prim->tpage = 0x2B;
            prim->clut  = 0x4383;
            prim->code |= 3;
            u0          = (tex % 5) * 0x30;
            v0          = (tex / 5) * 0x30;
            u1          = u0 + 0x2F;
            v1          = v0 - 0x51;
            v0          = v0 - 0x80;
            setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
            sine      = rsin(ang);
            span      = (s16)arg2 * 0x2F;
            block->dx = ((span / block->otz) * sine) >> 12;
            block->dy = ((span / block->otz) * rcos(ang)) >> 12;
            prim->x0  = block->sx + (u16)block->dx;
            prim->x3  = block->sx - (u16)block->dx;
            prim->y0  = block->sy - (u16)block->dy;
            prim->y3  = block->sy + (u16)block->dy;
            ang2      = ang + 0x400;
            block->dx = ((span / block->otz) * rsin(ang2)) >> 12;
            block->dy = ((span / block->otz) * rcos(ang2)) >> 12;
            prim->x1  = block->sx + (u16)block->dx;
            prim->x2  = block->sx - (u16)block->dx;
            prim->y1  = block->sy - (u16)block->dy;
            prim->y2  = block->sy + (u16)block->dy;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
        }
    }
    SCRATCH_POP(GpEffFlareScratch);
}

/// A drifting mote: the spawn argument gives its brightness flags, vertical
/// speed and lifetime. With neither of the two low bits set it starts dim and
/// rising, with some random extra speed, in state 1, which brightens it;
/// otherwise it starts bright in state 2, moving down, or up when bit 1 is
/// set. Every other tick it advances its
/// animation frame and draws; eight ticks before its lifetime ends it fades
/// out, and it releases its work block once dark or when the room's event
/// state reaches 4.
void func_dryfield_night_main_street_8017FA68(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                if (task->spawnArg1.value & 3) {
                    work->scale   = 0x80;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = ((RoomMoteArg*)&task->spawnArg1.value)->speed;
                    work->move.vz = 0;
                    if (task->spawnArg1.value & 2) {
                        work->move.vy = -work->move.vy;
                    }
                    task->state = 2;
                } else {
                    work->scale   = 0x20;
                    work->angle   = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xFFF;
                    work->period  = ((RoomMoteArg*)&task->spawnArg1.value)->flags & 0xF000;
                    lifetime      = ((RoomMoteArg*)&task->spawnArg1.value)->lifetime;
                    work->step    = lifetime;
                    work->move.vx = 0;
                    work->move.vy = -((RoomMoteArg*)&task->spawnArg1.value)->speed - (((u32)(Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0x3F);
                    work->move.vz = 0;
                    task->state   = (task->spawnArg1.value & 1) + 1;
                }
                break;
            case 1:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_dryfield_night_main_street_8017FD34(coord, work->index, work->angle | 0x1000, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    } else if (work->scale < 0x80) {
                        work->scale += 0x20;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
            case 2:
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_dryfield_night_main_street_8017FD34(coord, work->index, work->angle, work->scale | work->period);
                }
                if (work->scale > 0) {
                    if (work->step - 8 < work->age) {
                        work->scale -= 0x10;
                    }
                } else {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws one mote: projects the coordinate's world position through
/// `GsWSMATRIX` and, unless the GTE flags the projection, queues one
/// semi-transparent textured square centred on it. `arg1`'s low two bits and
/// `arg2`'s top nibble pick the 24-texel texture cell, `arg2`'s low twelve
/// bits are the half-extent (scaled by 23 / (otz + 1)), `arg3`'s low byte is
/// the grey level and its top nibble picks the palette.
static void func_dryfield_night_main_street_8017FD34(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    DisplayState*  ds;
    u16            row;
    u16            pal;
    s32            u0;
    s32            u1;
    s16            xy;

    row           = arg2 >> 12;
    arg2         &= 0xFFF;
    pal           = arg3 >> 12;
    arg3         &= 0xFF;
    block         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->tpage = 0x2A;
        setRGB0(prim, arg3, arg3, arg3);
        if (pal != 0) {
            prim->clut = getClut(pal * 16 + 0xF0, 0x10B);
        } else {
            prim->clut = getClut(0xB0, 0x10B);
        }
        u0 = row * 0x60 + (arg1 & 3) * 24;
        u1 = u0 + 0x17;
        setUV4(prim, u0, 0, u1, 0, u0, 0x17, u1, 0x17);
        block->step = arg2 * 23 / block->otz;
        xy          = block->sx - block->step;
        prim->x2    = xy;
        prim->x0    = xy;
        xy          = block->sx + block->step;
        prim->x3    = xy;
        prim->x1    = xy;
        xy          = block->sy - block->step;
        prim->y1    = xy;
        prim->y0    = xy;
        xy          = block->sy + block->step;
        prim->y3    = xy;
        prim->y2    = xy;
        ds          = &gDisplayState;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a ring of sixteen gouraud quads around the coordinate's projected
/// position, between the radii `arg1` and `arg1 + arg2` (each scaled by 64
/// over the OTZ). The `arg1` edge is black and the other edge takes `rgb`, so
/// the ring fades across its width.
static void func_dryfield_night_main_street_8017FFF8(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   next;
    s32                   litRadius = arg1 + arg2;

    SCRATCH_PUSH(RoomBillboardScratch);
    block         = SCRATCH_HEAD(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = ((s16)arg1 * 64) / block->otz;
        block->rInner = ((s16)litRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            next     = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(next)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(next)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(next)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(next)) >> 12);
            ang      = next;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Draws a glow at the coordinate's projected position: eight gouraud quads
/// fanned around it, of radius `arg1` scaled by 64 over the OTZ. The centre
/// takes `rgb` and the rim is black.
static void func_dryfield_night_main_street_8018041C(GfxCoord* arg0, s16 arg1, u8* rgb)
{
    RoomFanScratch* block;
    POLY_G4*        prim;
    s32             ang;
    s32             otz;

    block         = SCRATCH_PUSH(RoomFanScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        otz           = block->otz + 1;
        block->otz    = otz;
        block->radius = (arg1 * 64) / otz;

        for (ang = 0; ang < 0x1000; ang += 0x200) {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(ang + 0x100)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(ang + 0x100)) >> 12);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(ang + 0x200)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(ang + 0x200)) >> 12);
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomFanScratch);
}

/// A halo on an effect's anchor. State 0 parks the coordinate frame on the
/// anchor and derives the fade step from the spawn argument's duration.
/// State 1 grows and brightens a glow (with a half-bright echo on odd ticks)
/// and a shrinking ring around it for that duration; state 2 fades a
/// two-ring glow back out, then the work block is released. The tint's
/// channel shifts come from the table above.
void func_dryfield_night_main_street_801807B0(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GfxCoord*   coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        switch (arg0->state) {
            case 0:
                rot                 = (GpMtxWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00_m01        = 0x1000;
                rot->m02_m10        = 0;
                rot->m11_m12        = 0x1000;
                rot->m20_m21        = 0;
                rot->m22            = 0x1000;
                coord->coord.t[0]   = mem->pos.vx;
                coord->coord.t[1]   = mem->pos.vy;
                coord->coord.t[2]   = mem->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                shift                 = arg0->spawnArg1.halves.high;
                mem->index            = shift;
                arg0->spawnArg1.value = arg0->spawnArg1.halves.low;
                arg0->state           = 1;
                mem->step             = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale            += mem->step;
                mem->angle            += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]                 = mem->scale >> D_dryfield_night_main_street_80182270.shades[mem->index].r;
                rgb[1]                 = mem->scale >> D_dryfield_night_main_street_80182270.shades[mem->index].g;
                rgb[2]                 = mem->scale >> D_dryfield_night_main_street_80182270.shades[mem->index].b;
                func_dryfield_night_main_street_8018041C(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_dryfield_night_main_street_8018041C(coord, (s16)(mem->angle + 0x100), rgb);
                }
                func_dryfield_night_main_street_8017FFF8(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_dryfield_night_main_street_80182270.shades[mem->index].r;
                    rgb[1] = mem->scale >> D_dryfield_night_main_street_80182270.shades[mem->index].g;
                    rgb[2] = mem->scale >> D_dryfield_night_main_street_80182270.shades[mem->index].b;
                    func_dryfield_night_main_street_80181598(coord, (u16)mem->angle * 4, rgb);
                    mem->scale -= 0x10;
                    mem->angle += 8;
                    return;
                }
                /* fallthrough */
            case 3:
                goto kill;
            default:
                return;
        }
    }
kill:
    Gp_ReleaseState1CMem(mem, arg0);
}

/// A burst on an effect's anchor. Each tick it grows a warm glow and the
/// flickering light glow at its coordinate; while its second level lasts it
/// also draws a widening ring that fades with that level, and afterwards the
/// main level runs down until the work block is released. The task also ends
/// when the room's event state reaches 4.
void func_dryfield_night_main_street_80180B48(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        mem->age++;
        if (arg0->state == 0) {
            mem->age    = 1;
            mem->scale  = 0xE0;
            mem->angle  = 0x80;
            mem->period = 0xE0;
            mem->step   = 0x80;
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        rgb[0]     = mem->scale;
        rgb[1]     = mem->scale >> 1;
        rgb[2]     = mem->scale >> 2;
        step       = mem->angle + 0x10;
        mem->angle = step;
        func_dryfield_night_main_street_8018041C(coord, (s16)(step * 2), rgb);
        func_dryfield_night_main_street_80180CF4(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_dryfield_night_main_street_8017FFF8(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
            mem->period -= 0x18;
            mem->step   += 0x30;
            return;
        }
        mem->scale -= 0x18;
        if (mem->scale < 0x18) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

/// Draws a glow at the coordinate: two camera-facing textured squares, an
/// inner one of half-extent `size` and an outer one of `size * 3 / 2`
/// (each scaled by 0x37 / otz), plus a flat quad on the ground beneath it.
/// It also points the `Gp_RoomCoords[2]` light at the
/// coordinate with a randomly flickering intensity. Nothing is drawn when the
/// GTE flags the projection.
static void func_dryfield_night_main_street_80180CF4(GfxCoord* coord, s16 size)
{
    GfxCoord       ground;
    POLY_FT4*      prim;
    s16            outerLeft;
    s16            outerRight;
    s16            outerTop;
    s16            outerBottom;
    s16            intensity;
    s16            left;
    s16            right;
    s16            top;
    s16            bottom;
    s32            outerSize;
    s32            shifted;
    u32            random;
    GpCoord64*     slot;
    GpPointLight*  light;
    GpRingScratch* block;

    slot                                  = &Gp_RoomCoords[2];
    slot->framesLeft                      = 2;
    light                                 = &slot->light;
    light->inner                          = 0x300;
    light->outer                          = 0x3000;
    random                                = (Gp_LcgState * 5) + 0x71357911;
    intensity                             = ((random >> 0x10) & 0x700) + 0x800;
    light->head.r                         = intensity;
    shifted                               = intensity << 0x10;
    light->head.g                         = shifted >> 0x11;
    light->head.b                         = shifted >> 0x12;
    light->head.u.at.local.t[0]           = coord->coord.t[0];
    light->head.u.at.local.t[1]           = coord->coord.t[1];
    light->head.u.at.local.t[2]           = coord->coord.t[2];
    slot->light.head.u.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_LcgState                           = random;
    block                                 = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx                         = coord->workm.t[0];
    block->vec.vy                         = coord->workm.t[1];
    block->vec.vz                         = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2EU;
        prim->tpage = 0x29;
        if (gDisplayState.animFrame & 1) {
            prim->r0   = 0xA0;
            prim->g0   = 0x80;
            prim->b0   = 0x60;
            prim->clut = 0x428B;
            setUV4(prim, 0x70, 0xC8, 0xA7, 0xC8, 0x70, 0xFF, 0xA7, 0xFF);
        } else {
            prim->clut = 0x428C;
            setUV4(prim, 0xA8, 0xC8, 0xDF, 0xC8, 0xA8, 0xFF, 0xDF, 0xFF);
            prim->code |= 1;
        }
        block->step = size * 0x37 / block->otz;
        left        = block->sx - block->step;
        prim->x2    = left;
        prim->x0    = left;
        right       = block->sx + block->step;
        prim->x3    = right;
        prim->x1    = right;
        top         = block->sy - block->step;
        prim->y1    = top;
        prim->y0    = top;
        bottom      = block->sy + block->step;
        prim->y3    = bottom;
        prim->y2    = bottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        prim->code  = 0x2F;
        prim->tpage = 0x29;
        prim->clut =
            (((gDisplayState.animFrame & 1) * 0x10 + 0x120) >> 4) | 0x4300;
        setUV4(prim, 0x38, 0xC8, 0x6F, 0xC8, 0x38, 0xFF, 0x6F, 0xFF);
        outerSize   = (s16)(size * 3 / 2);
        block->step = outerSize * 0x37 / block->otz;
        outerLeft   = block->sx - block->step;
        prim->x2    = outerLeft;
        prim->x0    = outerLeft;
        outerRight  = block->sx + block->step;
        prim->x3    = outerRight;
        prim->x1    = outerRight;
        outerTop    = block->sy - block->step;
        prim->y1    = outerTop;
        prim->y0    = outerTop;
        outerBottom = block->sy + block->step;
        prim->y3    = outerBottom;
        prim->y2    = outerBottom;
        addPrim(
            Gpu_OtEntryAtByteOffset((((u32)block->otz << gDisplayState.otDepthShift) >> 2 & 0xFFC)),
            prim);
        if (Gp_TraceGroundCoord(coord, &ground) == 1) {
            func_dryfield_night_main_street_80181220(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_dryfield_night_main_street_80181220(GfxCoord* arg0, s32 arg1)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;
    s32            u;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        v     = &block->vec[i];
        tbl   = &D_80111E38[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(v);
        gte_rtv0();
        gte_stsv(v);
        v->vx += arg0->workm.t[0];
        v->vy += arg0->workm.t[1];
        v->vz += arg0->workm.t[2];
    }

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x30, 0x20, 0x20);
        prim->tpage = 0x28;
        prim->clut  = 0x428C;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v0    = 0x38;
        prim->u0    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v1    = 0x38;
        prim->u1    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xC0;
        prim->v2    = 0x57;
        prim->u2    = u;
        u           = ((gDisplayState.animFrame & 1) << 5) + 0xDF;
        prim->v3    = 0x57;
        prim->u3    = u;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Draws a star-shaped glow at the coordinate's projected position: a
/// half-bright fan of radius `arg1` (scaled by 64 over the OTZ), a full-bright
/// fan of half that radius over it, and four half-bright spikes. Every quad
/// takes `arg2` at the centre and is black at its rim.
static void func_dryfield_night_main_street_80181598(GfxCoord* arg0, s16 arg1, u8* arg2)
{
    RoomBillboardScratch* block;
    POLY_G4*              prim;
    s32                   ang;
    s32                   t;
    s32                   t2;
    s32                   u;

    block         = SCRATCH_PUSH(RoomBillboardScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Emits sparks from the task's coordinate frame: each tick it turns its angle
/// on by a random amount, spawns effect `D_80115728` moving outward along that
/// angle and upward faster as the task ages, and releases its work block after
/// 0x15 ticks or when the room's event state reaches 4.
void func_dryfield_night_main_street_80181F58(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.tmd->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto kill;
    } else {
        Gp_UpdateCoord(coord);
        mem->age++;
        if (mem->age >= 0x15) {
        kill:
            Gp_ReleaseState1CMem(mem, arg0);
            return;
        }
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        ang          = mem->scale + ((((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200);
        mem->scale   = ang;
        mem->move.vx = (u32)(rcos(ang) * 3) >> 4;
        mem->move.vy = -mem->age * 128;
        mem->move.vz = (u32)(rsin(mem->scale) * 3) >> 4;
        Gp_SpawnEff(D_80115728, coord, 0x30080201, &mem->move);
    }
}
