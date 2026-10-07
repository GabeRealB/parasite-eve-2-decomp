#include "rooms/neo_ark_observatory.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
#include "../../shared/room_variants.h"
#include "../../shared/follow_collision.h"

static s32 _roomVariantResolveShelter(RoomEventMsg* request, RoomEventMsg* reply);

/// The clip the room adds to the player's animation bank, with the event
/// scene's records stored after it.
///
/// The scene script sends `data.copy` to the player before it plays the clip.
/// The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of
/// the storage, which is more than the one-entry clip table holds: the set
/// pointer occupies extended id 47, and the copy request, the play request and
/// the script's first four commands are written into the bank after it. The
/// play request selects id 47 only, so none of those words is played as a clip.
///
/// The script is not animation-bank data. It is part of this object because
/// the copied span ends inside it.
typedef union {
    struct {
        AnimationSet*            sets[1];        // Player clip for extended id 47
        AnimationBankCopyRequest copy;           // Installs the first `ANIMATION_BANK_EXTENSION_CAPACITY` words of this storage in the player's bank extension
        AnimationPlayRequest     playRequest;    // Blends the player into extended id 47 over 15 frames
        EvsCommand               sceneScript[8]; // The room's one-time event scene: starts CAP sequence 12, installs and plays the clip, waits for the CAP cue and ends the player's scripted control
    } data;                                      // The records by name
    s32 words[56];                               // The same storage as the copy reads it; the last 24 words lie beyond the copied span
} _NeoArkObservatoryAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_NeoArkObservatoryAnimationBankExtensionStorage, 224);

extern _NeoArkObservatoryAnimationBankExtensionStorage D_neo_ark_observatory_801811E0;

extern EvsCommand D_actor_450200_80137EE4[];
extern EvsCommand D_actor_450200_80138694[];
extern EvsCommand D_actor_450200_8013C72C[];
extern EvsCommand D_actor_450200_8013CAEC[];
extern EvsCommand D_actor_450200_8013FC58[];
extern EvsCommand D_actor_450200_80140078[];

extern void func_actor_450200_80132220(void);
extern void func_actor_450200_801322F8(void);

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
static u8 Reflection_Data_8017FC8C[];

/// The departure task's descriptor.
extern TaskDesc D_neo_ark_observatory_80180DD4;

extern EvsCommand D_neo_ark_observatory_801812C0[];

/// Descriptor of the cap-file task `func_neo_ark_observatory_8017FB1C`.
extern TaskDesc D_neo_ark_observatory_801811AC;

/// Messages the room task answers, terminated by id `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_neo_ark_observatory_801811B8[];

/// Offset `func_neo_ark_observatory_8017FA98` hands the mesh rebuild; only its
/// `vy` is ever set.
extern SVECTOR D_neo_ark_observatory_80181368;

extern WorldCollisionGrid gFollowCollisionSource;
extern WorldCollisionGrid gFollowCollisionGrid;
extern SVECTOR            D_neo_ark_observatory_80181434[];
extern SVECTOR            D_neo_ark_observatory_801814E4[];
extern SVECTOR            D_neo_ark_observatory_801814F4[];
extern SVECTOR            D_neo_ark_observatory_801814FC[];
extern SVECTOR            D_neo_ark_observatory_8018150C[];
extern SVECTOR            D_neo_ark_observatory_8018151C[];
extern SVECTOR            D_neo_ark_observatory_80181524[];
extern SVECTOR            D_neo_ark_observatory_80181564[];
extern SVECTOR            D_neo_ark_observatory_80181574[];
extern SVECTOR            D_neo_ark_observatory_8018157C[];

extern AreaApplyRec  D_neo_ark_observatory_80187A28[];
extern RoomDeparture gRoomDeparture;
extern s16           D_neo_ark_observatory_80187A3C;

/// Defines the reflection scale at the shared implementation's include position.
///
/// 1 lets `planar_reflection.inc.c` include `planar_reflection_rodata.inc.c`.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 1
static void _neoArkObservatoryPlayerReflectionTask(Task* reflectionTask);
#include "../../shared/planar_reflection.h"

static void _neoArkObservatoryDrawLightBeam(const SVECTOR ringCenters[2], s32 outerRadius, s16 baseIntensity, s16 segmentCount);

/// Projects one beam segment's four world-space corners to screen pixels.
///
/// Borrows a live, word-aligned `quadScratch` block for this call. All four
/// vertices' XYZ components must be initialized as signed 16-bit world
/// coordinates in GPU quad strip order. Loads `gGfxViewCoord.workm`'s rotation;
/// the caller must load its translation and configure GTE projection first.
///
/// Writes all four `screenCorners` and the final RTPT `projectionFlags`, even
/// on rejection. Corner 0's RTPS flags are discarded; the caller rejects a
/// negative final FLAG. Leaves `vertices` and `depth` untouched, with corner
/// 3's depth in GTE SZ3 for reading before another depth-changing GTE command.
/// GTE state is not restored. Reserves no storage and retains no pointer.
static inline void _neoArkObservatoryProjectBeamSegment(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    // Save corner 0 before the triple transform replaces the screen FIFO.
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

enum { NEO_ARK_OBSERVATORY_MESSAGE_USE_KEY_ITEM = 0x13F1 };

extern WorldCollisionGrid     gFollowCollisionGrid;
extern WorldCollisionOccluder D_neo_ark_observatory_801878D4[4];
extern WorldCollisionTrigger  D_neo_ark_observatory_80186ED4[18];
extern WorldCollisionTrigger  D_neo_ark_observatory_8018742C[14];
extern WorldCoordRoomLights   D_neo_ark_observatory_80186844[1];
extern WorldCoordRoomLights   D_neo_ark_observatory_80186EBC[1];

s32        func_neo_ark_observatory_8017F6F8(Task* task, s32 msgId, const void* firstArg, s32);
static s32 _neoArkObservatoryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg);
s32        func_neo_ark_observatory_8017FBE8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32        func_neo_ark_observatory_8017FCA0(Task*, s32, s32, s32);

void func_neo_ark_observatory_8017FB1C(Task*);

#include "../../shared/planar_reflection_data.inc.c"

TaskDesc D_neo_ark_observatory_80180DBC[2] = {
    { { { TASK_BODY_NONE, 112 } }, _neoArkObservatoryPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 112 } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return D_neo_ark_observatory_80180DBC;
}

TaskDesc D_neo_ark_observatory_80180DD4 = { { { TASK_BODY_NONE, 32 } }, roomDepartureTask, { .value = 0 } };

static AnimationPackedPose _gNeoArkObservatoryAnimation03BC4Bank1[6] = {
#include "assets/neo_ark_observatory_animation_03BC4_bank1.inc"
};

static AnimationPackedRotation _gNeoArkObservatoryAnimation03BC4Bank4[64] = {
#include "assets/neo_ark_observatory_animation_03BC4_bank4.inc"
};

static AnimationRecord _gNeoArkObservatoryAnimation03BC4Records[141] = {
#include "assets/neo_ark_observatory_animation_03BC4_records.inc"
};

static u16 _gNeoArkObservatoryAnimation03BC4Indices[20] = {
#include "assets/neo_ark_observatory_animation_03BC4_indices.inc"
};

static AnimationSet _gNeoArkObservatoryAnimation03BC4 = {
    _gNeoArkObservatoryAnimation03BC4Records,
    _gNeoArkObservatoryAnimation03BC4Indices,
    { NULL, _gNeoArkObservatoryAnimation03BC4Bank1, NULL, NULL, _gNeoArkObservatoryAnimation03BC4Bank4, NULL, NULL, NULL },
};

TaskDesc D_neo_ark_observatory_801811AC = { { { TASK_BODY_NONE, 192 } }, func_neo_ark_observatory_8017FB1C, { .value = 0 } };

TaskMessageEntry D_neo_ark_observatory_801811B8[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_observatory_8017FBE8 },
    { NEO_ARK_OBSERVATORY_MESSAGE_USE_KEY_ITEM, _neoArkObservatoryRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_observatory_8017F6F8 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_observatory_8017FCA0 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

_NeoArkObservatoryAnimationBankExtensionStorage D_neo_ark_observatory_801811E0 = { .data = { { &_gNeoArkObservatoryAnimation03BC4 }, { { .words = D_neo_ark_observatory_801811E0.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_ENABLE }, { { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } }, { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_neo_ark_observatory_801811E0.data.copy } }, { .value = 0 } }, { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_observatory_801811E0.data.playRequest }, { .value = 0 } }, { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55070009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } }, { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } }, { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } }, { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } }, { .opcode = EVENT_SCRIPT_OPCODE_END } } } };

EvsCommand D_neo_ark_observatory_801812C0[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_neo_ark_observatory_80181368 = { 0, 0, -200, 0 };

static SVECTOR _gNeoArkObservatoryCollision03E50Normals[4] = {
#include "assets/neo_ark_observatory_collision_03E50_normals.inc"
};

static SVECTOR _gNeoArkObservatoryCollision03E50Verts[8] = {
#include "assets/neo_ark_observatory_collision_03E50_verts.inc"
};

static WorldCollisionGridFace _gNeoArkObservatoryCollision03E50Faces[4] = {
#include "assets/neo_ark_observatory_collision_03E50_faces.inc"
};

static s16 _gNeoArkObservatoryCollision03E50Cells[6] = {
#include "assets/neo_ark_observatory_collision_03E50_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkObservatoryCollision03E50Cells[i])
static s16* _gNeoArkObservatoryCollision03E50Table[1] = {
#include "assets/neo_ark_observatory_collision_03E50_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFollowCollisionSource = { NULL, _gNeoArkObservatoryCollision03E50Normals, _gNeoArkObservatoryCollision03E50Verts, _gNeoArkObservatoryCollision03E50Faces, _gNeoArkObservatoryCollision03E50Table, 399, 500, 1, 1, 4000, 4 };

SVECTOR D_neo_ark_observatory_80181434[22] = {
    { 512, -5120, 8192, 0 },
    { 512, -1024, 8192, 0 },
    { 1536, -5120, 8448, 0 },
    { 1536, -1024, 8448, 0 },
    { 2560, -5120, 8192, 0 },
    { 2560, -1024, 8192, 0 },
    { 3840, -5120, 9216, 0 },
    { 3840, -1024, 9216, 0 },
    { 4096, -5120, 8448, 0 },
    { 4096, -1024, 8448, 0 },
    { 3840, -5120, 7424, 0 },
    { 3840, -1024, 7424, 0 },
    { 768, -5120, 7936, 0 },
    { 768, -1024, 7936, 0 },
    { 1536, -5120, 7680, 0 },
    { 1536, -1024, 7680, 0 },
    { 2816, -5120, 7936, 0 },
    { 2816, -1024, 7936, 0 },
    { 4096, -3584, 1536, 0 },
    { 4096, 0, 1536, 0 },
    { 4096, -3584, 0x3600, 0 },
    { 4096, 0, 0x3600, 0 },
};

SVECTOR D_neo_ark_observatory_801814E4[2] = {
    { 0x2EE0, -3000, 0x2EE0, 0 },
    { 1000, -3000, 0x2EE0, 0 },
};

SVECTOR D_neo_ark_observatory_801814F4[1] = {
    { 8000, -3000, 0x2EE0, 0 },
};

SVECTOR D_neo_ark_observatory_801814FC[2] = {
    { 8000, -3000, 0x2710, 0 },
    { 8000, -3000, 8000, 0 },
};

SVECTOR D_neo_ark_observatory_8018150C[2] = {
    { 8000, -3000, 6000, 0 },
    { 8000, -3000, 4000, 0 },
};

SVECTOR D_neo_ark_observatory_8018151C[1] = {
    { 5640, -210, 3030, 0 },
};

SVECTOR D_neo_ark_observatory_80181524[8] = {
    { 5640, -210, 2580, 0 },
    { 5640, -210, 30, 0 },
    { 5640, -210, 0x325A, 0 },
    { 5640, -210, 0x341C, 0 },
    { 5640, -210, 0x3E12, 0 },
    { 4110, -70, 360, 0 },
    { 2380, -70, 1400, 0 },
    { 980, -70, 3160, 0 },
};

SVECTOR D_neo_ark_observatory_80181564[2] = {
    { 60, -70, 5640, 0 },
    { -240, -70, 8000, 0 },
};

SVECTOR D_neo_ark_observatory_80181574[1] = {
    { 70, -70, 0x292C, 0 },
};

SVECTOR D_neo_ark_observatory_8018157C[3] = {
    { 970, -70, 0x321E, 0 },
    { 2380, -70, 0x38FE, 0 },
    { 4120, -70, 0x3D0E, 0 },
};

WorldCollisionRoomResources D_neo_ark_observatory_80181594[2] = {
    { &gFollowCollisionGrid, D_neo_ark_observatory_80186ED4, D_neo_ark_observatory_8018742C, D_neo_ark_observatory_801878D4 },
    { &gFollowCollisionGrid, D_neo_ark_observatory_80186ED4, D_neo_ark_observatory_8018742C, D_neo_ark_observatory_801878D4 },
};

WorldCoordRoomLighting D_neo_ark_observatory_801815B4[2] = {
    { D_neo_ark_observatory_80186844, NULL },
    { D_neo_ark_observatory_80186EBC, NULL },
};

u8 D_neo_ark_observatory_801815C4[24] = {
    1,
    2,
    3,
    16,
    17,
    18,
    7,
    8,
    9,
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
    21,
    0,
    0,
    0,
};

u8* D_neo_ark_observatory_801815DC[2] = {
    D_neo_ark_observatory_801815C4,
    gViewIdentityMap,
};

ViewCount D_neo_ark_observatory_801815E4[2] = { 21, 21 };

DirectionWarpEntry D_neo_ark_observatory_801815E8[3] = {
    { { { .word = 3072 }, 0x34BC, 0, 0x2EE0 }, { 0, 0, 0, 0 }, { { .word = 2560 }, 0x2D82, 0, 0x316A }, { 0, 0, 0, 0 }, 0x55070006, 0x55070005, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_B2_MAIN_CORRIDOR },
    { { { .word = 3072 }, 6700, 0, 0x37DC }, { 0, 0, 0, 0 }, { { .word = 3072 }, 6700, 0, 0x37DC }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 11, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 3072 }, 6490, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 6490, 0, 1700 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 10, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gNeoArkObservatoryCollision049E4Normals[26] = {
#include "assets/neo_ark_observatory_collision_049E4_normals.inc"
};

static SVECTOR _gNeoArkObservatoryCollision049E4Verts[106] = {
#include "assets/neo_ark_observatory_collision_049E4_verts.inc"
};

static WorldCollisionGridFace _gNeoArkObservatoryCollision049E4Faces[48] = {
#include "assets/neo_ark_observatory_collision_049E4_faces.inc"
};

static s16 _gNeoArkObservatoryCollision049E4Cells[306] = {
#include "assets/neo_ark_observatory_collision_049E4_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkObservatoryCollision049E4Cells[i])
static s16* _gNeoArkObservatoryCollision049E4Table[20] = {
#include "assets/neo_ark_observatory_collision_049E4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid gFollowCollisionGrid = { NULL, _gNeoArkObservatoryCollision049E4Normals, _gNeoArkObservatoryCollision049E4Verts, _gNeoArkObservatoryCollision049E4Faces, _gNeoArkObservatoryCollision049E4Table, 0, 0, 4, 5, 4000, 48 };

ViewCamera D_neo_ark_observatory_80181FC8[21] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7000, 0x61A8, -8000 } }, 329 },
    { { { { -972, 0, -3978 }, { 543, 4057, -133 }, { 3941, -559, -963 } }, { -6480, 550, -0x305C } }, 289 },
    { { { { 3999, 0, -882 }, { -171, 4018, -775 }, { 865, 794, 3923 } }, { -7450, 2250, -5570 } }, 289 },
    { { { { -4077, 0, -389 }, { -13, 4093, 145 }, { 389, 146, -4074 } }, { -7450, 1570, -0x32B4 } }, 289 },
    { { { { -4072, 0, -434 }, { 55, 4062, -517 }, { 430, -520, -4039 } }, { -7450, 820, -9330 } }, 289 },
    { { { { -635, 0, -4046 }, { -426, 4073, 66 }, { 4023, 431, -632 } }, { -2310, 1470, -4610 } }, 289 },
    { { { { -3915, 0, -1202 }, { 195, 4041, -635 }, { 1186, -664, -3863 } }, { -660, 970, -0x2A9E } }, 289 },
    { { { { -200, 0, 4091 }, { 2927, 2861, 143 }, { -2858, 2930, -139 } }, { -5900, 5070, -8460 } }, 289 },
    { { { { 3816, 0, -1485 }, { 238, 4042, 613 }, { 1466, -658, 3767 } }, { -660, 970, -5150 } }, 289 },
    { { { { 912, 0, 3993 }, { 1882, 3612, -429 }, { -3521, 1930, 804 } }, { -0x2742, 2930, -450 } }, 289 },
    { { { { -821, 0, 4012 }, { 1903, 3605, 389 }, { -3532, 1942, -723 } }, { -0x2742, 2930, -0x3C6E } }, 289 },
    { { { { -481, 0, 4067 }, { -759, 4023, -89 }, { -3996, -764, -472 } }, { -0x3548, 550, -0x2FC6 } }, 289 },
    { { { { -1278, 0, -3891 }, { -2947, 2674, 967 }, { 2541, 3101, -834 } }, { -8640, 3230, -0x319C } }, 329 },
    { { { { 635, 0, 4046 }, { -751, 4024, 118 }, { -3975, -760, 624 } }, { -0x3548, 550, -0x2C4C } }, 289 },
    { { { { 1193, 0, -3918 }, { -3328, 2160, -1014 }, { 2066, 3479, 629 } }, { -8150, 3980, -0x2D32 } }, 329 },
    { { { { -4077, 0, -389 }, { -13, 4093, 145 }, { 389, 146, -4074 } }, { -7450, 1570, -0x32B4 } }, 289 },
    { { { { -4072, 0, -434 }, { 55, 4062, -517 }, { 430, -520, -4039 } }, { -7450, 820, -9330 } }, 289 },
    { { { { -635, 0, -4046 }, { -426, 4073, 66 }, { 4023, 431, -632 } }, { -2310, 1470, -4610 } }, 289 },
    { { { { -3915, 0, -1202 }, { 195, 4041, -635 }, { 1186, -664, -3863 } }, { -660, 970, -0x2A9E } }, 289 },
    { { { { -200, 0, 4091 }, { 2927, 2861, 143 }, { -2858, 2930, -139 } }, { -5900, 5070, -8460 } }, 289 },
    { { { { -803, 0, 4016 }, { 3001, 2721, 600 }, { -2668, 3061, -534 } }, { -3016, 3531, -7604 } }, 329 },
};

SpriteBatch D_neo_ark_observatory_801822BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_observatory_801822CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_801822DC[21] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1450, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1450, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1450, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1450, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1450, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1450, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1450, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1450, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -120, 1450, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1450, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -120, 1450, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -72, 1450, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1450, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 24, 1450, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, 24, 1450, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 48, 1500, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -24, 1450, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -24, 1500, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -72, 1500, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, -48, 1500, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, -120, 1500, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_80182480[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80182498[19] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 16, 1950, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -32, 1950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -80, 1950, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, -120, 1950, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 64, -120, 1950, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -80, 1950, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -32, 1950, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 16, 1950, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, 16, 1950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -32, 1950, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 48, -80, 1950, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 1950, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 40, -120, 1950, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, -80, 1950, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, -32, 1950, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 16, 1950, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, 16, 2075, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 32, -32, 2075, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, -80, 2075, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_80182614[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_8018262C[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -88, 1200, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 56, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1125, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 1125, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 72, -120, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -88, 1125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -40, 1125, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 8, 1125, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 56, 1125, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 56, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 8, 1125, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -40, 1125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -88, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 1125, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -120, 1200, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -48, 1200, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -40, 1200, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 1200, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 56, 1200, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_801827D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_801827E8[82] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, -80, 975, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -72, 987, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -8, 1275, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 1000, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 912, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -120, 931, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 912, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 931, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 912, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -24, 931, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 912, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, 24, 931, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, 72, 912, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, 72, 931, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -160, 88, 912, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -120, 937, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -72, 1000, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -24, 1000, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, 24, 1000, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -72, 1150, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -24, 1150, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1150, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 1275, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -48, -16, 1275, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -88, 950, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -88, 950, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -120, 1025, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -120, 1025, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 40, -120, 1037, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -72, 1025, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -24, 1025, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 1025, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 24, 1037, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1025, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -64, -120, 1025, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, -120, 848, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, -120, 1125, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 862, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 831, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 1100, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -120, 825, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -72, 825, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -24, 825, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 104, 24, 825, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 104, 72, 825, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -120, 950, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -72, 950, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -24, 950, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, 24, 950, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, 72, 72, 950, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 24, 1100, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -24, 1100, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -72, 1100, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 1100, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -120, 1125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -72, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -24, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 24, 1125, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1325, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 40, 64, 1187, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 1187, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -32, 1187, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, 16, 1187, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, 16, 1312, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -32, 1312, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -80, 1312, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 64, 1175, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 56, 1435, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 0, 48, 1551, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, 40, 1633, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 32, 1712, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 64, 1303, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 56, 1419, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 48, 1599, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 40, 1633, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 32, 1712, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 1303, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 56, 1419, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 48, 1599, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 40, 1633, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 1712, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_80182E50[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { 26, 10, 0, 0, { 3, 0 } },
    { 36, 23, 0, 0, { 2, 0 } },
    { 59, 8, 0, 0, { 4, 0 } },
    { 67, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80182E88[83] = {
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -160, 64, 1550, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 1550, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -8, 1550, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -48, 1550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -88, 1550, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -128, -48, 1875, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 1875, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -72, 1875, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -48, 2150, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -104, -8, 2150, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -104, 16, 2150, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 2150, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 1875, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -112, 48, 1875, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, -48, 2325, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -56, 2150, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 80, 2375, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 32, 2375, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, 32, 2375, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, 80, 2375, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 32, 2375, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -16, 2375, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -16, 2375, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -16, 2375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -160, -24, 2375, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -112, -24, 2375, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, -24, 2375, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 1250, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, 72, 1250, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 1250, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 40, 1250, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1250, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -80, -8, 1250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 1250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1250, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 1250, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -112, -88, 1250, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 80, 812, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, 96, 775, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 40, 812, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 48, 775, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, -160, 16, 800, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -128, 24, 850, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 24, 862, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 1062, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 1300, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -152, 32, 1300, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, 72, 1200, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -136, 32, 1200, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -112, 32, 1062, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 72, 1062, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -72, 2554, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -48, 2350, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, -48, 2350, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, -48, 2350, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, -48, 2350, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -152, -64, 2425, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, -64, 2425, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, -64, 2425, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -64, 2425, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, -64, 2562, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -88, -72, 2500, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -72, 2575, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 32, 987, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 72, 987, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 72, 925, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 88, 40, 987, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 40, 925, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 40, 925, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, 16, 1000, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 128, 16, 1000, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 144, 8, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 96, 1096, { .fields = { 96, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, 48, 796, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 48, 24 } }, -160, 96, 789, { .fields = { 104, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, -112, 48, 960, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -112, 96, 956, { .fields = { 56, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, 48, 1106, { .fields = { 96, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 40 } }, -152, 48, 1437, { .fields = { 48, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, -136, 48, 1437, { .fields = { 40, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 48 } }, -112, 48, 1300, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_observatory_80183504[11] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 5, 0 } },
    { 27, 12, 0, 0, { 4, 0 } },
    { 39, 7, 0, 0, { 6, 0 } },
    { 46, 7, 0, 0, { 1, 0 } },
    { 53, 12, 0, 0, { 7, 0 } },
    { 65, 9, 0, 0, { 3, 0 } },
    { 74, 6, 0, 0, { 8, 0 } },
    { 80, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_8018355C[94] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 48, 64, 812, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 875, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 750, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 72, 750, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 56, 750, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 56, 750, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 48, 750, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 40, 750, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 40, 750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 32, 750, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 32, 750, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 32, 750, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -8, 32, 750, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 8, 40, 750, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 40, 750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 40, 48, 750, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 64, 750, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 88, 72, 750, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 80, 750, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 120, 96, 750, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -152, 104, 875, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -136, 96, 875, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -112, 88, 875, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 88, 875, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -64, 88, 875, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -32, 88, 875, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 88, 875, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 32, 88, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, 88, 875, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 72, 96, 875, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 72, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, 72, 875, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 72, 875, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, 72, 875, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 72, 734, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, 72, 875, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 56, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, 72, 812, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -56, 64, 875, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 64, 1236, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, 32, 625, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -96, 48, 625, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 48, 625, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 625, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 88, 625, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 88, 625, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -64, 88, 625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -32, 88, 625, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 88, 625, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 88, 625, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 64, 88, 625, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 88, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 88, 625, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 56, 625, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, 56, 625, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 56, 625, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -64, 64, 625, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, 64, 625, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 0, 56, 625, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 56, 625, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 72, 56, 625, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, 56, 943, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 56, 625, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 128, 48, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, 40, 625, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, 40, 625, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 40, 625, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 32, 625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -128, 32, 625, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 80, 80, 800, { .fields = { 16, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 8 } }, 72, 104, 800, { .fields = { 24, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 8 } }, -136, 112, 800, { .fields = { 96, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -48, 48, 800, { .fields = { 88, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -16, 48, 800, { .fields = { 112, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -16, 64, 800, { .fields = { 64, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -80, 48, 800, { .fields = { 64, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 16, 48, 800, { .fields = { 64, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, 16, 64, 800, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -80, 64, 800, { .fields = { 96, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -48, 64, 800, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -104, 56, 800, { .fields = { 48, 184 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, -112, 64, 800, { .fields = { 112, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, 48, 64, 800, { .fields = { 112, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 48, 88, 800, { .fields = { 72, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, 88, 800, { .fields = { 16, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, -112, 88, 800, { .fields = { 80, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 24 } }, -136, 64, 800, { .fields = { 0, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 24 } }, -136, 88, 800, { .fields = { 120, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 24 } }, -160, 88, 800, { .fields = { 120, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 24, 8 } }, -160, 112, 800, { .fields = { 112, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 8 } }, -152, 80, 800, { .fields = { 24, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 80, 88, 800, { .fields = { 40, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 16 } }, 80, 104, 800, { .fields = { 40, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 16, 16 } }, 112, 104, 800, { .fields = { 80, 232 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_observatory_80183CB4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 1, 0 } },
    { 40, 29, 0, 0, { 2, 0 } },
    { 69, 25, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80183CDC[69] = {
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 32, 32, 2250, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 80, 32, 2250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 32, 2250, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 32, -16, 2250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 80, -16, 2250, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -16, 2250, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 32, -24, 2284, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, -24, 1302, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 1200, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1200, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1200, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1200, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 112, -88, 1200, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -112, 1200, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -72, 1200, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, -80, 1200, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -24, 1200, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 24, 1200, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, 72, 1200, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 72, 1200, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -24, 1200, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 72, -64, 1200, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -16, 1200, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -8, 1200, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -8, 1200, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, -48, 1200, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 56, 24, 750, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 24, 737, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 16, 700, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 128, 16, 650, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 40, 700, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 112, 64, 700, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 96, 700, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 96, 700, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 48, 700, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 1400, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 112, 72, 1400, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 112, 96, 1400, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 112, 48, 1400, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, 32, 1400, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 72, 1350, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 96, 1350, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 96, 1250, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 72, 1250, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 48, 1250, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 80, 48, 1350, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 80, 32, 1350, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 24, 1250, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 32, -56, 2309, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 72, -56, 1985, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 112, -56, 1251, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1496, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 112, -16, 1283, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -24, 1216, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -16, 1604, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 32, -72, 2505, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -64, 2772, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -72, 2578, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 16, -56, 2763, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, 48, 650, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 48, 24 } }, 112, 96, 650, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 32, 48 } }, 80, 48, 700, { .fields = { 96, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 32, 24 } }, 80, 96, 700, { .fields = { 48, 208 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 48 } }, 64, 48, 750, { .fields = { 0, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 16 } }, 64, 96, 750, { .fields = { 96, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, 56, 48, 800, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 48 } }, 56, 48, 1300, { .fields = { 88, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 24, 48 } }, 72, 48, 1300, { .fields = { 72, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 40 } }, 96, 48, 1350, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_observatory_80184240[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 18, 0, 0, { 4, 0 } },
    { 26, 9, 0, 0, { 1, 0 } },
    { 35, 13, 0, 0, { 5, 0 } },
    { 48, 11, 0, 0, { 0, 0 } },
    { 59, 7, 0, 0, { 6, 0 } },
    { 66, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80184288[76] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 72, 1150, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, 24, 1150, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -24, 1150, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -72, 1150, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -120, 1150, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, 72, 1250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, 24, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, -24, 1250, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, -72, 1250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -128, -120, 1250, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -120, 1325, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -72, 1325, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, -40, 1395, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, -24, 1325, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -24, 1395, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 24, 1325, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -104, 72, 1325, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 72, 1395, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, 24, 1395, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 48, -80, 1550, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 975, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 975, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 975, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 975, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 975, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -120, 1175, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -72, 1175, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -24, 1175, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 24, 1175, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 72, 1175, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, -120, 1350, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, -72, 1350, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 48, -72, 1550, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, -24, 1550, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, -24, 1350, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 56, 24, 1350, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 64, 72, 1350, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 104, 1225, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -8, 104, 1237, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 96, 1137, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 96, 1150, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 88, 1150, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 80, 1112, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 88, 1162, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 80, 1112, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 72, 1050, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, 104, 1287, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 104, 1300, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, 96, 1212, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, 88, 1150, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, 80, 1100, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 40, 72, 1062, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 40, 64, 1062, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 96, 1162, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 88, 1137, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 88, 80, 1087, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 88, 72, 1075, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 88, 64, 1050, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 104, 1137, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -24, 112, 1137, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, -24, 1025, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, -24, 1025, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, 8, 1037, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, 8, 1037, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -56, 40, 1087, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -56, 80, 1100, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -40, 80, 1125, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 112, 1125, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 112, 1125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, 40, 1112, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -72, 40, 1087, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, 8, 1050, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -88, 80, 1175, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 1175, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -72, 48, 1187, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -72, 88, 1200, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_80184878[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 18, 0, 0, { 0, 0 } },
    { 37, 21, 0, 0, { 5, 0 } },
    { 58, 14, 0, 0, { 1, 0 } },
    { 72, 2, 0, 0, { 4, 0 } },
    { 74, 2, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_801848B8[78] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, -80, 1450, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 72, 1200, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 24, 1200, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -24, 1200, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -72, 1200, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -120, 1200, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -120, 1275, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -72, 1275, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -24, 1275, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, 24, 1275, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, 72, 1275, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 72, 1350, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 24, 1350, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -24, 1350, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -72, 1350, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, -120, 1350, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -72, 1450, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -24, 1450, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 24, 1450, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 72, 1450, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 1037, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 1037, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 1037, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 1037, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 1037, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, -120, 1212, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1212, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1212, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, 24, 1212, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, 72, 1370, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -112, 72, 1212, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -24, 1370, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -72, 1370, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -120, 1370, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -96, 1450, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, -72, 1500, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 1500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, 24, 1370, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -104, 56, 1036, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 8, 104, 1275, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -40, 104, 1287, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -88, 104, 1287, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 104, 1287, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 96, 1275, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -88, 96, 1275, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 96, 1145, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 96, 1145, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 88, 1125, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, 80, 1075, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 88, 1132, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 80, 1121, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -40, 72, 1075, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -88, 88, 1145, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 80, 1132, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 72, 1120, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, 64, 1075, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 88, 1145, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 80, 1132, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 72, 1120, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, 64, 1075, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 56, 48, 1075, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, -24, 1025, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -24, 1025, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 0, 1037, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 0, 1037, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 24, 1050, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 48, 1075, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 72, 1087, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 32, 96, 1100, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, 96, 1075, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, 72, 1075, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, 48, 1062, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, 24, 1050, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 24, 1050, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 24, 80, 1250, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, 80, 1250, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 32, 80, 1200, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 32, 40, 1187, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_80184ED0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 18, 0, 0, { 0, 0 } },
    { 38, 22, 0, 0, { 5, 0 } },
    { 60, 14, 0, 0, { 1, 0 } },
    { 74, 2, 0, 0, { 4, 0 } },
    { 76, 2, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80184F10[23] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -80, 1125, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 64, 1125, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 16, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -32, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -80, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, -104, 1125, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -112, -104, 1125, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -80, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -32, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 16, 1125, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 64, 1125, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 64, 1125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, 16, 1125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -32, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -48, 16, 1125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -32, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -32, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -80, 1125, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -80, 1125, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, -104, 1125, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, -104, 1125, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -104, 1125, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, 64, 1125, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_801850DC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_observatory_801850F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80185104[14] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 64, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 64, 1000, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 80, 1000, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 16, 1000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 16, 1000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -32, 1000, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -32, 1000, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, -32, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -80, 1000, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -80, 1000, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -80, 1000, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, -120, 1000, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -112, -120, 1000, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -120, 1000, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_8018521C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_observatory_80185234[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_observatory_80185244[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80185254[21] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, -88, 1200, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 56, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1125, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 1125, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 72, -120, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -88, 1125, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -40, 1125, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 8, 1125, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 56, 1125, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 56, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 8, 1125, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -40, 1125, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -88, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 1125, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -120, 1200, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, -48, 1200, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, -40, 1200, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 1200, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, 56, 1200, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_801853F8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80185410[82] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, -80, 975, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -72, 987, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -40, -8, 1275, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -64, 72, 1000, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 912, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -120, 931, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 912, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 931, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 912, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, -24, 931, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 912, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -112, 24, 931, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, 72, 912, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, 72, 931, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -160, 88, 912, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -120, 937, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -72, 1000, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, -24, 1000, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, 24, 1000, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -72, 1150, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, -24, 1150, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1150, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -48, 24, 1275, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -48, -16, 1275, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -88, 950, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, -88, 950, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, -120, 1025, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -8, -120, 1025, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 40, -120, 1037, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -72, 1025, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, -24, 1025, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 48, 24, 1025, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, 24, 1037, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 56, 72, 1025, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -64, -120, 1025, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, -120, 848, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, -120, 1125, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -120, 862, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 144, 24, 831, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 64, 72, 1100, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -120, 825, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -72, 825, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, -24, 825, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 104, 24, 825, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 104, 72, 825, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -120, 950, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -72, 950, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 72, -24, 950, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, 24, 950, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, 72, 72, 950, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 24, 1100, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -24, 1100, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -72, 1100, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 1100, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -120, 1125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -72, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -24, 1125, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 48, 24, 1125, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 1125, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1325, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 40, 64, 1187, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 1187, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, -32, 1187, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 32, 16, 1187, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, 16, 1312, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -32, 1312, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -80, 1312, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 64, 1175, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 0, 56, 1435, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 0, 48, 1551, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 0, 40, 1633, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 32, 1712, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 64, 1303, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 56, 1419, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 48, 1599, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 40, 1633, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -48, 32, 1712, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 64, 1303, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 56, 1419, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -88, 48, 1599, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 40, 1633, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 1712, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_observatory_80185A78[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { 26, 10, 0, 0, { 3, 0 } },
    { 36, 23, 0, 0, { 2, 0 } },
    { 59, 8, 0, 0, { 4, 0 } },
    { 67, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_observatory_80185AB0[74] = {
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -160, 64, 1550, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 32, 1550, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -8, 1550, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -48, 1550, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -160, -88, 1550, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -128, -48, 1875, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -8, 1875, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -128, -72, 1875, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -48, 2150, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -104, -8, 2150, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -104, 16, 2150, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 48, 2150, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 32, 1875, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -112, 48, 1875, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -80, -48, 2325, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -56, 2150, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 80, 2375, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 32, 2375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 32, 2375, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -80, 80, 2375, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 32, 2375, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -16, 2375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -16, 2375, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -16, 2375, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -160, -24, 2375, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -112, -24, 2375, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, -24, 2375, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 1250, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 1250, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, 72, 1250, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 1250, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 40, 1250, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -80, -8, 1250, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 1250, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, -88, 1250, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 80, 812, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -128, 96, 775, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -152, 40, 812, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 48, 775, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 16, 800, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 24, 850, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 24, 862, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 24, 1062, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 72, 1300, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -152, 32, 1300, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, 72, 1200, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -136, 32, 1200, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -112, 32, 1062, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 72, 1062, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -72, 2554, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -48, 2350, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -120, -48, 2350, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -88, -48, 2350, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -56, -48, 2350, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -152, -64, 2425, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -120, -64, 2425, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, -64, 2425, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, -64, 2425, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -64, 2562, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -88, -72, 2500, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -72, 2575, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 96, 1096, { .fields = { 48, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, 48, 796, { .fields = { 32, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4000, { .fields = { 48, 24 } }, -160, 96, 789, { .fields = { 120, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 48 } }, -112, 48, 960, { .fields = { 8, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 24, 16 } }, -112, 96, 956, { .fields = { 8, 240 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, 48, 1106, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 40 } }, -152, 48, 1437, { .fields = { 104, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 40 } }, -136, 48, 1437, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 48 } }, -112, 48, 1300, { .fields = { 64, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_observatory_80186078[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 4, 0 } },
    { 27, 12, 0, 0, { 3, 0 } },
    { 39, 7, 0, 0, { 5, 0 } },
    { 46, 7, 0, 0, { 1, 0 } },
    { 53, 12, 0, 0, { 6, 0 } },
    { 65, 6, 0, 0, { 7, 0 } },
    { 71, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_observatory_801860C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_observatory_801860D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_observatory_801860E8[21] = {
    { { .empty = D_neo_ark_observatory_801822BC }, D_neo_ark_observatory_801822BC, NULL },
    { { .empty = D_neo_ark_observatory_801822CC }, D_neo_ark_observatory_801822CC, NULL },
    { { .elements = D_neo_ark_observatory_801822DC }, D_neo_ark_observatory_80182480, NULL },
    { { .elements = D_neo_ark_observatory_80182498 }, D_neo_ark_observatory_80182614, NULL },
    { { .elements = D_neo_ark_observatory_8018262C }, D_neo_ark_observatory_801827D0, NULL },
    { { .elements = D_neo_ark_observatory_801827E8 }, D_neo_ark_observatory_80182E50, NULL },
    { { .elements = D_neo_ark_observatory_80182E88 }, D_neo_ark_observatory_80183504, NULL },
    { { .elements = D_neo_ark_observatory_8018355C }, D_neo_ark_observatory_80183CB4, NULL },
    { { .elements = D_neo_ark_observatory_80183CDC }, D_neo_ark_observatory_80184240, NULL },
    { { .elements = D_neo_ark_observatory_80184288 }, D_neo_ark_observatory_80184878, NULL },
    { { .elements = D_neo_ark_observatory_801848B8 }, D_neo_ark_observatory_80184ED0, NULL },
    { { .elements = D_neo_ark_observatory_80184F10 }, D_neo_ark_observatory_801850DC, NULL },
    { { .empty = D_neo_ark_observatory_801850F4 }, D_neo_ark_observatory_801850F4, NULL },
    { { .elements = D_neo_ark_observatory_80185104 }, D_neo_ark_observatory_8018521C, NULL },
    { { .empty = D_neo_ark_observatory_80185234 }, D_neo_ark_observatory_80185234, NULL },
    { { .empty = D_neo_ark_observatory_80185244 }, D_neo_ark_observatory_80185244, NULL },
    { { .elements = D_neo_ark_observatory_80185254 }, D_neo_ark_observatory_801853F8, NULL },
    { { .elements = D_neo_ark_observatory_80185410 }, D_neo_ark_observatory_80185A78, NULL },
    { { .elements = D_neo_ark_observatory_80185AB0 }, D_neo_ark_observatory_80186078, NULL },
    { { .empty = D_neo_ark_observatory_801860C8 }, D_neo_ark_observatory_801860C8, NULL },
    { { .empty = D_neo_ark_observatory_801860D8 }, D_neo_ark_observatory_801860D8, NULL },
};

WorldCoordPointLight D_neo_ark_observatory_801861E4[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CF2, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9184, -2500, 0x2EC6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 9640 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 7413 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 5559 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6982, -2500, 3913 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 6335 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 9409 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 0x37E0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -802, -2500, 8240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CD5, -2337, 0x2BA7 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 906, 669 }, { 0, 0 } }, 100, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 0x2F1B } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 3999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 1447 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3103, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 1837 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 0x36FE } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
};

WorldCoordRoomLights D_neo_ark_observatory_80186844[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_observatory_801861E4), D_neo_ark_observatory_801861E4, 0, NULL },
};

WorldCoordPointLight D_neo_ark_observatory_8018685C[17] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CD5, -2337, 0x2BA7 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 906, 669 }, { 0, 0 } }, 100, 1600 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CF2, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9184, -2500, 0x2EC6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 9640 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 7413 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 5559 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6982, -2500, 3913 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 6335 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 9409 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 0x37E0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -802, -2500, 8240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 0x2F1B } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 3999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 1447 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3103, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 500, 3500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 1837 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 0x36FE } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3115, 2954, 2717 }, { 0, 0 } }, 2000, 7000 },
};

WorldCoordRoomLights D_neo_ark_observatory_80186EBC[1] = {
    { 0, NULL, ARRAY_SIZE(D_neo_ark_observatory_8018685C), D_neo_ark_observatory_8018685C, 0, NULL },
};

WorldCollisionTrigger D_neo_ark_observatory_80186ED4[18] = {
    { NULL, NULL, NULL, { 8851, -1408, 0x2EEF, 0 }, { { 506, -1872, -1859, 0 }, { -506, -1872, 1859, 0 }, { 506, 1872, -1859, 0 }, { -506, 1872, 1859, 0 } }, { 3953, 0, 1076, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8955, -1440, 0x2F0C, 0 }, { { -524, -1840, 1842, 0 }, { 524, -1840, -1841, 0 }, { -524, 1840, 1842, 0 }, { 524, 1840, -1841, 0 } }, { -3946, 0, -1124, 0 }, { 0, 0, 4096, 0 }, 2648, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8047, -1440, 9136, 0 }, { { -1596, -1840, 162, 0 }, { 1596, -1840, -161, 0 }, { -1596, 1840, 162, 0 }, { 1596, 1840, -161, 0 } }, { -415, 0, -4088, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8032, -1472, 8993, 0 }, { { 1668, -1840, -158, 0 }, { -1668, -1840, 159, 0 }, { 1668, 1840, -158, 0 }, { -1668, 1840, 159, 0 } }, { 386, 0, 4081, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7904, -1505, 6720, 0 }, { { 1732, -1840, 2, 0 }, { -1732, -1840, -1, 0 }, { 1732, 1840, 2, 0 }, { -1732, 1840, -1, 0 } }, { -4, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7952, -1441, 6861, 0 }, { { -1900, -1840, 21, 0 }, { 1900, -1840, -21, 0 }, { -1900, 1840, 21, 0 }, { 1900, 1840, -21, 0 } }, { -46, 0, -4106, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 8128, -1536, 3984, 0 }, { { -1900, -1840, 1573, 0 }, { 1900, -1840, -1573, 0 }, { -1900, 1840, 1573, 0 }, { 1900, 1840, -1573, 0 } }, { -2629, 0, -3176, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7970, -1536, 3937, 0 }, { { 1908, -1840, -1595, 0 }, { -1908, -1840, 1595, 0 }, { 1908, 1840, -1595, 0 }, { -1908, 1840, 1595, 0 } }, { 2629, 0, 3145, 0 }, { 0, 0, 4096, 0 }, 3093, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5121, -1600, 5120, 0 }, { { -12, -1840, -2459, 0 }, { 12, -1840, 2459, 0 }, { -12, 1840, -2459, 0 }, { 12, 1840, 2459, 0 } }, { 4108, 0, -21, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5297, -1568, 4705, 0 }, { { 4, -1840, 1829, 0 }, { -4, -1840, -1829, 0 }, { 4, 1840, 1829, 0 }, { -4, 1840, -1829, 0 } }, { -4106, 0, 8, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4289, -1568, 1072, 0 }, { { 1060, -1840, 1717, 0 }, { -1060, -1840, -1717, 0 }, { 1060, 1840, 1717, 0 }, { -1060, 1840, -1717, 0 } }, { -3492, 0, 2154, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 7, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4224, -1633, 1297, 0 }, { { -988, -1840, -1563, 0 }, { 989, -1840, 1563, 0 }, { -988, 1840, -1563, 0 }, { 989, 1840, 1563, 0 } }, { 3466, 0, -2194, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 10, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1776, -1600, 6129, 0 }, { { -2540, -1840, -843, 0 }, { 2541, -1840, 843, 0 }, { -2540, 1840, -843, 0 }, { 2541, 1840, 843, 0 } }, { 1291, 0, -3893, 0 }, { 0, 0, 4096, 0 }, 3248, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1921, -1569, 5986, 0 }, { { 2420, -1840, 853, 0 }, { -2420, -1840, -853, 0 }, { 2420, 1840, 853, 0 }, { -2420, 1840, -853, 0 } }, { -1363, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 3156, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1888, -1505, 0x2831, 0 }, { { 2356, -1840, -1051, 0 }, { -2356, -1840, 1051, 0 }, { 2356, 1840, -1051, 0 }, { -2356, 1840, 1051, 0 } }, { 1677, 0, 3761, 0 }, { 0, 0, 4096, 0 }, 3166, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1936, -1409, 0x28C1, 0 }, { { -2524, -1840, 1093, 0 }, { 2524, -1840, -1093, 0 }, { -2524, 1840, 1093, 0 }, { 2524, 1840, -1093, 0 } }, { -1629, 0, -3762, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4400, -1440, 0x3A80, 0 }, { { 836, -1840, -1787, 0 }, { -836, -1840, 1787, 0 }, { 836, 1840, -1787, 0 }, { -836, 1840, 1787, 0 } }, { 3717, 0, 1738, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 11, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4576, -1440, 0x3AE1, 0 }, { { -924, -1840, 1861, 0 }, { 924, -1840, -1861, 0 }, { -924, 1840, 1861, 0 }, { 924, 1840, -1861, 0 } }, { -3674, 0, -1825, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 9, 11, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_observatory_8018742C[14] = {
    { NULL, NULL, NULL, { 0x3540, -48, 0x2EC0, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 10, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7584, -256, 0x3780, 0 }, { { 0, 1232, -960, 0 }, { 0, -1232, -960, 0 }, { 0, 1232, 960, 0 }, { 0, -1232, 960, 0 } }, { 4105, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1557, WORLD_COLLISION_TRIGGER_ACTION_WARP, 10, 33, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 7552, -272, 1728, 0 }, { { 0, 1184, -1280, 0 }, { 0, -1184, -1280, 0 }, { 0, 1184, 1280, 0 }, { 0, -1184, 1280, 0 } }, { 4106, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1740, WORLD_COLLISION_TRIGGER_ACTION_WARP, 19, 50, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x2B60, -64, 0x2EE0, 0 }, { { 0, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { 0, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, 201, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 5504, -64, 4096, 0 }, { { -544, 0, -1504, 0 }, { 544, 0, -1504, 0 }, { -544, 0, 1504, 0 }, { 544, 0, 1504, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1598, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 512, -64, 7008, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2495, -64, 0x34AF, 0 }, { { -2561, 0, -1893, 0 }, { -1033, 0, -2838, 0 }, { 1034, 0, 2839, 0 }, { 2562, 0, 1894, 0 } }, { 0, 4108, 0, 0 }, { 2896, 0, -2896, 0 }, 3176, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2175, -64, 3008, 0 }, { { 920, 0, -3050, 0 }, { 2325, 0, -1929, 0 }, { -2324, 0, 1930, 0 }, { -920, 0, 3050, 0 } }, { 0, 4112, 0, 0 }, { 3702, 0, 1751, 0 }, 3176, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5600, -64, 7968, 0 }, { { -3488, 0, -2080, 0 }, { -384, 0, -2080, 0 }, { -3488, 0, 2400, 0 }, { -384, 0, 2400, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 4222, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5568, -64, 7968, 0 }, { { -2208, 0, -2912, 0 }, { 1056, 0, -2912, 0 }, { -2208, 0, 3584, 0 }, { 1056, 0, 3584, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 4190, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6864, -64, 0x37B0, 0 }, { { -208, 0, -624, 0 }, { 208, 0, -624, 0 }, { -208, 0, 624, 0 }, { 208, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 655, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 40, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6880, -64, 1760, 0 }, { { -224, 0, -608, 0 }, { 224, 0, -608, 0 }, { -224, 0, 608, 0 }, { 224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_FACING | 0x100, 40, 64, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 9824, -64, 0x2BC0, 0 }, { { -448, 0, -192, 0 }, { 1056, 0, -192, 0 }, { -1024, 0, 1728, 0 }, { 1056, 0, 1728, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 2023, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 3, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5728, -64, 1152, 0 }, { { -768, 0, -1504, 0 }, { 768, 0, -1504, 0 }, { -768, 0, 1504, 0 }, { 768, 0, 1504, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 1688, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 4, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_observatory_80187854[2] = {
    { 101, 502, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_450200_80137A60 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_observatory_8018786C[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B010, D_neo_ark_observatory_80187854 },
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
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_observatory_801878D4[4] = {
    { NULL, NULL, { 6336, -1664, 9200, 0 }, { { 32, 2688, 4016, 0 }, { -32, 2688, -4016, 0 }, { 32, -2688, 4016, 0 }, { -32, -2688, -4016, 0 } }, { 4114, 0, -33, 0 }, 4830, 1, 0 },
    { NULL, NULL, { 9248, -1440, 6720, 0 }, { { 32, 2688, 4016, 0 }, { -32, 2688, -4016, 0 }, { 32, -2688, 4016, 0 }, { -32, -2688, -4016, 0 } }, { 4114, 0, -33, 0 }, 4830, 1, 0 },
    { NULL, NULL, { 9312, -1376, 2656, 0 }, { { 4016, 2688, -32, 0 }, { -4016, 2688, 32, 0 }, { 4016, -2688, -32, 0 }, { -4016, -2688, 32, 0 } }, { -33, 0, -4115, 0 }, 4830, 1, 0 },
    { NULL, NULL, { 0x291F, -1504, 0x349F, 0 }, { { 4013, 2688, 166, 0 }, { -4012, 2688, -165, 0 }, { 4013, -2688, 166, 0 }, { -4012, -2688, -165, 0 } }, { 169, 0, -4111, 0 }, 4830, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCollisionFootstepSounds D_neo_ark_observatory_801879C4 = {
    0x10000015,
    0x10000017,
    0x10000019,
};

WorldCollisionFootstepSounds D_neo_ark_observatory_801879D0 = {
    0x10000041,
    0x10000043,
    0x10000055,
};

WorldCollisionFootstepSounds D_neo_ark_observatory_801879DC = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_neo_ark_observatory_801879E8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_observatory_801879F0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_observatory_801879C4 },
};

WorldCollisionSurfaceProperties D_neo_ark_observatory_801879F8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_observatory_801879D0 },
};

WorldCollisionSurfaceProperties D_neo_ark_observatory_80187A00[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_observatory_801879DC },
};

WorldCollisionSurfaceProperties* D_neo_ark_observatory_80187A08[8] = {
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879F0,
    D_neo_ark_observatory_801879F8,
    D_neo_ark_observatory_80187A00,
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879E8,
};

AreaApplyRec D_neo_ark_observatory_80187A28[2] = {
    { 5, 7, 2, 0 },
    { 255, 0, 0, 0 },
};

RoomDeparture gRoomDeparture;

s16 D_neo_ark_observatory_80187A3C;

static __inline__ void _neoArkObservatoryStageMarker(RoomDeparture* desc, RoomVariantResolver resolve);
static void            func_neo_ark_observatory_8017FCE0(Task* arg0);
static void            func_neo_ark_observatory_8017FD7C(Task* task);

#include "../../shared/planar_reflection.inc.c"

/// Runs this room's player reflection, including its attachment reflections.
///
/// Starts bodyless in state 0 with a live player model; spawnArg1.value selects
/// a floor reflection (0) or the room's mirror plane (1). State 1 updates the
/// cloned model and reflected view. The task owns its clone and mirror work
/// and is torn down with the player; dispatch requires state 0 or 1.
static void _neoArkObservatoryPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

#include "../../shared/room_variants_shelter.inc.c"
#undef ROOM_VARIANT_RESOLVE_SHELTER

#include "../../shared/room_event_departure_task.inc.c"

/// Copies the area, warp and room of `desc` into a resolver record, lets
/// `resolve` rewrite the record in place, and copies the result back.
static __inline__ void _neoArkObservatoryStageMarker(RoomDeparture* desc, RoomVariantResolver resolve)
{
    RoomEventMsg rec;

    rec.areaId    = desc->area;
    rec.warp      = desc->warp;
    rec.room      = desc->room;
    rec.queryOnly = ROOM_EVENT_EXECUTE;
    resolve(&rec, &rec);
    desc->area = rec.areaId;
    desc->warp = rec.warp;
    desc->room = rec.room;
}

s32 func_neo_ark_observatory_8017F6F8(Task* arg0, s32 arg1, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    RoomDeparture       desc;
    RoomVariantResolver resolve;
    s32                 temp;

    if (request->actionId == 0xA) {
        if (gameFlagGetNibble(GAME_FLAG_0D1) == 2) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 8);
        }
        if (gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) == 0) {
            temp = gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED);
            if (temp == 1) {
                RoomVariantResolver resolve;

                gameFlagSetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN, 1);
                desc.stage    = GAME_STAGE_MINE_SHELTER;
                desc.area     = GAME_AREA_SHELTER_B1_CONTROL_ROOM;
                desc.warp     = 3;
                desc.room     = temp;
                desc.sndEvent = 0x55070005;
                desc.facing   = 0x400;
                resolve       = _roomVariantResolveShelter;
                Gp_MsgPlayerWeapon(0);
                _neoArkObservatoryStageMarker(&desc, resolve);
                gRoomDeparture = desc;
                taskSpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
                return 0;
            }
        }
        desc.stage    = GAME_STAGE_MINE_SHELTER;
        desc.area     = request->argument;
        desc.room     = 1;
        desc.warp     = 4;
        desc.sndEvent = 0x55070005;
        desc.facing   = 0x400;
        resolve       = _roomVariantResolveShelter;
        Gp_MsgPlayerWeapon(0);
        _neoArkObservatoryStageMarker(&desc, resolve);
        gRoomDeparture = desc;
        taskSpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
    }
    if (request->actionId == 1 && gameFlagGetNibble(GAME_FLAG_0D7) == 0) {
        gameFlagSetNibble(GAME_FLAG_0D7, 1);
        if (gameFlagGetNibble(GAME_FLAG_083) != 0) {
            func_800E3FAC(0xA2, 0x2C);
            func_800E8634(D_actor_450200_8013C72C, 0, D_actor_450200_8013CAEC);
        } else {
            func_800E3FAC(0xA2, 0x2D);
            gameFlagSetNibble(GAME_FLAG_0D1, 3);
            func_800E8634(D_actor_450200_80137EE4, 0, D_actor_450200_80138694);
        }
    }
    if (request->actionId == 2) {
        if (gameFlagGetNibble(GAME_FLAG_0E1) == 0) {
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 6);
            gameFlagSetNibble(GAME_FLAG_0E1, 1);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x15;
            func_800E8634(D_actor_450200_8013FC58, 0, D_actor_450200_80140078);
        }
    }
    if (request->actionId == 3 && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL && gGameSession->location.loc.view == 2) {
        func_actor_450200_80132220();
    }
    if (request->actionId == 4 && gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) != 0 && gameFlagGetNibble(GAME_FLAG_OBSERVATORY_EVENT_SEEN) == 0) {
        gameFlagSetNibble(GAME_FLAG_OBSERVATORY_EVENT_SEEN, 1);
        func_800E8634(D_neo_ark_observatory_801811E0.data.sceneScript, 0, D_neo_ark_observatory_801812C0);
    }
    return 0;
}

/// Rebuilds the room's mesh under the model of the slot-0xA task, or of the
/// slot-3 task when there is none. The mesh is offset by `vy` = 0 while a
/// slot-0xA task exists and flag nibble 0xD7 is set, and by 10000 otherwise.
/// `arg0` is unused.
void func_neo_ark_observatory_8017FA98(s32 arg0)
{
    Task* task;
    Task* slotA;

    task  = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    slotA = task;
    if (task == NULL) {
        task = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    }
    if (slotA != NULL && gameFlagGetNibble(GAME_FLAG_0D7) != 0) {
        D_neo_ark_observatory_80181368.vy = 0;
    } else {
        D_neo_ark_observatory_80181368.vy = 0x2710;
    }
    followCollisionRebuild(task->extra.tmd->coords, &D_neo_ark_observatory_80181368);
}

void func_neo_ark_observatory_8017FB1C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            capSetTexturePage(0x300, 0);
            Gp_SpawnIfCapIdle(task->spawnArg1.value, 0);
            task->state++;
            break;
        case 1:
            if (capIsBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

/// Rejects key-item use in this room with result 0 and no side effects.
///
/// All four callback arguments are unused. The key-item menu interprets zero
/// as an unavailable use and leaves the item in inventory.
static s32 _neoArkObservatoryRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg)
{
    return 0;
}

/// Room event-script handler: mirrors the incoming message onto the outgoing
/// one and lets `mapNeoArkResolveRoomVariant` act on both. Once the observatory has been
/// reached from both routes (nibbles 0xD1 == 3 and 0x4C == 9) and the script
/// raises one of the two arrival ids with no sub-state pending, nibble 0x4C is
/// cleared and the room's area records are applied.
s32 func_neo_ark_observatory_8017FBE8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    mapNeoArkResolveRoomVariant(in, out);
    if ((gameFlagGetNibble(GAME_FLAG_0D1) == 3) && (gameFlagGetNibble(GAME_FLAG_COMPANION_1_SCHEDULE) == 9) &&
        ((in->areaId == GAME_AREA_NEO_ARK_NORTH_PROMENADE) || (in->areaId == GAME_AREA_NEO_ARK_SOUTH_PROMENADE)) && (in->queryOnly == ROOM_EVENT_EXECUTE)) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 0);
        Gp_ApplyAreaRecs(D_neo_ark_observatory_80187A28);
    }
    return 1;
}

s32 func_neo_ark_observatory_8017FCA0(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        Gp_MsgPlayerWeapon(0);
        taskSpawnFromTable(&D_neo_ark_observatory_801811AC, 0, 1, 0);
    }
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// slot 7, then advances state.
static void func_neo_ark_observatory_8017FCE0(Task* arg0)
{
    arg0->msgTable = D_neo_ark_observatory_801811B8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) && (gGameSession->location.loc.variant == 1)) {
        func_actor_450200_801322F8();
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (gameFlagGetNibble(GAME_FLAG_0E1) != 0) {
        neoArkObservatorySetLightBeamIntensity(0xA0);
    }
    arg0->state = arg0->state + 1;
}

/// Per-frame tick of the room entry task: sends the ally message 0x3F3 with 2
/// while no event runs and the save's view is not 2, and otherwise with 2 in
/// view 3 and 1 in any other view.
static void func_neo_ark_observatory_8017FD7C(Task* task)
{
    s32 var_a0;

    if (gGameSession->eventState == 0) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view != 2) {
            Gp_MsgAlly3F3(2);
            return;
        }
    }
    var_a0 = 1;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view == 3) {
        var_a0 = 2;
    }
    Gp_MsgAlly3F3(var_a0);
}

/// State handlers of the room entry task `func_neo_ark_observatory_8017FDDC`,
/// indexed by `Task::state`: the set-up tick, the arrival-line tick, and
/// `taskKill`.
static const TaskFuncTable3 D_neo_ark_observatory_8017D698 = {
    {
        func_neo_ark_observatory_8017FCE0,
        func_neo_ark_observatory_8017FD7C,
        taskKill,
    },
};

/// Room entry task: dispatches through a stack copy of its state table.
void func_neo_ark_observatory_8017FDDC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_observatory_8017D698;
    sp.funcs[task->state](task);
}

#include "../../shared/follow_collision_rebuild.inc.c"

void neoArkObservatoryGlowTask(Task* effectTask)
{
    enum {
        NEO_ARK_OBSERVATORY_GLOW_STATE_INIT   = 0,
        NEO_ARK_OBSERVATORY_GLOW_STATE_DRAW   = 1,
        NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT  = 0x444, // Packed RGB nibbles
        NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM  = 0x333,
        NEO_ARK_OBSERVATORY_GLOW_DISC_DIM     = 0x222,
        NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS  = 0x280, // Projected radius numerator
        NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS   = 0x200,
        NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS = 0x400, // World-coordinate units
        NEO_ARK_OBSERVATORY_BEAM_LARGE_RADIUS = 0x600,
    };
    u8 mappedView;

    if (effectTask->state == NEO_ARK_OBSERVATORY_GLOW_STATE_INIT) {
        D_neo_ark_observatory_80187A3C = 0;
        effectTask->state              = NEO_ARK_OBSERVATORY_GLOW_STATE_DRAW;
    }

    // Select the lights visible in the mapped camera, including alternate room views.
    // Retained offsets span light-position storage split into separate array symbols.
    mappedView = viewGetMappedIndex();
    switch (mappedView) {
        case 2:
            glowDrawDisc(&D_neo_ark_observatory_801814E4[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        case 3: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_801814F4;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 4:
        case 16: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_801814FC;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[3], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            break;
        }
        case 5:
        case 17: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_8018150C;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 6:
        case 18:
            glowDrawDisc(&D_neo_ark_observatory_8018151C[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        case 7: {
            const SVECTOR* beamCenters;
            beamCenters = D_neo_ark_observatory_80181434;
            _neoArkObservatoryDrawLightBeam(&beamCenters[0], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 8);
            _neoArkObservatoryDrawLightBeam(&beamCenters[2], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 0xC);
            _neoArkObservatoryDrawLightBeam(&beamCenters[4], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 8);
        }
            // View 7 also draws the glow discs used by mapped view 19.
            /* fallthrough */
        case 19: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_8018151C;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[6], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[7], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[8], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[9], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 8:
        case 20: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_80181564;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-32], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 8);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-30], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 8);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-28], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 9: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_80181574;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[3], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[-6], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[-8], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-28], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 8);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-26], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 0xC);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-24], NEO_ARK_OBSERVATORY_BEAM_SMALL_RADIUS, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 10: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_80181524;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[5], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[6], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[7], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-12], NEO_ARK_OBSERVATORY_BEAM_LARGE_RADIUS, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 11: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_8018157C;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_DIM);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_MEDIUM);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[-7], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[-8], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            _neoArkObservatoryDrawLightBeam(&glowPoints[-21], NEO_ARK_OBSERVATORY_BEAM_LARGE_RADIUS, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 12:
        case 14: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_801814E4;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[2], NEO_ARK_OBSERVATORY_GLOW_HIGH_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
        case 21: {
            const SVECTOR* glowPoints;
            glowPoints = D_neo_ark_observatory_80181564;
            glowDrawDisc(&glowPoints[0], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            glowDrawDisc(&glowPoints[1], NEO_ARK_OBSERVATORY_GLOW_LOW_RADIUS, NEO_ARK_OBSERVATORY_GLOW_DISC_BRIGHT);
            break;
        }
    }
}

/// Draws an additive grey light beam between two horizontal world-space rings.
///
/// Borrows two centres for this call: ring 0 has half the signed low-halfword
/// outerRadius, ring 1 has the full radius, both in world-coordinate units.
/// segmentCount must be 1..4096; room callers use 8, 12 or 16. Angles use 4096
/// units per turn, advancing one unit per animation frame. Integer division
/// truncates the angle step when the count does not divide a turn exactly.
///
/// A four-frame sine pulse adds -4..4 to baseIntensity, narrowed to s16.
/// Negative results draw nothing; nonnegative intensities narrow to RGB bytes.
/// The first ring fades from full to half intensity per segment, and the
/// second ring is black. Each accepted projection queues one quad and an
/// additive blend command, at the last corner's camera depth / 4 plus one.
/// Requires the composed view matrix, scratch stack and current frame arena.
static void _neoArkObservatoryDrawLightBeam(const SVECTOR ringCenters[2], s32 outerRadius, s16 baseIntensity, s16 segmentCount)
{
    enum {
        NEO_ARK_OBSERVATORY_BEAM_PULSE_ANGLE_SHIFT = 10, // Quarter-turn per animation frame
        NEO_ARK_OBSERVATORY_BEAM_PULSE_LEVEL_SHIFT = 10, // Q12 sine to -4..4 intensity
    };
    EffectQuadScratch*  quadScratch;
    POLY_G4*            quad;
    const SVECTOR*      outerCenter;
    const DisplayState* display;
    s16                 startAngle;
    s16                 angleStep;
    s32                 angle;
    s32                 nextAngle;
    s16                 innerRadius;
    s16                 intensity;

    angleStep   = GLOW_FULL_TURN / segmentCount;
    outerCenter = ringCenters + 1;
    innerRadius = (s16)outerRadius >> 1;
    startAngle  = gDisplayState.animFrame & (GLOW_FULL_TURN - 1);
    intensity   = baseIntensity + (rsin(gDisplayState.animFrame << NEO_ARK_OBSERVATORY_BEAM_PULSE_ANGLE_SHIFT) >> NEO_ARK_OBSERVATORY_BEAM_PULSE_LEVEL_SHIFT);
    if (intensity >= 0) {
        SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
        quadScratch = SCRATCH_STACK_CURSOR(EffectQuadScratch);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        // Build adjoining XZ ring edges, then project each strip segment through the view.
        for (angle = startAngle; angle < startAngle + angleStep * segmentCount; angle = nextAngle) {
            quadScratch->vertices[0].vx = ringCenters->vx + ((rsin(angle) * innerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[0].vy = ringCenters->vy;
            quadScratch->vertices[0].vz = ringCenters->vz + ((rcos(angle) * innerRadius) >> GLOW_TRIG_SHIFT);
            nextAngle                   = angle + angleStep;
            quadScratch->vertices[1].vx = ringCenters->vx + ((rsin(nextAngle) * innerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[1].vy = ringCenters->vy;
            quadScratch->vertices[1].vz = ringCenters->vz + ((rcos(nextAngle) * innerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[2].vx = outerCenter->vx + ((rsin(angle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[2].vy = outerCenter->vy;
            quadScratch->vertices[2].vz = outerCenter->vz + ((rcos(angle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[3].vx = outerCenter->vx + ((rsin(nextAngle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            quadScratch->vertices[3].vy = outerCenter->vy;
            quadScratch->vertices[3].vz = outerCenter->vz + ((rcos(nextAngle) * (s16)outerRadius) >> GLOW_TRIG_SHIFT);
            _neoArkObservatoryProjectBeamSegment(quadScratch);
            if (quadScratch->projectionFlags >= 0) {
                // Use the final projected corner for ordering, with a one-entry bias.
                gte_stszotz(&quadScratch->depth);
                quadScratch->depth++;
                display        = &gDisplayState;
                quad           = gGpuPrimCursor;
                gGpuPrimCursor = quad + 1;
                setPolyG4(quad);
                setRGB0(quad, intensity, intensity, intensity);
                setRGB1(quad, intensity >> 1, intensity >> 1, intensity >> 1);
                setRGB2(quad, 0, 0, 0);
                setRGB3(quad, 0, 0, 0);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(quadScratch->depth << display->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        quad);
                quad->x0 = quadScratch->screenCorners[0].vx;
                quad->y0 = quadScratch->screenCorners[0].vy;
                quad->x1 = quadScratch->screenCorners[1].vx;
                quad->y1 = quadScratch->screenCorners[1].vy;
                quad->x2 = quadScratch->screenCorners[2].vx;
                quad->y2 = quadScratch->screenCorners[2].vy;
                quad->x3 = quadScratch->screenCorners[3].vx;
                quad->y3 = quadScratch->screenCorners[3].vy;
                gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, quadScratch->depth);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
    }
}

#include "../../shared/glow_draw_disc.inc.c"

void neoArkObservatorySetLightBeamIntensity(s32 intensity)
{
    D_neo_ark_observatory_80187A3C = intensity;
}
