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
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
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

#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"
// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
#include "../../shared/room_events.h"
// Exported instance: another image refers to this package's copy by name.
#define mainStreetPuffTask dryfieldNightMainStreetPuffTask
#include "../../shared/main_street.h"

#define DRYFIELD_NIGHT_MAIN_STREET_RAND()     ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)
#define D_dryfield_night_main_street_801821B8 (D_dryfield_night_main_street_801821A8[2])
#define D_dryfield_night_main_street_801821C8 (D_dryfield_night_main_street_801821A8[4])
#define D_dryfield_night_main_street_801821D8 (D_dryfield_night_main_street_801821A8[6])
#define D_dryfield_night_main_street_801821E8 (D_dryfield_night_main_street_801821A8[8])

/// Advances the gameplay LCG and yields the high half of the new state.

/// Applies the patch list `table[gameFlagGetNibble(nibble)]` to the current
/// area's view sprite commands. The list is a stream of byte pairs ended by a
/// 0xFF first byte: `(view, 0xFF)` selects that view's command list, and any
/// other `(cmd, value)` stores `value` in that batch's `hidden` byte. The list
/// starts on the view named by its first byte.
#define DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(table, nibble)           \
    {                                                                        \
        GameLocationKey* sess;                                               \
        SpriteView*      rec;                                                \
        SpriteBatch*     batches;                                            \
        u8*              p;                                                  \
        s16              idx;                                                \
        u8**             tbl;                                                \
                                                                             \
        idx     = gameFlagGetNibble(nibble);                                 \
        tbl     = table;                                                     \
        p       = tbl[idx];                                                  \
        sess    = &gGameSession->location.loc;                               \
        rec     = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1]; \
        batches = rec[p[0]].batches;                                         \
        if (p[0] != 0xFF) {                                                  \
            do {                                                             \
                if (p[1] == 0xFF) {                                          \
                    batches = rec[p[0]].batches;                             \
                    p      += 2;                                             \
                }                                                            \
                batches[p[0]].hidden = p[1];                                 \
                p                   += 2;                                    \
            } while (p[0] != 0xFF);                                          \
        }                                                                    \
    }

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc gMainStreetEventTaskDesc;

/// Descriptor of the event task the event gate spawns.
extern TaskDesc gRoomEventTaskDesc;

/// Descriptor of the task `mainStreetPlayTimeTask` runs as.
extern TaskDesc gMainStreetPlayTimeTaskDesc;

/// Message table installed at `Task::msgTable` by the room task's state 0
/// (ids `0x13EE`-`0x13F1`).
extern TaskMessageEntry D_dryfield_night_main_street_801820B0[];

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

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and event the message handler latched for the room's event
/// task, and the flag it raises once it has spawned that task.
extern RoomEventMsg            gRoomEventStagedMsg;
extern RoomEventStartStorage   gMainStreetEventSpawned;
extern RoomLatchedEventStorage gRoomEventLatched;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 gRoomEventActive;

static void func_dryfield_night_main_street_8017E064(Task* arg0);
static void func_dryfield_night_main_street_8017E0B8(Task* task);
static void func_dryfield_night_main_street_8017E118(void);

s32 func_dryfield_night_main_street_8017E054(Task*, s32, s32, s32);
s32 func_dryfield_night_main_street_8017E05C(Task*, s32, s32, s32);

extern WorldCollisionGrid    D_dryfield_night_main_street_801833D0[1];
extern WorldCollisionGrid    D_dryfield_night_main_street_80184540[1];
extern WorldCollisionTrigger D_dryfield_night_main_street_80187704[26];
extern WorldCollisionTrigger D_dryfield_night_main_street_80187EBC[12];

extern WorldCoordRoomAmbientEntry D_dryfield_night_main_street_80188A70[25];
extern WorldCoordRoomLights       D_dryfield_night_main_street_8018899C[1];

extern SpriteBatch  D_dryfield_night_main_street_801848C4[2];
extern SpriteBatch  D_dryfield_night_main_street_80184CD0[8];
extern SpriteBatch  D_dryfield_night_main_street_80184F18[3];
extern SpriteBatch  D_dryfield_night_main_street_80185084[3];
extern SpriteBatch  D_dryfield_night_main_street_801852B8[3];
extern SpriteBatch  D_dryfield_night_main_street_801854D8[6];
extern SpriteBatch  D_dryfield_night_main_street_80185774[5];
extern SpriteBatch  D_dryfield_night_main_street_8018579C[2];
extern SpriteBatch  D_dryfield_night_main_street_801857AC[2];
extern SpriteBatch  D_dryfield_night_main_street_801857BC[2];
extern SpriteBatch  D_dryfield_night_main_street_801857CC[2];
extern SpriteBatch  D_dryfield_night_main_street_801857DC[2];
extern SpriteBatch  D_dryfield_night_main_street_80185AA8[6];
extern SpriteBatch  D_dryfield_night_main_street_80185FB0[10];
extern SpriteBatch  D_dryfield_night_main_street_8018626C[7];
extern SpriteBatch  D_dryfield_night_main_street_801864C0[3];
extern SpriteBatch  D_dryfield_night_main_street_80186834[8];
extern SpriteBatch  D_dryfield_night_main_street_80186950[3];
extern SpriteBatch  D_dryfield_night_main_street_80186968[2];
extern SpriteBatch  D_dryfield_night_main_street_80186978[2];
extern SpriteBatch  D_dryfield_night_main_street_80186C94[7];
extern SpriteBatch  D_dryfield_night_main_street_801870F0[7];
extern SpriteBatch  D_dryfield_night_main_street_8018759C[7];
extern SpriteSource D_dryfield_night_main_street_801848D4[51];
extern SpriteSource D_dryfield_night_main_street_80184D10[26];
extern SpriteSource D_dryfield_night_main_street_80184F30[17];
extern SpriteSource D_dryfield_night_main_street_8018509C[27];
extern SpriteSource D_dryfield_night_main_street_801852D0[26];
extern SpriteSource D_dryfield_night_main_street_80185508[31];
extern SpriteSource D_dryfield_night_main_street_801857EC[35];
extern SpriteSource D_dryfield_night_main_street_80185AD8[62];
extern SpriteSource D_dryfield_night_main_street_80186000[31];
extern SpriteSource D_dryfield_night_main_street_801862A4[27];
extern SpriteSource D_dryfield_night_main_street_801864D8[43];
extern SpriteSource D_dryfield_night_main_street_80186874[11];
extern SpriteSource D_dryfield_night_main_street_80186988[39];
extern SpriteSource D_dryfield_night_main_street_80186CCC[53];
extern SpriteSource D_dryfield_night_main_street_80187128[57];
extern TaskDesc     Actor00100_D1BA84;

TaskDesc gMainStreetEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gMainStreetPlayTimeTaskDesc = { { { TASK_BODY_NONE, 32 } }, mainStreetPlayTimeTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_main_street_801820B0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, mainStreetResolveMsg },
    { 5105, func_dryfield_night_main_street_8017E054 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_main_street_8017E05C },
    { ROOM_MESSAGE_COMMAND, mainStreetTalkMsg },
    { ROOM_MESSAGE_SOUND, mainStreetCapSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
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

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0xE0DC }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"

static inline RoomFxShade* RoomFx_GetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

WorldCollisionRoomResources D_dryfield_night_main_street_80182284[3] = {
    { D_dryfield_night_main_street_801833D0, D_dryfield_night_main_street_80187704, D_dryfield_night_main_street_80187EBC, NULL },
    { D_dryfield_night_main_street_80184540, D_dryfield_night_main_street_80187704, D_dryfield_night_main_street_8018824C, NULL },
    { D_dryfield_night_main_street_80184540, D_dryfield_night_main_street_80187704, D_dryfield_night_main_street_8018824C, NULL },
};

WorldCoordRoomLighting D_dryfield_night_main_street_801822B4[3] = {
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
    gViewIdentityMap,
    D_dryfield_night_main_street_801822CC,
    D_dryfield_night_main_street_801822E4,
};

ViewCount D_dryfield_night_main_street_80182308[3] = { 24, 24, 24 };

DirectionWarpEntry D_dryfield_night_main_street_80182310[7] = {
    { { { .word = 3072 }, -450, 0, -3435 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -450, 0, -3435 }, { 0, 0, 0, 0 }, 0x53020011, 0x53020010, DIRECTION_WARP_SOUND_NONE, 21, DIRECTION_WARP_FLAG_NONE, 488 },
    { { { .word = 1024 }, -6588, 0, -2500 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6588, 0, -2500 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 7, DIRECTION_WARP_FLAG_NONE, 487 },
    { { { .word = 1024 }, -6673, 0, 2155 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6673, 0, 2155 }, { 0, 0, 0, 0 }, 0x53020004, 0x53020003, 0x53020007, 6, DIRECTION_WARP_FLAG_NONE, 486 },
    { { { .word = 1024 }, -6588, 0, 5900 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6588, 0, 5900 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 5, DIRECTION_WARP_FLAG_NONE, 485 },
    { { { .word = 1024 }, -6652, 0, 0x2956 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6652, 0, 0x2956 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 5, DIRECTION_WARP_FLAG_NONE, 484 },
    { { { .word = 2048 }, -3478, 0, 0x29B5 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -3478, 0, 0x29B5 }, { 0, 0, 0, 0 }, 0x53020006, 0x53020005, 0x53020009, 4, DIRECTION_WARP_FLAG_NONE, 483 },
    { { { .word = 3072 }, -488, 0, 8495 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -488, 0, 8495 }, { 0, 0, 0, 0 }, 0x53020002, 0x53020001, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, 482 },
};

static SVECTOR _gDryfieldNightMainStreetCollision05E10Normals[20] = {
#include "assets/dryfield_night_main_street_collision_05E10_normals.inc"
};

static SVECTOR _gDryfieldNightMainStreetCollision05E10Verts[180] = {
#include "assets/dryfield_night_main_street_collision_05E10_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMainStreetCollision05E10Faces[85] = {
#include "assets/dryfield_night_main_street_collision_05E10_faces.inc"
};

static s16 _gDryfieldNightMainStreetCollision05E10Cells[554] = {
#include "assets/dryfield_night_main_street_collision_05E10_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMainStreetCollision05E10Cells[i])
static s16* _gDryfieldNightMainStreetCollision05E10Table[42] = {
#include "assets/dryfield_night_main_street_collision_05E10_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_main_street_801833D0[1] = {
    { NULL, _gDryfieldNightMainStreetCollision05E10Normals, _gDryfieldNightMainStreetCollision05E10Verts, _gDryfieldNightMainStreetCollision05E10Faces, _gDryfieldNightMainStreetCollision05E10Table, 0x2EE6, 0x2C4C, 6, 7, 4000, 85 },
};

SVECTOR gDryfieldNightMainStreetCollision06F80Normals[23] = {
#include "assets/dryfield_night_main_street_collision_06F80_normals.inc"
};

SVECTOR gDryfieldNightMainStreetCollision06F80Verts[196] = {
#include "assets/dryfield_night_main_street_collision_06F80_verts.inc"
};

WorldCollisionGridFace gDryfieldNightMainStreetCollision06F80Faces[89] = {
#include "assets/dryfield_night_main_street_collision_06F80_faces.inc"
};

static s16 _gDryfieldNightMainStreetCollision06F80Cells[720] = {
#include "assets/dryfield_night_main_street_collision_06F80_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMainStreetCollision06F80Cells[i])
static s16* _gDryfieldNightMainStreetCollision06F80Table[42] = {
#include "assets/dryfield_night_main_street_collision_06F80_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_main_street_80184540[1] = {
    { NULL, gDryfieldNightMainStreetCollision06F80Normals, gDryfieldNightMainStreetCollision06F80Verts, gDryfieldNightMainStreetCollision06F80Faces, _gDryfieldNightMainStreetCollision06F80Table, 0x2EE6, 0x2C4C, 6, 7, 4000, 89 },
};

ViewCamera D_dryfield_night_main_street_80184564[24] = {
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

SpriteBatch D_dryfield_night_main_street_801848C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_801848D4[51] = {
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

SpriteBatch D_dryfield_night_main_street_80184CD0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 7, 0, 0, { 0, 0 } },
    { 9, 24, 0, 0, { 5, 0 } },
    { 33, 6, 0, 0, { 1, 0 } },
    { 39, 11, 0, 0, { 4, 0 } },
    { 50, 1, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80184D10[26] = {
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

SpriteBatch D_dryfield_night_main_street_80184F18[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80184F30[17] = {
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

SpriteBatch D_dryfield_night_main_street_80185084[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_8018509C[27] = {
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

SpriteBatch D_dryfield_night_main_street_801852B8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_801852D0[26] = {
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

SpriteBatch D_dryfield_night_main_street_801854D8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 15, 0, 0, { 0, 0 } },
    { 23, 3, 0, 0, { 2, 0 } },
    { 26, 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80185508[31] = {
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

SpriteBatch D_dryfield_night_main_street_80185774[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 5, 0, 0, { 2, 0 } },
    { 10, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_8018579C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_801857AC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_801857BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_801857CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_801857DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_801857EC[35] = {
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

SpriteBatch D_dryfield_night_main_street_80185AA8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 3, 0 } },
    { 5, 8, 0, 0, { 0, 0 } },
    { 13, 13, 0, 0, { 2, 0 } },
    { 26, 9, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80185AD8[62] = {
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

SpriteBatch D_dryfield_night_main_street_80185FB0[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 6, 0 } },
    { 13, 8, 0, 0, { 1, 0 } },
    { 21, 4, 0, 0, { 4, 0 } },
    { 25, 6, 0, 0, { 0, 0 } },
    { 31, 24, 0, 0, { 5, 0 } },
    { 55, 1, 0, 0, { 3, 0 } },
    { 56, 1, 0, 0, { 7, 0 } },
    { 57, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80186000[31] = {
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

SpriteBatch D_dryfield_night_main_street_8018626C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 11, 0, 0, { 3, 0 } },
    { 17, 2, 0, 0, { 2, 0 } },
    { 19, 1, 0, 0, { 4, 0 } },
    { 20, 11, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_801862A4[27] = {
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

SpriteBatch D_dryfield_night_main_street_801864C0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_801864D8[43] = {
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

SpriteBatch D_dryfield_night_main_street_80186834[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 3, 0 } },
    { 4, 7, 0, 0, { 0, 0 } },
    { 11, 20, 0, 0, { 5, 0 } },
    { 31, 8, 0, 0, { 1, 0 } },
    { 39, 3, 0, 0, { 4, 0 } },
    { 42, 1, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80186874[11] = {
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

SpriteBatch D_dryfield_night_main_street_80186950[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_80186968[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_80186978[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80186988[39] = {
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

SpriteBatch D_dryfield_night_main_street_80186C94[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 4, 0, 0, { 3, 0 } },
    { 17, 13, 0, 0, { 2, 0 } },
    { 30, 5, 0, 0, { 4, 0 } },
    { 35, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80186CCC[53] = {
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

SpriteBatch D_dryfield_night_main_street_801870F0[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 8, 0, 0, { 3, 0 } },
    { 21, 26, 0, 0, { 2, 0 } },
    { 47, 6, 0, 0, { 4, 0 } },
    { 53, 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_main_street_80187128[57] = {
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

SpriteBatch D_dryfield_night_main_street_8018759C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { 13, 8, 0, 0, { 3, 0 } },
    { 21, 24, 0, 0, { 2, 0 } },
    { 45, 6, 0, 0, { 4, 0 } },
    { 51, 6, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_main_street_801875D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_main_street_801875E4[24] = {
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

WorldCollisionTrigger D_dryfield_night_main_street_80187704[26] = {
    { NULL, NULL, NULL, { -2385, -3392, 351, 0 }, { { -2863, -4416, -165, 0 }, { 2863, -4416, 165, 0 }, { -2863, 4416, -165, 0 }, { 2863, 4416, 165, 0 } }, { 236, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2337, -3376, 239, 0 }, { { 2868, -4400, 137, 0 }, { -2867, -4400, -136, 0 }, { 2868, 4400, 137, 0 }, { -2867, 4400, -136, 0 } }, { -195, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3665, -3232, 5071, 0 }, { { -1383, -4256, 519, 0 }, { 1381, -4256, -521, 0 }, { -1383, 4256, 519, 0 }, { 1381, 4256, -521, 0 } }, { -1447, 0, -3843, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3764, -3216, 4924, 0 }, { { 1447, -4240, -514, 0 }, { -1449, -4240, 512, 0 }, { 1447, 4240, -514, 0 }, { -1449, 4240, 512, 0 } }, { 1368, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -3200, 5327, 0 }, { { 980, -4224, 41, 0 }, { -979, -4224, -40, 0 }, { 980, 4224, 41, 0 }, { -979, 4224, -40, 0 } }, { -171, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6210, -3280, 5598, 0 }, { { -979, -4304, 7, 0 }, { 980, -4304, -7, 0 }, { -979, 4304, 7, 0 }, { 980, 4304, -7, 0 } }, { -30, 0, -4117, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5988, -3552, -292, 0 }, { { -959, -4576, 198, 0 }, { 960, -4576, -198, 0 }, { -959, 4576, 198, 0 }, { 960, 4576, -198, 0 } }, { -829, 0, -4015, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6051, -3536, -452, 0 }, { { 949, -4560, -244, 0 }, { -949, -4560, 245, 0 }, { 949, 4560, -244, 0 }, { -949, 4560, 245, 0 } }, { 1025, 0, 3983, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3488, -2833, 0 }, { { -8, -4224, -2308, 0 }, { 8, -4224, 2308, 0 }, { -8, 4224, -2308, 0 }, { 8, 4224, 2308, 0 } }, { 4105, 0, -15, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5072, -3488, -2960, 0 }, { { 8, -4224, 2292, 0 }, { -8, -4224, -2292, 0 }, { 8, 4224, 2292, 0 }, { -8, 4224, -2292, 0 } }, { -4102, 0, 14, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5312, -2848, 0x28E0, 0 }, { { 235, -4224, -1330, 0 }, { -240, -4224, 1325, 0 }, { 235, 4224, -1330, 0 }, { -240, 4224, 1325, 0 } }, { 4031, 0, 720, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3008, 0x2940, 0 }, { { -239, -4224, 1326, 0 }, { 236, -4224, -1329, 0 }, { -239, 4224, 1326, 0 }, { 236, 4224, -1329, 0 } }, { -4033, 0, -722, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1314, -3296, 4926, 0 }, { { 1314, -4256, 675, 0 }, { -1313, -4256, -675, 0 }, { 1314, 4256, 675, 0 }, { -1313, 4256, -675, 0 } }, { -1878, 0, 3651, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1076, -3264, 5229, 0 }, { { -1235, -4256, -651, 0 }, { 1236, -4256, 651, 0 }, { -1235, 4256, -651, 0 }, { 1236, 4256, 651, 0 } }, { 1912, 0, -3632, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5056, -3232, 2256, 0 }, { { 24, -4224, 2036, 0 }, { -24, -4224, -2036, 0 }, { 24, 4224, 2036, 0 }, { -24, 4224, -2036, 0 } }, { -4105, 0, 48, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -2944, 2272, 0 }, { { -24, -4224, -2100, 0 }, { 24, -4224, 2100, 0 }, { -24, 4224, -2100, 0 }, { 24, 4224, 2100, 0 } }, { 4111, 0, -47, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5248, -2912, 6816, 0 }, { { -24, -4224, -1844, 0 }, { 24, -4224, 1844, 0 }, { -24, 4224, -1844, 0 }, { 24, 4224, 1844, 0 } }, { 4101, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5040, -3008, 6912, 0 }, { { -24, -4224, 1844, 0 }, { 24, -4224, -1844, 0 }, { -24, 4224, 1844, 0 }, { 24, 4224, -1844, 0 } }, { -4103, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -384, -2624, 7537, 0 }, { { -1800, -4416, 722, 0 }, { 1793, -4416, -730, 0 }, { -1800, 4416, 722, 0 }, { 1793, 4416, -730, 0 } }, { -1542, 0, -3815, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -448, -2816, 7328, 0 }, { { 1841, -4416, -751, 0 }, { -1852, -4416, 741, 0 }, { 1841, 4416, -751, 0 }, { -1852, 4416, 741, 0 } }, { 1537, 0, 3806, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2465, -2944, 9983, 0 }, { { 211, -4416, -1982, 0 }, { -215, -4416, 1978, 0 }, { 211, 4416, -1982, 0 }, { -215, 4416, 1978, 0 } }, { 4081, 0, 438, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2370, -2976, 0x27BE, 0 }, { { -217, -4416, 1976, 0 }, { 209, -4416, -1985, 0 }, { -217, 4416, 1976, 0 }, { 209, 4416, -1985, 0 } }, { -4083, 0, -440, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -400, -3008, -2064, 0 }, { { -1759, -4416, -117, 0 }, { 1759, -4416, 117, 0 }, { -1759, 4416, -117, 0 }, { 1759, 4416, 117, 0 } }, { 271, 0, -4092, 0 }, { 0, 0, 4096, 0 }, 4748, 0, 21, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -224, -3296, -2176, 0 }, { { 1759, -4416, 117, 0 }, { -1759, -4416, -117, 0 }, { 1759, 4416, 117, 0 }, { -1759, 4416, -117, 0 } }, { -273, 0, 4090, 0 }, { 0, 0, 4096, 0 }, 4748, 0, 2, 21, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1889, -3232, -4353, 0 }, { { 265, -4416, -2218, 0 }, { -265, -4416, 2218, 0 }, { 265, 4416, -2218, 0 }, { -265, 4416, 2218, 0 } }, { 4078, 0, 486, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 21, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1696, -3136, -4480, 0 }, { { -265, -4416, 2218, 0 }, { 265, -4416, -2218, 0 }, { -265, 4416, 2218, 0 }, { 265, 4416, -2218, 0 } }, { -4079, 0, -488, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 2, 21, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_main_street_80187EBC[12] = {
    { NULL, NULL, NULL, { -384, 0, -3712, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2816, 0 }, { { -416, 0, -864, 0 }, { 416, 0, -864, 0 }, { -416, 0, 864, 0 }, { 416, 0, 864, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 957, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6880, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 6000, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, 0x27E0, 0 }, { { -416, 0, -800, 0 }, { 416, 0, -800, 0 }, { -416, 0, 800, 0 }, { 416, 0, 800, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 900, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3584, -48, 0x2A00, 0 }, { { 736, 0, -416, 0 }, { 736, 0, 416, 0 }, { -736, 0, -416, 0 }, { -736, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 97, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -736, -64, 0x2842, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -225, -48, 8767, 0 }, { { -415, 0, -415, 0 }, { 417, 0, -415, 0 }, { -416, 0, 416, 0 }, { 416, 0, 416, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 586, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 115, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5600, -64, -3600, 0 }, { { -1600, 0, -1872, 0 }, { 1600, 0, -1872, 0 }, { -1600, 0, 1873, 0 }, { 1600, 0, 1873, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2305, -64, 8447, 0 }, { { -2192, 0, -2576, 0 }, { 2193, 0, -2576, 0 }, { -2192, 0, 2576, 0 }, { 2193, 0, 2576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 3376, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 17, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5920, -64, -4064, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2962, -64, 7199, 0 }, { { -624, 0, -512, 0 }, { 625, 0, -512, 0 }, { -624, 0, 513, 0 }, { 625, 0, 513, 0 } }, { 0, 4096, 0, 0 }, { -2598, 0, -3166, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 19, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_main_street_8018824C[12] = {
    { NULL, NULL, NULL, { -384, 0, -3712, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2816, 0 }, { { -416, 0, -864, 0 }, { 416, 0, -864, 0 }, { -416, 0, 864, 0 }, { 416, 0, 864, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 957, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6880, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 6000, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, 0x27E0, 0 }, { { -416, 0, -800, 0 }, { 416, 0, -800, 0 }, { -416, 0, 800, 0 }, { 416, 0, 800, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 900, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3584, -48, 0x2A00, 0 }, { { 736, 0, -416, 0 }, { 736, 0, 416, 0 }, { -736, 0, -416, 0 }, { -736, 0, 416, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, -4096, 0 }, 844, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 97, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -736, -64, 0x2842, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -929, -48, 8303, 0 }, { { -415, 0, -687, 0 }, { 417, 0, -687, 0 }, { -416, 0, 688, 0 }, { 416, 0, 688, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 801, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 115, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5600, -64, -3600, 0 }, { { -1600, 0, -1872, 0 }, { 1600, 0, -1872, 0 }, { -1600, 0, 1873, 0 }, { 1600, 0, 1873, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 2455, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 16, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2305, -64, 8447, 0 }, { { -2192, 0, -2576, 0 }, { 2193, 0, -2576, 0 }, { -2192, 0, 2576, 0 }, { 2193, 0, 2576, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 3376, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 17, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5920, -64, -4064, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2962, -64, 7199, 0 }, { { -624, 0, -512, 0 }, { 625, 0, -512, 0 }, { -624, 0, 513, 0 }, { 625, 0, 513, 0 } }, { 0, 4096, 0, 0 }, { -2598, 0, -3166, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 19, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_night_main_street_801885DC[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4417, -2000, 2032 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4417, -2000, -1229 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -878, -2000, -2607 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 3686 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -878, -2000, 7297 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 3686 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4253, -2000, 0x2A75 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 9652 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 6623 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1664, -2000, 9386 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, -3140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3404 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 316 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 2000 },
};

WorldCoordRoomLights D_dryfield_night_main_street_8018899C[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_main_street_801885DC), D_dryfield_night_main_street_801885DC, 0, NULL },
};

AreaResource D_dryfield_night_main_street_801889B4[3] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { 8, 7, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_300700_80165B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_main_street_801889D8[2] = {
    { 106, 361, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_136100_80140744 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_main_street_801889F0[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_main_street_80188A08[13] = {
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

WorldCoordRoomAmbientEntry D_dryfield_night_main_street_80188A70[25] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_main_street_80188A70) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 617, 618, 616 } },
    { .color = { 616, 615, 615, 615 } },
    { .color = { 617, 618, 617, 617 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 617, 615, 616, 615 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_night_main_street_80188B38 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_dryfield_night_main_street_80188B44 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_dryfield_night_main_street_80188B50 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionSurfaceProperties D_dryfield_night_main_street_80188B5C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_main_street_80188B64[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_main_street_80188B38 },
};

WorldCollisionSurfaceProperties D_dryfield_night_main_street_80188B6C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_main_street_80188B44 },
};

WorldCollisionSurfaceProperties D_dryfield_night_main_street_80188B74[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_main_street_80188B50 },
};

WorldCollisionSurfaceProperties D_dryfield_night_main_street_80188B7C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_night_main_street_80188B84[8] = {
    D_dryfield_night_main_street_80188B5C,
    D_dryfield_night_main_street_80188B7C,
    D_dryfield_night_main_street_80188B6C,
    D_dryfield_night_main_street_80188B74,
    D_dryfield_night_main_street_80188B64,
    D_dryfield_night_main_street_80188B5C,
    D_dryfield_night_main_street_80188B5C,
    D_dryfield_night_main_street_80188B5C,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

RoomEventStartStorage gMainStreetEventSpawned = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

RoomLatchedEventStorage gRoomEventLatched;

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_staged_task.inc.c"

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_night_main_street_8017D5F4 = {
    { func_dryfield_night_main_street_8017E064, func_dryfield_night_main_street_8017E0B8, taskKill },
};

#include "../../shared/main_street_resolve_msg.inc.c"

#include "../../shared/main_street_play_time_task.inc.c"

#include "../../shared/main_street_cap_sound_cue.inc.c"

#include "../../shared/main_street_talk_msg.inc.c"

s32 func_dryfield_night_main_street_8017E054(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_night_main_street_8017E05C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// pointer slot 7, runs the room's message-table pass, then advances state and
/// raises the `D_80115598` flag.
static void func_dryfield_night_main_street_8017E064(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_main_street_801820B0;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
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
        gRoomEffectMoteId         = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_MOTE;
        gRoomEffectHaloId         = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_HALO;
        gRoomEffectOrangeBurstId  = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_ORANGE_BURST;
        gRoomEffectSparkEmitterId = EFFECT_DRYFIELD_NIGHT_MAIN_STREET_SPARK_EMITTER;
        task->state               = 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_07F) != 0) {
        D_dryfield_night_main_street_80182230[3] = 0;
        D_dryfield_night_main_street_80182230[2] = 0;
    }
    if (mask & D_dryfield_night_main_street_80182230[0]) {
        glowDrawShaft(D_dryfield_night_main_street_801821A8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[2]) {
        glowDrawShaft(&D_dryfield_night_main_street_801821B8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[4]) {
        glowDrawShaft(&D_dryfield_night_main_street_801821C8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[6]) {
        glowDrawShaft(&D_dryfield_night_main_street_801821D8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[8]) {
        glowDrawShaft(&D_dryfield_night_main_street_801821E8, 0x180);
    }
    for (i = 10; i < 16; i++) {
        if (mask & D_dryfield_night_main_street_80182230[i]) {
            glowDrawFlare(&D_dryfield_night_main_street_801821A8[i], 1, 0x380);
        }
    }
    gRoomEffectState->roomEffectMode = D_dryfield_night_main_street_80182178[(Gp_GetViewIndex() & 0xFF) - 1];
    if ((Gp_GetViewIndex() & 0xFF) == 8 || (Gp_GetViewIndex() & 0xFF) == 0x13) {
        if (task->spawnArg1.value != (Gp_GetViewIndex() & 0xFF)) {
            for (i = 0; i < 0x30; i++) {
                D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
                D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
                D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MAIN_STREET_PUFF, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) + 0x103100,
                            &D_dryfield_night_main_street_801821A8[16]);
            }
        } else if (gDisplayState.animFrame & 1) {
            D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
            D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
            D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
            Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MAIN_STREET_PUFF, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) | 0x82100,
                        &D_dryfield_night_main_street_801821A8[16]);
        }
    }
    task->spawnArg1.value = Gp_GetViewIndex() & 0xFF;
}

#include "../../shared/glow_draw_shaft.inc.c"

#include "../../shared/glow_draw_flare.inc.c"

#include "../../shared/main_street_puff_task.inc.c"

#include "../../shared/main_street_draw_puff.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_dryfield_night_main_street_8017FA68(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_dryfield_night_main_street_801807B0(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_dryfield_night_main_street_80180B48(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_dryfield_night_main_street_80181F58(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}
