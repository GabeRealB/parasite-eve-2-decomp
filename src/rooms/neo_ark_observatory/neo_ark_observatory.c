#include "common.h"
#include "rooms/neo_ark_observatory.h"
#include "mapui/map_neo_ark.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/gtemac.h>
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/area_transitions.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/model_objects.h"
#include "gameplay/room_effects.h"
#include "gameplay/loading.h"

#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/view.h"
#include "rooms/stage_tables.h"

#include "actors/task_tables.h"
#include "mapui/stage_tables.h"

// The animation copy spans the bank and its following records.
// Keep the typed fields and the complete copied word range together.
typedef union {
    struct {
        GpAnimSet* sets[1];
        GpCopyArg copy;
        GpAnimArg arguments[1];
        GpEvsCmd commands[8];
    } data;
    s32 words[56];
} NeoArkObservatoryAnimStorage11E0;
STATIC_ASSERT_SIZEOF(NeoArkObservatoryAnimStorage11E0, 224);

extern NeoArkObservatoryAnimStorage11E0 D_neo_ark_observatory_801811E0;

/// Record the destination resolver `func_neo_ark_observatory_8017F44C` reads:
/// `field_0` is the destination area and `field_5` must be 0 for it to act.
typedef struct MapMarkerRec {
    u16  field_0;
    byte pad_2[3];
    u8   field_5;
} MapMarkerRec;

/// Record the destination resolver writes: `field_3` is the destination room.
/// The caller passes the same buffer as the `MapMarkerRec`, so it overwrites
/// the room byte the caller staged after the area and warp.
typedef struct MapMarkerOut {
    byte pad_0[3];
    s8   field_3;
} MapMarkerOut;

/// A destination resolver: reads the area from its first record and writes the
/// room through the second.
typedef s32 (*_MapMarkerResolve)(MapMarkerRec*, MapMarkerOut*);

extern TaskDesc D_80137EE4;
extern TaskDesc D_80138694;
extern TaskDesc D_8013C72C;
extern TaskDesc D_8013CAEC;
extern TaskDesc D_8013FC58;
extern TaskDesc D_80140078;

extern void func_80132220(void);
extern void func_801322F8(void);

/// Index of the mirrored player's coordinate part each held-object reflection
/// is parented to, by `Task::spawnArg1`.
extern u8 D_neo_ark_observatory_80180DB8[];

/// The mirror's task descriptors: entry 0 runs the mirror task itself, entry 1
/// the held-object reflections it spawns.
extern TaskDesc D_neo_ark_observatory_80180DBC[];

/// The departure task's descriptor.
extern TaskDesc D_neo_ark_observatory_80180DD4;

extern GpEvsCmd D_neo_ark_observatory_801812C0[];

/// Descriptor of the cap-file task `func_neo_ark_observatory_8017FB1C`.
extern TaskDesc D_neo_ark_observatory_801811AC;

/// Messages the room task answers, terminated by id 0x7FFFFFFF.
extern GpMsgEntry D_neo_ark_observatory_801811B8[];

/// Offset `func_neo_ark_observatory_8017FA98` hands the mesh rebuild; only its
/// `vy` is ever set.
extern SVECTOR D_neo_ark_observatory_80181368;

extern GpGridParams D_neo_ark_observatory_80181410;
extern GpGridParams D_neo_ark_observatory_80181FA4;
extern SVECTOR      D_neo_ark_observatory_80181434[];
extern SVECTOR      D_neo_ark_observatory_801814E4[];
extern SVECTOR      D_neo_ark_observatory_801814F4[];
extern SVECTOR      D_neo_ark_observatory_801814FC[];
extern SVECTOR      D_neo_ark_observatory_8018150C[];
extern SVECTOR      D_neo_ark_observatory_8018151C[];
extern SVECTOR      D_neo_ark_observatory_80181524[];
extern SVECTOR      D_neo_ark_observatory_80181564[];
extern SVECTOR      D_neo_ark_observatory_80181574[];
extern SVECTOR      D_neo_ark_observatory_8018157C[];

extern GpAreaApplyRec D_neo_ark_observatory_80187A28[];
extern RoomDeparture  D_neo_ark_observatory_80187A30;
extern s16            D_neo_ark_observatory_80187A3C;

static void func_neo_ark_observatory_8017D8A8(Task* task);
static s32  func_neo_ark_observatory_8017F44C(MapMarkerRec* arg0, MapMarkerOut* arg1);
static void func_neo_ark_observatory_8017FE34(GpCoord* coord, SVECTOR* offset);
static void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s16 arg3);
static void func_neo_ark_observatory_80180A0C(SVECTOR* arg0, s32 arg1, s32 arg2);

extern GpGridParams D_neo_ark_observatory_80181FA4;
extern GpObj3A D_neo_ark_observatory_801878D4[4];
extern GpObj4C D_neo_ark_observatory_80186ED4[18];
extern GpObj4C D_neo_ark_observatory_8018742C[14];
extern GpRoomCoordSet D_neo_ark_observatory_80186844[1];
extern GpRoomCoordSet D_neo_ark_observatory_80186EBC[1];

extern NeoArkObservatoryAnimStorage11E0 D_neo_ark_observatory_801811E0;
s32 func_neo_ark_observatory_8017F6F8(Task *, s32, GpMsg13EF *, s32);
s32 func_neo_ark_observatory_8017FBE0(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_observatory_8017FBE8(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_neo_ark_observatory_8017FCA0(Task *, s32, s32, GpMessageArg);
void func_neo_ark_observatory_8017F22C(Task *);
void func_neo_ark_observatory_8017F3FC(Task *);
void func_neo_ark_observatory_8017F588(Task *);
void func_neo_ark_observatory_8017FB1C(Task *);

u8 D_neo_ark_observatory_80180DB8[4] = {
    12,
    8,
    12,
    8,
};

TaskDesc D_neo_ark_observatory_80180DBC[2] = {
    { 0, 112, func_neo_ark_observatory_8017F3FC, { .model = NULL } },
    { 0, 112, func_neo_ark_observatory_8017F22C, { .model = NULL } },
};

TaskDesc D_neo_ark_observatory_80180DD4 = { 0, 32, func_neo_ark_observatory_8017F588, { .model = NULL } };

// The player indexes this pose bank in words, then reads a full pose.
typedef union {
    GpPackedPose poses[6];
    GpPackedSvec words[18];
} NeoArkObservatoryPoseBank3820;

NeoArkObservatoryPoseBank3820 D_neo_ark_observatory_80180DE0 = { .poses = {
#include "assets/neo_ark_observatory_animation_03BC4_bank1.inc"
} };

GpPackedSvec D_neo_ark_observatory_80180E28[64] = {
#include "assets/neo_ark_observatory_animation_03BC4_bank4.inc"
};

GpAnimRec D_neo_ark_observatory_80180F28[141] = {
#include "assets/neo_ark_observatory_animation_03BC4_records.inc"
};

u16 D_neo_ark_observatory_8018115C[20] = {
#include "assets/neo_ark_observatory_animation_03BC4_indices.inc"
};

GpAnimSet D_neo_ark_observatory_80181184 = {
    D_neo_ark_observatory_80180F28, D_neo_ark_observatory_8018115C,
    { NULL, D_neo_ark_observatory_80180DE0.words, NULL, NULL, D_neo_ark_observatory_80180E28, NULL, NULL, NULL },
};

TaskDesc D_neo_ark_observatory_801811AC = { 0, 192, func_neo_ark_observatory_8017FB1C, { .model = NULL } };

GpMsgEntry D_neo_ark_observatory_801811B8[5] = {
    { 5102, func_neo_ark_observatory_8017FBE8 },
    { 5105, func_neo_ark_observatory_8017FBE0 },
    { 5103, func_neo_ark_observatory_8017F6F8 },
    { 5104, func_neo_ark_observatory_8017FCA0 },
    { 0x7FFFFFFF, NULL },
};

NeoArkObservatoryAnimStorage11E0 D_neo_ark_observatory_801811E0 = { .data = { { &D_neo_ark_observatory_80181184 }, { { .words = D_neo_ark_observatory_801811E0.words }, 32 }, { { { .index = 1 }, 47, 1, 15, 1 } }, { { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } }, { 1, { .value = 3 }, { .value = 0 }, { .value = 1015 }, { .storage = &D_neo_ark_observatory_801811E0.data.copy }, { .value = 0 } }, { 10, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_neo_ark_observatory_801811E0.data.arguments }, { .value = 0 } }, { 15, { .value = 0x55070009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } }, { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } }, { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } }, { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } }, { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } } } } };

GpEvsCmd D_neo_ark_observatory_801812C0[7] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_neo_ark_observatory_80181368 = { 0, 0, -200, 0 };

SVECTOR D_neo_ark_observatory_80181370[4] = {
    { -4073, 0, 429, 0 },
    { 0, 0, -4096, 0 },
    { 4053, 0, 590, 0 },
    { 0, 0, 4096, 0 },
};

SVECTOR D_neo_ark_observatory_80181390[8] = {
    { -268, 500, 747, 0 },
    { -268, -932, 747, 0 },
    { -399, -932, -500, 0 },
    { -399, 500, -500, 0 },
    { 396, -932, -500, 0 },
    { 396, 500, -500, 0 },
    { 214, -932, 747, 0 },
    { 214, 500, 747, 0 },
};

GpGridFace D_neo_ark_observatory_801813D0[4] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 2, 4, 3, 5 }, 1, 0 },
    { { 4, 6, 5, 7 }, 2, 0 },
    { { 6, 1, 7, 0 }, 3, 0 },
};

s16 D_neo_ark_observatory_80181400[5] = {
    0,
    1,
    2,
    3,
    -1,
};

s16 * D_neo_ark_observatory_8018140C[1] = {
    D_neo_ark_observatory_80181400,
};

GpGridParams D_neo_ark_observatory_80181410 = { NULL, D_neo_ark_observatory_80181370, D_neo_ark_observatory_80181390, D_neo_ark_observatory_801813D0, D_neo_ark_observatory_8018140C, 399, 500, 1, 1, 4000, 4 };

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

GpRoomObjRec D_neo_ark_observatory_80181594[2] = {
    { &D_neo_ark_observatory_80181FA4, D_neo_ark_observatory_80186ED4, D_neo_ark_observatory_8018742C, D_neo_ark_observatory_801878D4 },
    { &D_neo_ark_observatory_80181FA4, D_neo_ark_observatory_80186ED4, D_neo_ark_observatory_8018742C, D_neo_ark_observatory_801878D4 },
};

GpRoomCoordRec D_neo_ark_observatory_801815B4[2] = {
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

u8 * D_neo_ark_observatory_801815DC[2] = {
    D_neo_ark_observatory_801815C4,
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_observatory_801815E4[2] = {
    { { .bytes = { 21, 0 } } },
    { { .bytes = { 21, 0 } } },
};

GpWarpRec D_neo_ark_observatory_801815E8[3] = {
    { { .words = { 3072, 0x34BC, 0, 0x2EE0 } }, { 0, 0, 0, 0 }, { .words = { 2560, 0x2D82, 0, 0x316A } }, { 0, 0, 0, 0 }, 0x55070006, 0x55070005, 0, 2, 0, 452 },
    { { .words = { 3072, 6700, 0, 0x37DC } }, { 0, 0, 0, 0 }, { .words = { 3072, 6700, 0, 0x37DC } }, { 0, 0, 0, 0 }, 0, 0, 0, 11, 0, 0 },
    { { .words = { 3072, 6490, 0, 1700 } }, { 0, 0, 0, 0 }, { .words = { 3072, 6490, 0, 1700 } }, { 0, 0, 0, 0 }, 0, 0, 0, 10, 0, 0 },
};

SVECTOR D_neo_ark_observatory_80181690[26] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, -4096, 0, 0 },
    { -4096, 0, 0, 0 },
    { 2938, 0, -2854, 0 },
    { -3422, 0, -2251, 0 },
    { 0, 0, -4096, 0 },
    { 0, 0, 4096, 0 },
    { 3798, 0, -1535, 0 },
    { 4096, 0, 0, 0 },
    { 3798, 0, 1535, 0 },
    { 2985, 0, 2805, 0 },
    { 4073, 0, -430, 0 },
    { -3506, 0, 2118, 0 },
    { 4073, 0, 430, 0 },
    { -3622, 0, -1913, 0 },
    { -2896, 0, -2896, 0 },
    { -2896, 0, 2896, 0 },
    { -1060, 0, 3956, 0 },
    { -3956, 0, 1060, 0 },
    { -3956, 0, -1060, 0 },
    { -1060, 0, -3956, 0 },
    { -1209, 0, 3913, 0 },
    { -1295, 0, 3886, 0 },
};

SVECTOR D_neo_ark_observatory_80181760[106] = {
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
    { 6331, 0, 2800, 0 },
    { 4700, 0, 0, 0 },
    { 4700, 0, 0x3E80, 0 },
    { 6331, 0, 0x33CF, 0 },
    { 4992, 1, 2497, 0 },
    { 4992, 1, 3204, 0 },
    { 4992, -3006, 3204, 0 },
    { 4992, -3006, 2497, 0 },
    { 4992, -3006, 0x34BF, 0 },
    { 4992, -3006, 0x327C, 0 },
    { 4992, 1, 0x327C, 0 },
    { 4992, 1, 0x34BF, 0 },
    { 1665, 1, 0x34F6, 0 },
    { 1665, -3006, 0x34F6, 0 },
    { 3951, -3006, 0x3E26, 0 },
    { 3951, 1, 0x3E26, 0 },
    { 5392, -3006, 0x3E26, 0 },
    { 5992, -3006, 0x3A96, 0 },
    { 5992, 1, 0x3A96, 0 },
    { 5392, 1, 0x3E26, 0 },
    { 7002, -3006, 0x3A96, 0 },
    { 7002, -3006, 0x34BF, 0 },
    { 7002, 1, 0x34BF, 0 },
    { 7002, 1, 0x3A96, 0 },
    { 208, 1, 9952, 0 },
    { 208, -3006, 9952, 0 },
    { 5980, -3006, 0x327C, 0 },
    { 5980, 1, 0x327C, 0 },
    { 5980, -3006, 0x293F, 0 },
    { 5980, 1, 0x293F, 0 },
    { 0x36B4, -3006, 0x2B57, 0 },
    { 8906, -3006, 0x2B57, 0 },
    { 8906, 1, 0x2B57, 0 },
    { 0x36B4, 1, 0x2B57, 0 },
    { 7112, 1, 0x320D, 0 },
    { 7112, 1, 4698, 0 },
    { 7112, -3006, 4698, 0 },
    { 7112, -3006, 0x320D, 0 },
    { 1665, -3006, 2442, 0 },
    { 208, -3006, 6048, 0 },
    { 208, 1, 6048, 0 },
    { 1665, 1, 2442, 0 },
    { 5980, 1, 4698, 0 },
    { 5980, -3006, 4698, 0 },
    { 8906, -3006, 3601, 0 },
    { 8906, 1, 3601, 0 },
    { 5392, -3006, 9, 0 },
    { 3951, -3006, 9, 0 },
    { 3951, 1, 9, 0 },
    { 5392, 1, 9, 0 },
    { 0x36B4, 1, 0x320D, 0 },
    { 0x36B4, -3006, 0x320D, 0 },
    { 2, 1, 8000, 0 },
    { 2, -3006, 8000, 0 },
    { 5992, 1, 1002, 0 },
    { 5992, -3006, 1002, 0 },
    { 7002, 1, 2497, 0 },
    { 7002, -3006, 2497, 0 },
    { 6002, 1, 3204, 0 },
    { 6002, -3006, 3204, 0 },
    { 7002, -3006, 1002, 0 },
    { 7002, 1, 1002, 0 },
    { 5602, -3006, 5414, 0 },
    { 5602, 0, 5414, 0 },
    { 3374, 0, 6700, 0 },
    { 3374, -3006, 6700, 0 },
    { 4316, -3006, 5758, 0 },
    { 4316, 0, 5758, 0 },
    { 4316, 0, 0x27E6, 0 },
    { 4316, -3006, 0x27E6, 0 },
    { 3374, -3006, 9273, 0 },
    { 3374, 0, 9273, 0 },
    { 5602, 0, 0x293F, 0 },
    { 5602, -3006, 0x293F, 0 },
    { 3030, -3006, 7986, 0 },
    { 3030, 0, 7986, 0 },
    { 0, 0, 0, 0 },
    { 0, 0, 0x3E80, 0 },
    { 0x36B0, 0, 0x3E80, 0 },
    { 0x36B0, 0, 0x33CF, 0 },
    { 0x36B0, 0, 2800, 0 },
    { 0x36B0, 0, 0, 0 },
    { 7286, -3006, 3601, 0 },
    { 7286, 1, 3601, 0 },
    { 7400, -3000, 3600, 0 },
    { 7400, 0, 3600, 0 },
    { 9200, 0, 3600, 0 },
    { 9200, -3000, 3600, 0 },
    { 6200, -3000, 3200, 0 },
    { 6200, 0, 3200, 0 },
};

GpGridFace D_neo_ark_observatory_80181AB0[48] = {
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 0, 0, 0, 0 }, 0, 0 },
    { { 17, 18, 16, 19 }, 4, 2 },
    { { 21, 22, 20, 23 }, 5, 0 },
    { { 25, 26, 24, 27 }, 5, 0 },
    { { 29, 30, 28, 31 }, 6, 0 },
    { { 33, 34, 32, 35 }, 7, 0 },
    { { 31, 30, 35, 32 }, 8, 0 },
    { { 37, 38, 36, 39 }, 5, 0 },
    { { 24, 27, 37, 38 }, 9, 0 },
    { { 34, 33, 39, 36 }, 8, 0 },
    { { 40, 41, 28, 29 }, 10, 0 },
    { { 42, 43, 25, 26 }, 8, 0 },
    { { 45, 43, 44, 42 }, 5, 0 },
    { { 47, 48, 46, 49 }, 9, 0 },
    { { 51, 52, 50, 53 }, 11, 0 },
    { { 55, 56, 54, 57 }, 12, 0 },
    { { 58, 59, 51, 52 }, 8, 0 },
    { { 60, 61, 47, 48 }, 5, 0 },
    { { 63, 64, 62, 65 }, 9, 0 },
    { { 50, 53, 66, 67 }, 8, 0 },
    { { 46, 49, 67, 66 }, 5, 0 },
    { { 54, 57, 63, 64 }, 13, 0 },
    { { 68, 69, 40, 41 }, 14, 0 },
    { { 70, 71, 65, 62 }, 15, 0 },
    { { 20, 23, 72, 73 }, 8, 0 },
    { { 55, 69, 56, 68 }, 16, 0 },
    { { 75, 22, 74, 21 }, 9, 0 },
    { { 76, 77, 73, 72 }, 5, 0 },
    { { 71, 70, 76, 77 }, 9, 0 },
    { { 59, 58, 78, 79 }, 17, 0 },
    { { 81, 82, 80, 83 }, 18, 0 },
    { { 85, 86, 84, 87 }, 19, 0 },
    { { 89, 85, 88, 84 }, 20, 0 },
    { { 45, 44, 88, 89 }, 9, 0 },
    { { 86, 90, 87, 91 }, 21, 0 },
    { { 90, 81, 91, 80 }, 22, 0 },
    { { 82, 78, 83, 79 }, 23, 0 },
    { { 17, 92, 18, 93 }, 4, 3 },
    { { 18, 94, 19, 95 }, 4, 2 },
    { { 16, 19, 96, 95 }, 4, 1 },
    { { 16, 96, 17, 97 }, 4, 2 },
    { { 99, 61, 98, 60 }, 9, 0 },
    { { 74, 99, 75, 98 }, 24, 0 },
    { { 101, 102, 100, 103 }, 9, 0 },
    { { 100, 104, 101, 105 }, 25, 0 },
};

s16 D_neo_ark_observatory_80181CF0[15] = {
    0,
    1,
    2,
    3,
    4,
    5,
    18,
    21,
    24,
    26,
    27,
    29,
    40,
    43,
    -1,
};

s16 D_neo_ark_observatory_80181D10[19] = {
    0,
    1,
    2,
    3,
    4,
    5,
    18,
    24,
    25,
    28,
    29,
    32,
    33,
    34,
    37,
    38,
    39,
    40,
    -1,
};

s16 D_neo_ark_observatory_80181D38[18] = {
    0,
    1,
    2,
    3,
    4,
    7,
    13,
    15,
    25,
    28,
    33,
    34,
    35,
    36,
    37,
    38,
    40,
    -1,
};

s16 D_neo_ark_observatory_80181D5C[15] = {
    0,
    1,
    2,
    3,
    4,
    6,
    7,
    8,
    9,
    11,
    13,
    14,
    40,
    41,
    -1,
};

s16 D_neo_ark_observatory_80181D7C[11] = {
    0,
    1,
    2,
    3,
    4,
    7,
    8,
    9,
    40,
    41,
    -1,
};

s16 D_neo_ark_observatory_80181D94[26] = {
    0,
    1,
    2,
    3,
    4,
    5,
    17,
    19,
    20,
    21,
    24,
    26,
    27,
    29,
    30,
    31,
    32,
    39,
    40,
    42,
    43,
    44,
    45,
    46,
    47,
    -1,
};

s16 D_neo_ark_observatory_80181DC8[25] = {
    0,
    1,
    2,
    3,
    4,
    5,
    17,
    19,
    20,
    27,
    29,
    30,
    32,
    33,
    37,
    38,
    39,
    40,
    42,
    43,
    44,
    45,
    46,
    47,
    -1,
};

s16 D_neo_ark_observatory_80181DFC[23] = {
    0,
    1,
    2,
    3,
    4,
    6,
    10,
    11,
    14,
    15,
    16,
    17,
    20,
    22,
    34,
    35,
    36,
    37,
    38,
    40,
    41,
    42,
    -1,
};

s16 D_neo_ark_observatory_80181E2C[22] = {
    0,
    1,
    2,
    3,
    4,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    14,
    15,
    17,
    22,
    35,
    36,
    40,
    41,
    42,
    -1,
};

s16 D_neo_ark_observatory_80181E58[13] = {
    0,
    1,
    2,
    3,
    4,
    7,
    8,
    9,
    10,
    12,
    40,
    41,
    -1,
};

s16 D_neo_ark_observatory_80181E74[18] = {
    0,
    1,
    2,
    3,
    4,
    17,
    19,
    20,
    27,
    30,
    31,
    42,
    43,
    44,
    45,
    46,
    47,
    -1,
};

s16 D_neo_ark_observatory_80181E98[15] = {
    0,
    1,
    2,
    3,
    4,
    17,
    19,
    20,
    42,
    43,
    44,
    45,
    46,
    47,
    -1,
};

s16 D_neo_ark_observatory_80181EB8[14] = {
    0,
    1,
    2,
    3,
    4,
    15,
    16,
    17,
    20,
    22,
    36,
    41,
    42,
    -1,
};

s16 D_neo_ark_observatory_80181ED4[15] = {
    0,
    1,
    2,
    3,
    4,
    10,
    11,
    12,
    16,
    17,
    20,
    22,
    41,
    42,
    -1,
};

s16 D_neo_ark_observatory_80181EF4[6] = {
    0,
    1,
    2,
    3,
    41,
    -1,
};

s16 D_neo_ark_observatory_80181F00[7] = {
    0,
    1,
    2,
    3,
    42,
    43,
    -1,
};

s16 D_neo_ark_observatory_80181F10[7] = {
    0,
    1,
    2,
    3,
    42,
    43,
    -1,
};

s16 D_neo_ark_observatory_80181F20[10] = {
    0,
    1,
    2,
    3,
    16,
    22,
    23,
    41,
    42,
    -1,
};

s16 D_neo_ark_observatory_80181F34[10] = {
    0,
    1,
    2,
    3,
    16,
    22,
    23,
    41,
    42,
    -1,
};

s16 D_neo_ark_observatory_80181F48[6] = {
    0,
    1,
    2,
    3,
    41,
    -1,
};

s16 * D_neo_ark_observatory_80181F54[20] = {
    D_neo_ark_observatory_80181CF0,
    D_neo_ark_observatory_80181D10,
    D_neo_ark_observatory_80181D38,
    D_neo_ark_observatory_80181D5C,
    D_neo_ark_observatory_80181D7C,
    D_neo_ark_observatory_80181D94,
    D_neo_ark_observatory_80181DC8,
    D_neo_ark_observatory_80181DFC,
    D_neo_ark_observatory_80181E2C,
    D_neo_ark_observatory_80181E58,
    D_neo_ark_observatory_80181E74,
    D_neo_ark_observatory_80181E98,
    D_neo_ark_observatory_80181EB8,
    D_neo_ark_observatory_80181ED4,
    D_neo_ark_observatory_80181EF4,
    D_neo_ark_observatory_80181F00,
    D_neo_ark_observatory_80181F10,
    D_neo_ark_observatory_80181F20,
    D_neo_ark_observatory_80181F34,
    D_neo_ark_observatory_80181F48,
};

GpGridParams D_neo_ark_observatory_80181FA4 = { NULL, D_neo_ark_observatory_80181690, D_neo_ark_observatory_80181760, D_neo_ark_observatory_80181AB0, D_neo_ark_observatory_80181F54, 0, 0, 4, 5, 4000, 48 };

GpViewRec D_neo_ark_observatory_80181FC8[21] = {
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

GpSprtCmd D_neo_ark_observatory_801822BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_observatory_801822CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_801822DC[21] = {
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

GpSprtCmd D_neo_ark_observatory_80182480[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80182498[19] = {
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

GpSprtCmd D_neo_ark_observatory_80182614[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_8018262C[21] = {
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

GpSprtCmd D_neo_ark_observatory_801827D0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_801827E8[82] = {
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

GpSprtCmd D_neo_ark_observatory_80182E50[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { 26, 10, 0, 0, { 3, 0 } },
    { 36, 23, 0, 0, { 2, 0 } },
    { 59, 8, 0, 0, { 4, 0 } },
    { 67, 15, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80182E88[83] = {
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
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 96, 1096, { .fields = { 96, 80 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, 48, 796, { .fields = { 80, 0 } }, 128, 128, 128, 2 },
    { 141, 0x4000, { .fields = { 48, 24 } }, -160, 96, 789, { .fields = { 104, 72 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -112, 48, 960, { .fields = { 104, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -112, 96, 956, { .fields = { 56, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, 48, 1106, { .fields = { 96, 48 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 16, 40 } }, -152, 48, 1437, { .fields = { 48, 40 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 40 } }, -136, 48, 1437, { .fields = { 40, 120 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 32, 48 } }, -112, 48, 1300, { .fields = { 120, 0 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_neo_ark_observatory_80183504[11] = {
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
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_8018355C[94] = {
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
    { 143, 0x4000, { .fields = { 8, 8 } }, 80, 80, 800, { .fields = { 16, 152 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 72, 104, 800, { .fields = { 24, 152 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 8, 8 } }, -136, 112, 800, { .fields = { 96, 16 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -48, 48, 800, { .fields = { 88, 200 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -16, 48, 800, { .fields = { 112, 240 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -16, 64, 800, { .fields = { 64, 80 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -80, 48, 800, { .fields = { 64, 32 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 16, 48, 800, { .fields = { 64, 112 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 16, 64, 800, { .fields = { 104, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -80, 64, 800, { .fields = { 96, 216 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -48, 64, 800, { .fields = { 96, 128 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 8 } }, -104, 56, 800, { .fields = { 48, 184 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -112, 64, 800, { .fields = { 112, 152 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 48, 64, 800, { .fields = { 112, 32 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 48, 88, 800, { .fields = { 72, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, 88, 800, { .fields = { 16, 240 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -112, 88, 800, { .fields = { 80, 152 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -136, 64, 800, { .fields = { 0, 216 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -136, 88, 800, { .fields = { 120, 80 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -160, 88, 800, { .fields = { 120, 56 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 24, 8 } }, -160, 112, 800, { .fields = { 112, 24 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -152, 80, 800, { .fields = { 24, 248 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 80, 88, 800, { .fields = { 40, 96 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 80, 104, 800, { .fields = { 40, 48 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 16, 16 } }, 112, 104, 800, { .fields = { 80, 232 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_neo_ark_observatory_80183CB4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 1, 0 } },
    { 40, 29, 0, 0, { 2, 0 } },
    { 69, 25, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80183CDC[69] = {
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
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, 48, 650, { .fields = { 80, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 24 } }, 112, 96, 650, { .fields = { 32, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 32, 48 } }, 80, 48, 700, { .fields = { 96, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 80, 96, 700, { .fields = { 48, 208 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 48 } }, 64, 48, 750, { .fields = { 0, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 64, 96, 750, { .fields = { 96, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, 56, 48, 800, { .fields = { 64, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 16, 48 } }, 56, 48, 1300, { .fields = { 88, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 24, 48 } }, 72, 48, 1300, { .fields = { 72, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 32, 40 } }, 96, 48, 1350, { .fields = { 80, 128 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_neo_ark_observatory_80184240[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 18, 0, 0, { 4, 0 } },
    { 26, 9, 0, 0, { 1, 0 } },
    { 35, 13, 0, 0, { 5, 0 } },
    { 48, 11, 0, 0, { 0, 0 } },
    { 59, 7, 0, 0, { 6, 0 } },
    { 66, 3, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80184288[76] = {
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

GpSprtCmd D_neo_ark_observatory_80184878[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 18, 0, 0, { 0, 0 } },
    { 37, 21, 0, 0, { 5, 0 } },
    { 58, 14, 0, 0, { 1, 0 } },
    { 72, 2, 0, 0, { 4, 0 } },
    { 74, 2, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_801848B8[78] = {
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

GpSprtCmd D_neo_ark_observatory_80184ED0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 3, 0 } },
    { 20, 18, 0, 0, { 0, 0 } },
    { 38, 22, 0, 0, { 5, 0 } },
    { 60, 14, 0, 0, { 1, 0 } },
    { 74, 2, 0, 0, { 4, 0 } },
    { 76, 2, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80184F10[23] = {
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

GpSprtCmd D_neo_ark_observatory_801850DC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 23, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_observatory_801850F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80185104[14] = {
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

GpSprtCmd D_neo_ark_observatory_8018521C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_observatory_80185234[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_observatory_80185244[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80185254[21] = {
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

GpSprtCmd D_neo_ark_observatory_801853F8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 21, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80185410[82] = {
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

GpSprtCmd D_neo_ark_observatory_80185A78[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { 26, 10, 0, 0, { 3, 0 } },
    { 36, 23, 0, 0, { 2, 0 } },
    { 59, 8, 0, 0, { 4, 0 } },
    { 67, 15, 0, 0, { 1, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_observatory_80185AB0[74] = {
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
    { 142, 0x4000, { .fields = { 8, 8 } }, -88, 96, 1096, { .fields = { 48, 104 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, -160, 48, 796, { .fields = { 32, 48 } }, 128, 128, 128, 2 },
    { 141, 0x4000, { .fields = { 48, 24 } }, -160, 96, 789, { .fields = { 120, 136 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -112, 48, 960, { .fields = { 8, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -112, 96, 956, { .fields = { 8, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -88, 48, 1106, { .fields = { 48, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 16, 40 } }, -152, 48, 1437, { .fields = { 104, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4040, { .fields = { 24, 40 } }, -136, 48, 1437, { .fields = { 112, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4040, { .fields = { 32, 48 } }, -112, 48, 1300, { .fields = { 64, 96 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_neo_ark_observatory_80186078[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { 16, 11, 0, 0, { 4, 0 } },
    { 27, 12, 0, 0, { 3, 0 } },
    { 39, 7, 0, 0, { 5, 0 } },
    { 46, 7, 0, 0, { 1, 0 } },
    { 53, 12, 0, 0, { 6, 0 } },
    { 65, 6, 0, 0, { 7, 0 } },
    { 71, 3, 0, 0, { 2, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_observatory_801860C8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_observatory_801860D8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_observatory_801860E8[21] = {
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

GpPointLight D_neo_ark_observatory_801861E4[17] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CF2, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9184, -2500, 0x2EC6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 9640 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 7413 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 5559 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6982, -2500, 3913 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 6335 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 9409 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 0x37E0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -802, -2500, 8240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CD5, -2337, 0x2BA7 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 906, 669, { 0, 0 } }, 100, 1600 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 0x2F1B } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 3999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 1447 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3103, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 1837 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 7000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 0x36FE } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 7000 },
};

GpRoomCoordSet D_neo_ark_observatory_80186844[1] = {
    { 0, NULL, 17, D_neo_ark_observatory_801861E4, 0, NULL },
};

GpPointLight D_neo_ark_observatory_8018685C[17] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CD5, -2337, 0x2BA7 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 906, 669, { 0, 0 } }, 100, 1600 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CF2, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9184, -2500, 0x2EC6 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 9640 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 7413 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8002, -2500, 5559 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6982, -2500, 3913 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 6335 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3998, -2500, 9409 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 0x37E0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -802, -2500, 8240 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 0x2F1B } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -70, -2500, 3999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 8000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7441, -2000, 1447 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 4500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3103, -2500, 0x2EC5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 1837 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 7000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1155, -2500, 0x36FE } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3115, 2954, 2717, { 0, 0 } }, 2000, 7000 },
};

GpRoomCoordSet D_neo_ark_observatory_80186EBC[1] = {
    { 0, NULL, 17, D_neo_ark_observatory_8018685C, 0, NULL },
};

GpObj4C D_neo_ark_observatory_80186ED4[18] = {
    { NULL, NULL, NULL, { 8851, -1408, 0x2EEF, 0 }, { { 506, -1872, -1859, 0 }, { -506, -1872, 1859, 0 }, { 506, 1872, -1859, 0 }, { -506, 1872, 1859, 0 } }, { 3953, 0, 1076, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 8955, -1440, 0x2F0C, 0 }, { { -524, -1840, 1842, 0 }, { 524, -1840, -1841, 0 }, { -524, 1840, 1842, 0 }, { 524, 1840, -1841, 0 } }, { -3946, 0, -1124, 0 }, { 0, 0, 4096, 0 }, 2648, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 8047, -1440, 9136, 0 }, { { -1596, -1840, 162, 0 }, { 1596, -1840, -161, 0 }, { -1596, 1840, 162, 0 }, { 1596, 1840, -161, 0 } }, { -415, 0, -4088, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 8032, -1472, 8993, 0 }, { { 1668, -1840, -158, 0 }, { -1668, -1840, 159, 0 }, { 1668, 1840, -158, 0 }, { -1668, 1840, 159, 0 } }, { 386, 0, 4081, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 7904, -1505, 6720, 0 }, { { 1732, -1840, 2, 0 }, { -1732, -1840, -1, 0 }, { 1732, 1840, 2, 0 }, { -1732, 1840, -1, 0 } }, { -4, 0, 4105, 0 }, { 0, 0, 4096, 0 }, 2521, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 7952, -1441, 6861, 0 }, { { -1900, -1840, 21, 0 }, { 1900, -1840, -21, 0 }, { -1900, 1840, 21, 0 }, { 1900, 1840, -21, 0 } }, { -46, 0, -4106, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 8128, -1536, 3984, 0 }, { { -1900, -1840, 1573, 0 }, { 1900, -1840, -1573, 0 }, { -1900, 1840, 1573, 0 }, { 1900, 1840, -1573, 0 } }, { -2629, 0, -3176, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 7970, -1536, 3937, 0 }, { { 1908, -1840, -1595, 0 }, { -1908, -1840, 1595, 0 }, { 1908, 1840, -1595, 0 }, { -1908, 1840, 1595, 0 } }, { 2629, 0, 3145, 0 }, { 0, 0, 4096, 0 }, 3093, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 5121, -1600, 5120, 0 }, { { -12, -1840, -2459, 0 }, { 12, -1840, 2459, 0 }, { -12, 1840, -2459, 0 }, { 12, 1840, 2459, 0 } }, { 4108, 0, -21, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 5297, -1568, 4705, 0 }, { { 4, -1840, 1829, 0 }, { -4, -1840, -1829, 0 }, { 4, 1840, 1829, 0 }, { -4, 1840, -1829, 0 } }, { -4106, 0, 8, 0 }, { 0, 0, 4096, 0 }, 2585, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 4289, -1568, 1072, 0 }, { { 1060, -1840, 1717, 0 }, { -1060, -1840, -1717, 0 }, { 1060, 1840, 1717, 0 }, { -1060, 1840, -1717, 0 } }, { -3492, 0, 2154, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 7, 10, 1, 0 },
    { NULL, NULL, NULL, { 4224, -1633, 1297, 0 }, { { -988, -1840, -1563, 0 }, { 989, -1840, 1563, 0 }, { -988, 1840, -1563, 0 }, { 989, 1840, 1563, 0 } }, { 3466, 0, -2194, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 10, 7, 1, 0 },
    { NULL, NULL, NULL, { 1776, -1600, 6129, 0 }, { { -2540, -1840, -843, 0 }, { 2541, -1840, 843, 0 }, { -2540, 1840, -843, 0 }, { 2541, 1840, 843, 0 } }, { 1291, 0, -3893, 0 }, { 0, 0, 4096, 0 }, 3248, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 1921, -1569, 5986, 0 }, { { 2420, -1840, 853, 0 }, { -2420, -1840, -853, 0 }, { 2420, 1840, 853, 0 }, { -2420, 1840, -853, 0 } }, { -1363, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 3156, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 1888, -1505, 0x2831, 0 }, { { 2356, -1840, -1051, 0 }, { -2356, -1840, 1051, 0 }, { 2356, 1840, -1051, 0 }, { -2356, 1840, 1051, 0 } }, { 1677, 0, 3761, 0 }, { 0, 0, 4096, 0 }, 3166, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 1936, -1409, 0x28C1, 0 }, { { -2524, -1840, 1093, 0 }, { 2524, -1840, -1093, 0 }, { -2524, 1840, 1093, 0 }, { 2524, 1840, -1093, 0 } }, { -1629, 0, -3762, 0 }, { 0, 0, 4096, 0 }, 3308, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { 4400, -1440, 0x3A80, 0 }, { { 836, -1840, -1787, 0 }, { -836, -1840, 1787, 0 }, { 836, 1840, -1787, 0 }, { -836, 1840, 1787, 0 } }, { 3717, 0, 1738, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 11, 9, 1, 0 },
    { NULL, NULL, NULL, { 4576, -1440, 0x3AE1, 0 }, { { -924, -1840, 1861, 0 }, { 924, -1840, -1861, 0 }, { -924, 1840, 1861, 0 }, { 924, 1840, -1861, 0 } }, { -3674, 0, -1825, 0 }, { 0, 0, 4096, 0 }, 2769, 0, 9, 11, 129, 0 },
};

GpObj4C D_neo_ark_observatory_8018742C[14] = {
    { NULL, NULL, NULL, { 0x3540, -48, 0x2EC0, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1159, 5, 10, 33, 2, 0 },
    { NULL, NULL, NULL, { 7584, -256, 0x3780, 0 }, { { 0, 1232, -960, 0 }, { 0, -1232, -960, 0 }, { 0, 1232, 960, 0 }, { 0, -1232, 960, 0 } }, { 4105, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1557, 0, 10, 33, 3, 0 },
    { NULL, NULL, NULL, { 7552, -272, 1728, 0 }, { { 0, 1184, -1280, 0 }, { 0, -1184, -1280, 0 }, { 0, 1184, 1280, 0 }, { 0, -1184, 1280, 0 } }, { 4106, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1740, 0, 19, 50, 3, 0 },
    { NULL, NULL, NULL, { 0x2B60, -64, 0x2EE0, 0 }, { { 0, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { 0, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4091, 0, 201, 0 }, 1159, 0x8005, 1, 0, 3, 0 },
    { NULL, NULL, NULL, { 5504, -64, 4096, 0 }, { { -544, 0, -1504, 0 }, { 544, 0, -1504, 0 }, { -544, 0, 1504, 0 }, { 544, 0, 1504, 0 } }, { 0, 4105, 0, 0 }, { -4096, 0, 0, 0 }, 1598, 0x8005, 2, 0, 3, 0 },
    { NULL, NULL, NULL, { 512, -64, 7008, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, 2, 1, 255, 2, 0 },
    { NULL, NULL, NULL, { 2495, -64, 0x34AF, 0 }, { { -2561, 0, -1893, 0 }, { -1033, 0, -2838, 0 }, { 1034, 0, 2839, 0 }, { 2562, 0, 1894, 0 } }, { 0, 4108, 0, 0 }, { 2896, 0, -2896, 0 }, 3176, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 2175, -64, 3008, 0 }, { { 920, 0, -3050, 0 }, { 2325, 0, -1929, 0 }, { -2324, 0, 1930, 0 }, { -920, 0, 3050, 0 } }, { 0, 4112, 0, 0 }, { 3702, 0, 1751, 0 }, 3176, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 5600, -64, 7968, 0 }, { { -3488, 0, -2080, 0 }, { -384, 0, -2080, 0 }, { -3488, 0, 2400, 0 }, { -384, 0, 2400, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 4222, 2, 6, 0, 4, 0 },
    { NULL, NULL, NULL, { 5568, -64, 7968, 0 }, { { -2208, 0, -2912, 0 }, { 1056, 0, -2912, 0 }, { -2208, 0, 3584, 0 }, { 1056, 0, 3584, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 4190, 2, 6, 0, 4, 0 },
    { NULL, NULL, NULL, { 6864, -64, 0x37B0, 0 }, { { -208, 0, -624, 0 }, { 208, 0, -624, 0 }, { -208, 0, 624, 0 }, { 208, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 655, 257, 40, 64, 2, 0 },
    { NULL, NULL, NULL, { 6880, -64, 1760, 0 }, { { -224, 0, -608, 0 }, { 224, 0, -608, 0 }, { -224, 0, 608, 0 }, { 224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 646, 257, 40, 64, 2, 0 },
    { NULL, NULL, NULL, { 9824, -64, 0x2BC0, 0 }, { { -448, 0, -192, 0 }, { 1056, 0, -192, 0 }, { -1024, 0, 1728, 0 }, { 1056, 0, 1728, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 2023, 5, 3, 0, 4, 0 },
    { NULL, NULL, NULL, { 5728, -64, 1152, 0 }, { { -768, 0, -1504, 0 }, { 768, 0, -1504, 0 }, { -768, 0, 1504, 0 }, { 768, 0, 1504, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 1688, 0x8005, 4, 0, 131, 0 },
};

GpAreaTmdRec D_neo_ark_observatory_80187854[2] = {
    { 101, 502, 3, 0, { 0, 0 }, D_80137A60 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_observatory_8018786C[13] = {
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

GpObj3A D_neo_ark_observatory_801878D4[4] = {
    { NULL, NULL, { 6336, -1664, 9200, 0 }, { { 32, 2688, 4016, 0 }, { -32, 2688, -4016, 0 }, { 32, -2688, 4016, 0 }, { -32, -2688, -4016, 0 } }, { 4114, 0, -33, 0 }, { -34, 18 }, 1, 0 },
    { NULL, NULL, { 9248, -1440, 6720, 0 }, { { 32, 2688, 4016, 0 }, { -32, 2688, -4016, 0 }, { 32, -2688, 4016, 0 }, { -32, -2688, -4016, 0 } }, { 4114, 0, -33, 0 }, { -34, 18 }, 1, 0 },
    { NULL, NULL, { 9312, -1376, 2656, 0 }, { { 4016, 2688, -32, 0 }, { -4016, 2688, 32, 0 }, { 4016, -2688, -32, 0 }, { -4016, -2688, 32, 0 } }, { -33, 0, -4115, 0 }, { -34, 18 }, 1, 0 },
    { NULL, NULL, { 0x291F, -1504, 0x349F, 0 }, { { 4013, 2688, 166, 0 }, { -4012, 2688, -165, 0 }, { 4013, -2688, 166, 0 }, { -4012, -2688, -165, 0 } }, { 169, 0, -4111, 0 }, { -34, 18 }, 129, 0 },
};

s32 D_neo_ark_observatory_801879C4[3] = {
    0x10000015,
    0x10000017,
    0x10000019,
};

s32 D_neo_ark_observatory_801879D0[3] = {
    0x10000041,
    0x10000043,
    0x10000055,
};

s32 D_neo_ark_observatory_801879DC[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

GpRoomParamRec D_neo_ark_observatory_801879E8[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_observatory_801879F0[1] = {
    { 0, 0, 1, 0, D_neo_ark_observatory_801879C4 },
};

GpRoomParamRec D_neo_ark_observatory_801879F8[1] = {
    { 0, 0, 1, 0, D_neo_ark_observatory_801879D0 },
};

GpRoomParamRec D_neo_ark_observatory_80187A00[1] = {
    { 0, 0, 1, 0, D_neo_ark_observatory_801879DC },
};

GpRoomParamRec * D_neo_ark_observatory_80187A08[8] = {
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879F0,
    D_neo_ark_observatory_801879F8,
    D_neo_ark_observatory_80187A00,
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879E8,
    D_neo_ark_observatory_801879E8,
};

GpAreaApplyRec D_neo_ark_observatory_80187A28[2] = {
    { 5, 7, 2, 0 },
    { 255, 0, 0, 0 },
};

RoomDeparture D_neo_ark_observatory_80187A30 = { 0 };

s16 D_neo_ark_observatory_80187A3C = 0;

/// Sets up the room's mirror: re-attaches the player's own TMD source
/// to this task so the reflection draws the same model, allocates the
/// `RoomMirrorWork` block the reflection's coordinate frame and matrices live
/// in, and hangs the task off the player task so it dies with it.
/// `spawnArg1` must be 0 or 1, and 0 also raises `GameSession::field_4E`. The
/// two child tasks reflect the player's held-object tasks
/// (`GameActor::field_920` / `field_924`). Runs the first per-frame update
/// before returning.
static void func_neo_ark_observatory_8017D6F4(Task* task)
{
    Task*           owner;
    GameActor*      actor;
    TmdObject*      extra;
    GpCoord*        parts;
    RoomMirrorWork* work;
    Task*           child;
    Task*           spawned;
    s32             i;

    owner = gameGetPtrSlot(3);
    if (Gp_AttachTmd(task, owner->extra.tmd->source) == NULL) {
        taskKill(task);
        return;
    }
    extra = task->extra.tmd;
    parts = extra->coords;
    if ((u32)task->spawnArg1.value >= 2U) {
        taskKill(task);
        return;
    }
    work = memCalloc(sizeof(RoomMirrorWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = (TaskIdMap*)work;
    extra->tpage = 6;
    tmdProcessStream(extra);
    tmdProcessStream(extra);
    extra->flags    = 0x10;
    extra->otOffset = 0x1F;
    if (task->spawnArg1.value == 0) {
        gGameSession->field_4E = 1;
    }
    parts->sub      = &work->coord;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    Task_Reparent(owner, task);
    task->state++;
    work->viewFlg   = gGfxViewCoord.flg & 0x7FFFFFFF;
    work->field_4   = 1;
    work->configRev = -1;
    extra->flags   |= 0x80;
    work->field_4   = 0;
    work->viewFlg   = -1;
    actor           = (GameActor*)owner->work;
    for (i = 0; i < 2; i++) {
        child = (&actor->field_920)[i];
        if (child != NULL) {
            spawned = Task_SpawnFromTable(D_neo_ark_observatory_80180DBC, 1, i, task);
            if (spawned != NULL) {
                Task_Reparent(child, spawned);
            }
        }
    }
    func_neo_ark_observatory_8017D8A8(task);
}

/// Per-frame update of the room's mirror, the task
/// `func_neo_ark_observatory_8017D6F4` sets up.
///
/// When the player's equipped weapon changes it spawns reflection tasks for
/// the player's two held-object tasks. When the view moves it rebuilds the
/// reflection's coordinate frame. Mirror 0 copies the view matrix with its
/// second row negated and applies location-specific corrections. The other
/// mirror reflects through a plane chosen by the current stage, area and view.
/// On the frame after mirror 0 rebuilds, it queues packets that copy the frame
/// buffer into the off-screen strip at x = `width`. In stages 1 and 5 it
/// projects the reflected body to find its screen rectangle and, where that
/// overlaps the mirror's clip rectangle, draws quads sampling that strip. Otherwise the reflection is hidden. Every
/// frame it copies the player's pose and light matrices onto the reflection.
static void func_neo_ark_observatory_8017D8A8(Task* task)
{
    RoomMirrorWork*          work;
    PlayerStatus*            status;
    TmdObject*               extra;
    TmdObject*               model;
    Task*                    owner;
    GameActor*               actor;
    Task*                    child;
    Task*                    spawned;
    RoomMirrorPlaneScratch*  plane;
    RoomMirrorExtentScratch* extent;
    GpCoord*                 parts;
    GpCoord*                 refPart;
    DR_AREA*                 drArea;
    DR_STP*                  drStp;
    DR_OFFSET*               drOffset;
    SPRT*                    sprt;
    DR_TPAGE*                tpage;
    TILE*                    tile;
    POLY_FT4*                poly;
    s32                      stage;
    s32                      area;
    s32                      view;
    s32                      width;
    s32                      viewFlg;
    s32                      copyPending;
    s32                      halfWidth;
    s32                      texX;
    s32                      i;
    s32                      layer;
    u32                      j;

    width  = 0x1C0;
    work   = task->work;
    extra  = task->extra.tmd;
    stage  = Mc_SaveData[0].state.at4.loc.stage;
    area   = Mc_SaveData[0].state.at4.loc.area;
    view   = Mc_SaveData[0].state.at4.loc.view;
    status = &Player_Status;
    if (stage == 5) {
        width = 0x140;
    }
    if (work->configRev != status->weapon) {
        actor           = gameGetPtrSlot(3)->work;
        work->configRev = status->weapon;
        for (i = 0; i < 2; i++) {
            child = (&actor->field_918)[i];
            if (child != NULL) {
                spawned = Task_SpawnFromTable(D_neo_ark_observatory_80180DBC, 1, i + 2, task);
                if (spawned != NULL) {
                    Task_Reparent(child, spawned);
                }
            }
        }
    }
    extra->flags |= 0x10;
    viewFlg       = gGfxViewCoord.flg & 0x7FFFFFFF;
    if (work->viewFlg != viewFlg) {
        GpCoord* sub;

        work->viewFlg     = viewFlg;
        sub               = gGfxViewCoord.sub;
        work->field_A0[0] = -0xA0;
        work->field_A0[1] = 0xA0;
        work->coord.flg   = 0;
        work->field_A0[2] = -0x78;
        work->field_A0[3] = 0x78;
        plane             = (RoomMirrorPlaneScratch*)SCRATCH_PUSH_BYTES(0x70);
        work->coord.sub   = sub;
        if (task->spawnArg1.value == 0) {
            work->field_4     = 1;
            work->coord.coord = gGfxViewCoord.coord;
            plane->viewRow.vx = work->coord.coord.m[1][0];
            plane->viewRow.vy = work->coord.coord.m[1][1];
            plane->viewRow.vz = work->coord.coord.m[1][2];
            gte_lddp(-0x1000);
            gte_ldsv(&plane->viewRow);
            gte_gpf12();
            gte_stsv(&plane->viewRow);
            work->coord.coord.m[1][0] = plane->viewRow.vx;
            work->coord.coord.m[1][1] = plane->viewRow.vy;
            work->coord.coord.m[1][2] = plane->viewRow.vz;
            if (stage == 5) {
                if (area == 7) {
                    if (view >= 6 && view < 12 && gGameSession->at4.loc.room == 2) {
                        work->coord.coord.t[1] += 0x9B;
                        extra->flags           &= ~0x80;
                        work->field_8           = 0;
                    } else {
                        work->field_4 = 0;
                        extra->flags |= 0x80;
                    }
                }
            } else if (area == 1) {
                extra->flags |= 0x80;
                if (view == 9) {
                    work->field_4 = 0;
                }
            } else {
                if (area != 0x11) {
                    work->coord.coord.t[1] += 0x69;
                }
                work->field_8 = 1;
                if ((area == 0x11 && view == 5) || (area == 2 && (view == 7 || view == 5))) {
                    extra->flags |= 0x80;
                } else {
                    extra->flags &= ~0x80;
                }
            }
        } else {
            model         = task->extra.tmd;
            model->flags &= ~0x80;
            if (stage == 1) {
                switch (area) {
                    case 0x11:
                        switch (view) {
                            case 2:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = -0x1518;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 3:
                                plane->normal.vx = 0x64;
                                plane->normal.vy = 0;
                                plane->normal.vz = -0x384;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = -0x640;
                                break;
                            case 4:
                                work->field_A0[0] = -0x14;
                                work->field_A0[1] = 0x14;
                                plane->normal.vx  = -0x1000;
                                plane->normal.vy  = 0;
                                plane->normal.vz  = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x1644;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    case 1:
                        switch (view) {
                            case 6:
                                work->field_A0[1] = 0x64;
                                work->field_A0[0] = 0;
                                plane->normal.vx  = -0x1000;
                                plane->normal.vy  = 0;
                                plane->normal.vz  = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x1AF4;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                model->otOffset  = 0x1F;
                                break;
                            case 7:
                            case 8:
                                plane->normal.vx = 0;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0x1000;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0x14B4;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    case 2:
                        switch (view) {
                            case 2:
                            case 5:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0x170C;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 4:
                                plane->normal.vx = -0x1000;
                                plane->normal.vy = 0;
                                plane->normal.vz = 0;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = -0x1644;
                                plane->offset.vy = 0;
                                plane->offset.vz = 0;
                                break;
                            case 3:
                                plane->normal.vx = -0x64;
                                plane->normal.vy = 0;
                                plane->normal.vz = -0x384;
                                VectorNormalSS(&plane->normal, &plane->normal);
                                plane->offset.vx = 0;
                                plane->offset.vy = 0;
                                plane->offset.vz = -0x640;
                                break;
                            default:
                                model->flags |= 0x80;
                                break;
                        }
                        break;
                    default:
                        model->flags |= 0x80;
                        break;
                }
            } else if (view == 8 || view == 1) {
                plane->normal.vx = -0x1000;
                plane->normal.vy = 0;
                plane->normal.vz = 0;
                VectorNormalSS(&plane->normal, &plane->normal);
                plane->offset.vx = 0xA38;
                plane->offset.vy = 0;
                plane->offset.vz = 0;
            } else {
                model->flags |= 0x80;
            }
            if (!(model->flags & 0x80)) {
                plane->leastAbs = plane->normal.vx;
                if (plane->leastAbs < 0) {
                    plane->leastAbs = -plane->leastAbs;
                }
                plane->leastAxis = 0;
                plane->axisAbs   = plane->normal.vy;
                if (plane->axisAbs < 0) {
                    plane->axisAbs = -plane->axisAbs;
                }
                if (plane->leastAbs > plane->axisAbs) {
                    plane->leastAbs  = plane->axisAbs;
                    plane->leastAxis = 1;
                }
                plane->axisAbs = plane->normal.vz;
                if (plane->axisAbs < 0) {
                    plane->axisAbs = -plane->axisAbs;
                }
                if (plane->leastAbs > plane->axisAbs) {
                    plane->leastAbs  = plane->axisAbs;
                    plane->leastAxis = 2;
                }
                plane->refAxis.vx = 0;
                if (plane->leastAxis == 0) {
                    plane->refAxis.vx = 0x1000;
                }
                plane->refAxis.vy = 0;
                if (plane->leastAxis == 1) {
                    plane->refAxis.vy = 0x1000;
                }
                plane->refAxis.vz = 0;
                if (plane->leastAxis == 2) {
                    plane->refAxis.vz = 0x1000;
                }
                Gfx_OrthonormalBasis(&plane->basis, &plane->normal, &plane->refAxis);
                gte_TransposeMatrix(&plane->basis, &plane->reflect);
                plane->reflect.m[2][0] = -plane->reflect.m[2][0];
                plane->reflect.m[2][1] = -plane->reflect.m[2][1];
                plane->reflect.m[2][2] = -plane->reflect.m[2][2];
                gte_MulMatrix0(&plane->basis, &plane->reflect, &plane->reflect);
                work->coord.coord      = plane->reflect;
                work->coord.coord.t[0] = gGfxViewCoord.coord.t[0] + plane->offset.vx;
                work->coord.coord.t[1] = gGfxViewCoord.coord.t[1] + plane->offset.vy;
                work->coord.coord.t[2] = gGfxViewCoord.coord.t[2] + plane->offset.vz;
                gfxRotateSv(&plane->reflect, &plane->offset);
                work->coord.coord.t[0] -= plane->offset.vx;
                work->coord.coord.t[1] -= plane->offset.vy;
                work->coord.coord.t[2] -= plane->offset.vz;
                work->field_8           = 1;
            }
        }
        work->field_C = extra->flags;
        SCRATCH_POP_BYTES(0x70);
    }

    copyPending = work->field_4;
    if (copyPending == 1 && task->spawnArg1.value == 0 && !(area == 1 && view == 0xF) && gDisplayState.pendingMode == 0) {
        u16  ofs[2];
        RECT rect;

        work->field_4   = 0;
        drArea          = (DR_AREA*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_AREA);
        rect.x          = 0;
        rect.y          = gDisplayState.drawBuffer * 0x110;
        rect.w          = 0x140;
        rect.h          = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);

        drStp           = (DR_STP*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_STP);
        SetDrawStp(drStp, 0);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset        = (DR_OFFSET*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_OFFSET);
        ofs[0]          = 0xA0;
        ofs[1]          = gDisplayState.drawBuffer * 0x110 + 0x78;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        sprt            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(SPRT);
        sprt->x0        = -0xA0;
        sprt->y0        = -0x78;
        sprt->w         = 0xA0;
        sprt->h         = 0xF0;
        sprt->u0        = 0;
        sprt->v0        = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage           = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        sprt            = (SPRT*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(SPRT);
        sprt->x0        = 0;
        sprt->y0        = -0x78;
        sprt->w         = 0xA0;
        sprt->h         = 0xF0;
        sprt->u0        = 0x20;
        sprt->v0        = gDisplayState.drawBuffer << 4;
        setlen(sprt, 4);
        setcode(sprt, 0x65);
        addPrim(&gGpuCurrentOt[0x3FF], sprt);

        tpage           = (DR_TPAGE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_TPAGE);
        setDrawTPage(tpage, 1, 1, getTPage(2, 0, 0x80, gDisplayState.drawBuffer << 8));
        addPrim(&gGpuCurrentOt[0x3FF], tpage);

        tile            = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(TILE);
        setlen(tile, 3);
        setcode(tile, 0x60);
        tile->x0 = -0xA0;
        tile->y0 = -0x78;
        tile->r0 = tile->g0 = 2;
        tile->b0            = 2;
        tile->w             = 0x140;
        tile->h             = 0xF0;
        addPrim(&gGpuCurrentOt[0x3FF], tile);

        drStp           = (DR_STP*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_STP);
        SetDrawStp(drStp, 1);
        addPrim(&gGpuCurrentOt[0x3FF], drStp);

        drOffset        = (DR_OFFSET*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_OFFSET);
        ofs[0]          = width + 0xA0;
        ofs[1]          = 0x178;
        SetDrawOffset(drOffset, ofs);
        addPrim(&gGpuCurrentOt[0x3FF], drOffset);

        drArea          = (DR_AREA*)gGpuPrimCursor;
        gGpuPrimCursor += sizeof(DR_AREA);
        rect.x          = width;
        rect.y          = 0x100;
        rect.w          = 0x140;
        rect.h          = 0xF0;
        SetDrawArea(drArea, &rect);
        addPrim(&gGpuCurrentOt[0x3FF], drArea);
    }

    extra->flags = work->field_C;
    if (!(extra->flags & 0x80) && gGameSession->field_65 == 0) {
        parts   = task->extra.tmd->coords;
        owner   = gameGetPtrSlot(3);
        refPart = &parts[1];
        if (owner != NULL) {
            TmdObject* src       = owner->extra.tmd;
            GpCoord*   srcCoords = src->coords;

            parts->flg = 0;
            j          = 0;
            if (src->partCount != 0) {
                GpCoord* from = (GpCoord*)&srcCoords->coord;
                GpCoord* to   = (GpCoord*)&parts->coord;

                do {
                    *(MATRIX*)to = *(MATRIX*)from;
                    to++;
                    from++;
                } while (++j < src->partCount);
            }
        }
        if (stage == 1 || stage == 5) {
            extent = (RoomMirrorExtentScratch*)SCRATCH_PUSH_BYTES(0x34);
            if (gGameSession->eventState != 0) {
                Gp_UpdateCoord(refPart);
                gte_SetTransMatrix(&refPart->workm);
                gte_SetRotMatrix(&refPart->workm);
                extent->pos.vx = 0;
                extent->pos.vy = -0x3E8;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyHead, &extent->dp, &extent->flag, &extent->otzHead);
                extent->pos.vx = 0;
                extent->pos.vy = 0x3E8;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyFoot, &extent->dp, &extent->flag, &extent->otzFoot);
            } else {
                Gp_UpdateCoord(parts);
                gte_SetTransMatrix(&parts->workm);
                gte_SetRotMatrix(&parts->workm);
                extent->pos.vx = 0;
                extent->pos.vy = -0x7D0;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyHead, &extent->dp, &extent->flag, &extent->otzHead);
                extent->pos.vx = 0;
                extent->pos.vy = 0;
                extent->pos.vz = 0;
                gte_RotTransPers(&extent->pos, &extent->sxyFoot, &extent->dp, &extent->flag, &extent->otzFoot);
            }
            if (extent->sxyFoot.vy > extent->sxyHead.vy) {
                extent->sxyHead.vx = extent->sxyFoot.vy;
                extent->sxyFoot.vy = extent->sxyHead.vy;
                extent->sxyHead.vy = extent->sxyHead.vx;
            }
            extent->sxyFoot.vy -= 0x10;
            extent->sxyHead.vy += 0x10;
            halfWidth           = (extent->sxyHead.vy - extent->sxyFoot.vy) >> 1;
            if (halfWidth >= 0x60) {
                halfWidth = 0x5F;
            }
            if ((GP_LOC_WORD(Mc_SaveData[0].state.at4.loc) & GP_LOC_AREA_VIEW) == GP_LOC_KEY(0, 2, 0, 5)) {
                if (task->spawnArg1.value == 0) {
                    halfWidth = 0x5F;
                } else {
                    extent->otzFoot = extent->otzHead + 0xA;
                }
            }
            extent->left = extent->sxyFoot.vx - halfWidth;
            if (extent->left < -0xA0) {
                extent->left = -0xA0;
            }
            extent->right = extent->sxyFoot.vx + halfWidth;
            if (extent->right > 0xA0) {
                extent->right = 0xA0;
            }
            extent->top = extent->sxyFoot.vy;
            if (extent->top < -0x78) {
                extent->top = -0x78;
            }
            extent->bottom = extent->sxyHead.vy;
            if (extent->bottom > 0x78) {
                extent->bottom = 0x78;
            }
            if (extent->top < work->field_A0[3] && work->field_A0[2] < extent->bottom && extent->left < work->field_A0[1] &&
                work->field_A0[0] < extent->right) {
                DR_TPAGE* mode;

                mode            = (DR_TPAGE*)gGpuPrimCursor;
                texX            = extent->left + (u16)(width + 0xA0);
                extent->texX    = texX & 0xFFC0;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 1, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
                for (layer = work->field_8; layer < 3; layer++) {
                    poly            = (POLY_FT4*)gGpuPrimCursor;
                    gGpuPrimCursor += sizeof(POLY_FT4);
                    setPolyFT4(poly);
                    setSemiTrans(poly, 1);
                    if (work->field_8 == 1) {
                        setShadeTex(poly, 0);
                        poly->r0 = poly->g0 = poly->b0 = 0x80;
                    } else {
                        setShadeTex(poly, 1);
                    }
                    poly->x0 = poly->x2 = extent->left;
                    poly->x1 = poly->x3 = extent->right;
                    poly->y0 = poly->y1 = extent->top;
                    poly->y2 = poly->y3 = extent->bottom;
                    poly->tpage         = getTPage(2, layer, extent->texX, 0x100);
                    poly->u0 = poly->u2 = poly->x0 + 0xA0 + width - extent->texX;
                    poly->u1 = poly->u3 = poly->x1 + 0xA0 + width - extent->texX;
                    poly->v0 = poly->v1 = poly->y0 + 0x78;
                    poly->v2 = poly->v3 = poly->y2 + 0x78;
                    addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                            poly);
                }
                mode            = (DR_TPAGE*)gGpuPrimCursor;
                gGpuPrimCursor += sizeof(DR_TPAGE);
                setDrawTPage(mode, 0, 0, 0);
                addPrim(&gGpuCurrentOt[(((extent->otzFoot << gDisplayState.otDepthShift) & 0x3FFF) >> 4) + extra->otOffset - 15],
                        mode);
            } else {
                extra->flags |= 0x80;
            }
            SCRATCH_POP_BYTES(0x34);
        }
    }

    {
        GpCoord*   ownerParts;
        TmdObject* ownerBody;
        GpCoord*   ownParts;
        MATRIX     mtx;

        ownerParts  = gameGetPtrSlot(3)->extra.tmd->coords;
        ownerBody   = gameGetPtrSlot(3)->extra.tmd;
        ownParts    = task->extra.tmd->coords;
        work->light = *ownerBody->lightMtx;
        work->color = *ownerBody->colorMtx;
        Gp_UpdateCoord(ownParts);
        gte_TransposeMatrix(&ownParts->workm, &mtx);
        gte_MulMatrix0(&ownerParts->workm, &mtx, &mtx);
        gte_MulMatrix0(&work->light, &mtx, &work->light);
    }
}

/// Scale applied to reflections with `spawnArg1 >= 2`: X negated, Y and Z kept.
static const VECTOR D_neo_ark_observatory_8017D5C4 = { -0x1000, 0x1000, 0x1000 };

/// Per-frame callback of a held-object reflection. `Task::spawnArg2` is the
/// mirror task the room set up and the parent is the held-object task being
/// reflected. On the first frame it clones the parent's TMD source, parents the
/// clone's root coordinate to the mirrored player's corresponding part, points
/// the clone at the mirror's light and color matrices and negates the X
/// translation; every frame it republishes the mirror model's draw flags onto
/// the clone.
void func_neo_ark_observatory_8017F22C(Task* task)
{
    Task*           mirror;
    TmdObject*      mirrorExtra;
    RoomMirrorWork* work;
    GpCoord*        mirrorPart;
    TmdObject*      src;
    GpCoord*        srcParts;
    TmdObject*      extra;
    GpCoord*        parts;
    VECTOR          scale;
    u16             flags;

    if (task->parent == NULL) {
        Task_CallExit(task);
    }
    mirror      = (Task*)task->spawnArg2.pointer;
    mirrorPart  = &mirror->extra.tmd->coords[D_neo_ark_observatory_80180DB8[task->spawnArg1.value]];
    work        = (RoomMirrorWork*)mirror->work;
    mirrorExtra = mirror->extra.tmd;
    if (task->state == 0) {
        src      = task->parent->extra.tmd;
        srcParts = src->coords;
        if (Gp_AttachTmd(task, src->source) == NULL) {
            Task_CallExit(task);
            return;
        }
        extra        = task->extra.tmd;
        parts        = extra->coords;
        extra->tpage = src->tpage;
        tmdProcessStream(extra);
        tmdProcessStream(extra);
        extra->flags    = 0x10;
        extra->otOffset = 0x1F;
        parts->sub      = mirrorPart;
        extra->lightMtx = &work->light;
        extra->colorMtx = &work->color;
        if (task->spawnArg1.value >= 2) {
            scale = D_neo_ark_observatory_8017D5C4;
            ScaleMatrix(&parts->coord, &scale);
        }
        parts->coord.t[0] = -srcParts->coord.t[0];
        parts->coord.t[1] = srcParts->coord.t[1];
        parts->coord.t[2] = srcParts->coord.t[2];
        parts->flg        = 0;
        task->state++;
    }
    extra        = task->extra.tmd;
    flags        = mirrorExtra->flags;
    extra->flags = flags;
    if (task->spawnArg1.value >= 2) {
        extra->flags = flags & 0xFFEF;
    }
}

/// Mirror task: runs the set-up state, then the per-frame state.
void func_neo_ark_observatory_8017F3FC(Task* task)
{
    TaskFunc states[2] = {
        func_neo_ark_observatory_8017D6F4,
        func_neo_ark_observatory_8017D8A8,
    };

    states[task->state](task);
}

/// Picks the room a departure into area `arg0->field_0` lands in, for the
/// areas whose room depends on story progress: areas 5, 41 and 45 take a
/// flag nibble plus one, areas 2, 16 and 20 map flag nibbles to fixed rooms.
/// Every other area leaves `arg1->field_3` unchanged. Nothing is done unless
/// `arg0->field_5` is 0.
static s32 func_neo_ark_observatory_8017F44C(MapMarkerRec* arg0, MapMarkerOut* arg1)
{
    if (arg0->field_5 == 0) {
        switch (arg0->field_0) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    arg1->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    arg1->field_3 = 3;
                }
                break;
            case 5:
                arg1->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    arg1->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        arg1->field_3 = 1;
                        break;
                    case 1:
                        arg1->field_3 = 6;
                        break;
                    case 2:
                        arg1->field_3 = 7;
                        break;
                    case 3:
                        arg1->field_3 = 8;
                        break;
                    default:
                        arg1->field_3 = 1;
                        break;
                }
                break;
            case 45:
                arg1->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                arg1->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// Departure task. State 0 stages the descriptor's halfword into a `GpXformArg`
/// record and sends it to the slot-3 game pointer as message 0x3EE - the
/// all-ones halfword is the "nothing staged" marker, and the task skips to
/// state 2 rather than sending it. State 1 polls that same pointer with 0x3F0,
/// states 2 and 3 queue the descriptor's sound event and wait for the voice to
/// go quiet, and each of them advances the state once its call reports 0.
/// State 4 commits the save location the descriptor names, re-spawns the
/// player task as type 0x11 and kills itself.
void func_neo_ark_observatory_8017F588(Task* arg0)
{
    GpXformArg msg;
    void*      slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = D_neo_ark_observatory_80187A30.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsgPtr(slot, 0x3EE, &msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_neo_ark_observatory_80187A30.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_neo_ark_observatory_80187A30.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_neo_ark_observatory_80187A30.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.roomVariant    = 1;
            Mc_SaveData[0].state.at4.loc.stage = D_neo_ark_observatory_80187A30.stage;
            Mc_SaveData[0].state.at4.loc.area  = D_neo_ark_observatory_80187A30.area;
            Mc_SaveData[0].state.at4.loc.warp  = D_neo_ark_observatory_80187A30.warp;
            Mc_SaveData[0].state.at4.loc.room  = D_neo_ark_observatory_80187A30.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// Copies the area, warp and room of `desc` into a resolver record, lets
/// `resolve` rewrite the record in place, and copies the result back.
static __inline__ void _neoArkObservatoryStageMarker(RoomDeparture* desc, _MapMarkerResolve resolve)
{
    MapMarkerRec rec;

    rec.field_0  = desc->area;
    rec.pad_2[0] = desc->warp;
    rec.pad_2[1] = desc->room;
    rec.field_5  = 0;
    resolve(&rec, (MapMarkerOut*)&rec);
    desc->area = rec.field_0;
    desc->warp = rec.pad_2[0];
    desc->room = rec.pad_2[1];
}

s32 func_neo_ark_observatory_8017F6F8(Task* arg0, s32 arg1, GpMsg13EF * arg2, s32 arg3)
{
    RoomDeparture     desc;
    _MapMarkerResolve resolve;
    s32               temp;

    if (arg2->field_2 == 0xA) {
        if (GameFlag_GetNibble(0xD1) == 2) {
            GameFlag_SetNibble(0x4C, 8);
        }
        if (GameFlag_GetNibble(0xF7) == 0) {
            temp = GameFlag_GetNibble(0xDF);
            if (temp == 1) {
                _MapMarkerResolve resolve;

                GameFlag_SetNibble(0xF7, 1);
                desc.stage    = 4;
                desc.area     = 0x12;
                desc.warp     = 3;
                desc.room     = temp;
                desc.sndEvent = 0x55070005;
                desc.facing   = 0x400;
                resolve       = func_neo_ark_observatory_8017F44C;
                Gp_MsgPlayerWeapon(0);
                _neoArkObservatoryStageMarker(&desc, resolve);
                D_neo_ark_observatory_80187A30 = desc;
                Task_SpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
                return 0;
            }
        }
        desc.stage    = 4;
        desc.area     = arg2->field_3;
        desc.room     = 1;
        desc.warp     = 4;
        desc.sndEvent = 0x55070005;
        desc.facing   = 0x400;
        resolve       = func_neo_ark_observatory_8017F44C;
        Gp_MsgPlayerWeapon(0);
        _neoArkObservatoryStageMarker(&desc, resolve);
        D_neo_ark_observatory_80187A30 = desc;
        Task_SpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
    }
    if (arg2->field_2 == 1 && GameFlag_GetNibble(0xD7) == 0) {
        GameFlag_SetNibble(0xD7, 1);
        if (GameFlag_GetNibble(0x83) != 0) {
            func_800E3FAC(0xA2, 0x2C);
            func_800E8634(&D_8013C72C, 0, &D_8013CAEC);
        } else {
            func_800E3FAC(0xA2, 0x2D);
            GameFlag_SetNibble(0xD1, 3);
            func_800E8634(&D_80137EE4, 0, &D_80138694);
        }
    }
    if (arg2->field_2 == 2) {
        if (GameFlag_GetNibble(0xE1) == 0) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
            GameFlag_SetNibble(0xE1, 1);
            Mc_SaveData[0].state.sceneEvent = 0x15;
            func_800E8634(&D_8013FC58, 0, &D_80140078);
        }
    }
    if (arg2->field_2 == 3 && gameGetPtrSlot(0xA) != NULL && gGameSession->at4.loc.view == 2) {
        func_80132220();
    }
    if (arg2->field_2 == 4 && GameFlag_GetNibble(0xDE) != 0 && GameFlag_GetNibble(0x16E) == 0) {
        GameFlag_SetNibble(0x16E, 1);
        func_800E8634(D_neo_ark_observatory_801811E0.data.commands, 0, D_neo_ark_observatory_801812C0);
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

    task  = gameGetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = gameGetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xD7) != 0) {
        D_neo_ark_observatory_80181368.vy = 0;
    } else {
        D_neo_ark_observatory_80181368.vy = 0x2710;
    }
    func_neo_ark_observatory_8017FE34(task->extra.tmd->coords, &D_neo_ark_observatory_80181368);
}

void func_neo_ark_observatory_8017FB1C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x300, 0);
            Gp_SpawnIfCapIdle(task->spawnArg1.value, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
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

s32 func_neo_ark_observatory_8017FBE0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room event-script handler: mirrors the incoming message onto the outgoing
/// one and lets `func_map_neo_ark_80179B14` act on both. Once the observatory has been
/// reached from both routes (nibbles 0xD1 == 3 and 0x4C == 9) and the script
/// raises one of the two arrival ids with no sub-state pending, nibble 0x4C is
/// cleared and the room's area records are applied.
s32 func_neo_ark_observatory_8017FBE8(Task* arg0, s32 arg1, RoomEventMsg * in, RoomEventMsg * out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    if ((GameFlag_GetNibble(0xD1) == 3) && (GameFlag_GetNibble(0x4C) == 9) &&
        ((in->prefix.packed == 0xA) || (in->prefix.packed == 0x13)) && (in->field_5 == 0)) {
        GameFlag_SetNibble(0x4C, 0);
        Gp_ApplyAreaRecs(D_neo_ark_observatory_80187A28);
    }
    return 1;
}

s32 func_neo_ark_observatory_8017FCA0(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    if (arg2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_neo_ark_observatory_801811AC, 0, 1, 0);
    }
    return 0;
}

/// Room entry task tick: installs the room's message table, hands the task to
/// slot 7, then advances state.
static void func_neo_ark_observatory_8017FCE0(Task* arg0)
{
    arg0->msgTable = D_neo_ark_observatory_801811B8;
    Game_SetPtrSlot(arg0, 7);
    if ((gameGetPtrSlot(0xA) != NULL) && (gGameSession->at4.loc.place == 1)) {
        func_801322F8();
    } else {
        func_neo_ark_observatory_8017FA98(0);
    }
    if (GameFlag_GetNibble(0xE1) != 0) {
        func_neo_ark_observatory_80180DAC(0xA0);
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
        if (Mc_SaveData[0].state.at4.loc.view != 2) {
            Gp_MsgAlly3F3(2);
            return;
        }
    }
    var_a0 = 1;
    if (Mc_SaveData[0].state.at4.loc.view == 3) {
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

/// Rebuilds the working mesh from its source under `coord`: the first four
/// vectors are rotated only, the eight after them rotated and translated and,
/// when `offset` is non-NULL, shifted by it afterwards.
static void func_neo_ark_observatory_8017FE34(GpCoord* coord, SVECTOR* offset)
{
    MATRIX        m;
    long          flag;
    s32           i;
    SVECTOR*      d;
    SVECTOR*      s;
    GpGridParams* dst = &D_neo_ark_observatory_80181FA4;
    GpGridParams* src = &D_neo_ark_observatory_80181410;

    for (i = 0; i < 4; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }

    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    m = coord->coord;

    d = dst->field_4;
    s = src->field_4;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    if (offset != NULL) {
        for (i = 0; i < 8; i++) {
            RotTransSV(s, d, &flag);
            s++;
            d->vx += offset->vx;
            d->vy += offset->vy;
            d->vz += offset->vz;
            d++;
        }
    } else {
        for (i = 0; i < 8; i++) {
            RotTransSV(s++, d++, &flag);
        }
    }
}

void func_neo_ark_observatory_80180124(Task* task)
{
    u8 view;

    if (task->state == 0) {
        D_neo_ark_observatory_80187A3C = 0;
        task->state                    = 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2:
            func_neo_ark_observatory_80180A0C(&D_neo_ark_observatory_801814E4[0], 0x280, 0x444);
            break;
        case 3: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814F4;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x280, 0x444);
            break;
        }
        case 4:
        case 16: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814FC;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x280, 0x333);
            func_neo_ark_observatory_80180A0C(&p[3], 0x280, 0x222);
            break;
        }
        case 5:
        case 17: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018150C;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x280, 0x444);
            break;
        }
        case 6:
        case 18:
            func_neo_ark_observatory_80180A0C(&D_neo_ark_observatory_8018151C[0], 0x200, 0x444);
            break;
        case 7: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181434;
            func_neo_ark_observatory_80180534(&p[0], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[2], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[4], 0x400, D_neo_ark_observatory_80187A3C, 8);
        }
            /* fallthrough */
        case 19: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018151C;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[6], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[7], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[8], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[9], 0x200, 0x444);
            break;
        }
        case 8:
        case 20: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-32], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-30], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 9: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181574;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[3], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[-6], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[-8], 0x200, 0x333);
            func_neo_ark_observatory_80180534(&p[-28], 0x400, D_neo_ark_observatory_80187A3C, 8);
            func_neo_ark_observatory_80180534(&p[-26], 0x400, D_neo_ark_observatory_80187A3C, 0xC);
            func_neo_ark_observatory_80180534(&p[-24], 0x400, D_neo_ark_observatory_80187A3C, 8);
            break;
        }
        case 10: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181524;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[5], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[6], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[7], 0x200, 0x222);
            func_neo_ark_observatory_80180534(&p[-12], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 11: {
            SVECTOR* p;
            p = D_neo_ark_observatory_8018157C;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x222);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x333);
            func_neo_ark_observatory_80180A0C(&p[2], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[-7], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[-8], 0x200, 0x444);
            func_neo_ark_observatory_80180534(&p[-21], 0x600, D_neo_ark_observatory_80187A3C, 0x10);
            break;
        }
        case 12:
        case 14: {
            SVECTOR* p;
            p = D_neo_ark_observatory_801814E4;
            func_neo_ark_observatory_80180A0C(&p[0], 0x280, 0x444);
            func_neo_ark_observatory_80180A0C(&p[2], 0x280, 0x444);
            break;
        }
        case 21: {
            SVECTOR* p;
            p = D_neo_ark_observatory_80181564;
            func_neo_ark_observatory_80180A0C(&p[0], 0x200, 0x444);
            func_neo_ark_observatory_80180A0C(&p[1], 0x200, 0x444);
            break;
        }
    }
}

/// Draws a rotating ring of gouraud `POLY_G4` segments between two circles in
/// the XZ plane: an inner circle of radius `(s16)arg1 / 2` around `v[0]` and an
/// outer one of radius `(s16)arg1` around `v[1]`. `arg3` segments cover the
/// full turn, starting at a phase that advances with the frame counter. The
/// inner edge is lit at `arg2` plus a small pulse, fading to half at the far
/// corner and to black on the outer edge; nothing is drawn while that level
/// is negative.
static void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s16 arg3)
{
    RoomQuadProjScratch* blk;
    POLY_G4*             prim;
    SVECTOR*             outer;
    DisplayState*        ds;
    s16                  start;
    s16                  step;
    s32                  angle;
    s32                  next;
    s16                  innerRadius;
    s16                  level;

    step        = 0x1000 / arg3;
    outer       = v + 1;
    innerRadius = (s16)arg1 >> 1;
    start       = gDisplayState.animFrame & 0xFFF;
    level       = arg2 + (rsin(gDisplayState.animFrame << 10) >> 10);
    if (level >= 0) {
        SCRATCH_PUSH(RoomQuadProjScratch);
        blk = SCRATCH_HEAD(RoomQuadProjScratch);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        for (angle = start; angle < start + step * arg3; angle = next) {
            blk->v[0].vx = v->vx + ((rsin(angle) * innerRadius) >> 12);
            blk->v[0].vy = v->vy;
            blk->v[0].vz = v->vz + ((rcos(angle) * innerRadius) >> 12);
            next         = angle + step;
            blk->v[1].vx = v->vx + ((rsin(next) * innerRadius) >> 12);
            blk->v[1].vy = v->vy;
            blk->v[1].vz = v->vz + ((rcos(next) * innerRadius) >> 12);
            blk->v[2].vx = outer->vx + ((rsin(angle) * (s16)arg1) >> 12);
            blk->v[2].vy = outer->vy;
            blk->v[2].vz = outer->vz + ((rcos(angle) * (s16)arg1) >> 12);
            blk->v[3].vx = outer->vx + ((rsin(next) * (s16)arg1) >> 12);
            blk->v[3].vy = outer->vy;
            blk->v[3].vz = outer->vz + ((rcos(next) * (s16)arg1) >> 12);
            gte_SetRotMatrix(&gGfxViewCoord.workm);
            gte_ldv0(&blk->v[0]);
            gte_rtps();
            gte_stsxy(&blk->sxy[0]);
            gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
            gte_rtpt();
            gte_stsxy3(&blk->sxy[1], &blk->sxy[2], &blk->sxy[3]);
            gte_stflg(&blk->flag);
            if (blk->flag >= 0) {
                gte_stszotz(&blk->otz);
                blk->otz++;
                ds             = &gDisplayState;
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = (u8*)(prim + 1);
                setPolyG4(prim);
                setRGB0(prim, level, level, level);
                setRGB1(prim, level >> 1, level >> 1, level >> 1);
                setRGB2(prim, 0, 0, 0);
                setRGB3(prim, 0, 0, 0);
                addPrim(Gpu_OtEntryAtByteOffset((((u32)(blk->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                        prim);
                prim->x0 = blk->sxy[0].vx;
                prim->y0 = blk->sxy[0].vy;
                prim->x1 = blk->sxy[1].vx;
                prim->y1 = blk->sxy[1].vy;
                prim->x2 = blk->sxy[2].vx;
                prim->y2 = blk->sxy[2].vy;
                prim->x3 = blk->sxy[3].vx;
                prim->y3 = blk->sxy[3].vy;
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
        SCRATCH_POP(RoomQuadProjScratch);
    }
}

/// Draws a glow around the world point `arg0`: projects it through
/// `gGfxViewCoord.workm` and, when the GTE flag is non-negative, queues four
/// gouraud `POLY_G4` quads that together make a disc around the projected
/// centre, lit at the centre and black at the rim. The on-screen radius is
/// `(s16)arg1 * 64 / otz`. `arg2` packs the centre colour as three RGB
/// nibbles, each OR'd with a flicker bit taken from the frame counter.
static void func_neo_ark_observatory_80180A0C(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw13Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    s32                blend;
    s32                tr;
    s32                tg;
    u8                 r;
    u8                 g;
    u8                 b;

    block = SCRATCH_PUSH(RoomDraw13Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1          = ((s16)arg1 * 64) / block->otz;
        ang           = 0;
        blend         = ((u8)gDisplayState.animFrame & 1) * 8;
        packed        = arg2 << 16;
        tr            = (packed >> 20) & 0xF0;
        tg            = (packed >> 16) & 0xF0;
        r             = blend | tr;
        g             = blend | tg;
        b             = blend | ((arg2 & 0xF) << 4);
        block->radius = arg1;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x200;
            prim->y0 = block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw13Scratch);
}

void func_neo_ark_observatory_80180DAC(s32 arg0)
{
    D_neo_ark_observatory_80187A3C = arg0;
}
