#include "types.h"

#include "main/task_types.h"
#include "../../shared/glow_draw.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
Task* gRoomCutsceneSoundTask;

/// The cutscene task `func_dryfield_gas_station_801807E0` publishes once its
/// `DgsWork` block is set up, so the room's script helpers can reach it.
Task* D_dryfield_gas_station_80184BD4;

#include "rooms/dryfield_gas_station.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_gas_station_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/screen_fade.h"

#define D_dryfield_gas_station_80182E5C (D_dryfield_gas_station_80182E44[1])
#define D_dryfield_gas_station_80182E74 (D_dryfield_gas_station_80182E44[2])

/// Work block for the gas-station cutscene task, allocated as 0x10 zeroed bytes
/// by `func_dryfield_gas_station_801807E0` and hung off `Task::work` (0x1C).
///
/// `owner` is the slot-3 game pointer (`gameGetPtrSlot(3)`) the task dispatches
/// its messages to, and `playerEffActive` is the flag guarding
/// `Gp_KillPlayerEffs` / `Gp_SpawnWeaponEff`. `field_4` is the script command
/// `func_dryfield_gas_station_801803C0` carries out and clears once it is done,
/// `field_6` the step within a multi-frame command (both written together by
/// `func_dryfield_gas_station_80180B2C`), and `field_8` the frame counter of the
/// command that walks the owner across the forecourt.
typedef struct DgsWork {
    /* 0x00 */ void* owner;
    /* 0x04 */ u16   field_4;
    /* 0x06 */ u16   field_6;
    /* 0x08 */ u16   field_8;
    /* 0x0A */ byte  pad_A[0x2];
    /* 0x0C */ u16   playerEffActive;
    /* 0x0E */ byte  pad_E[0x2];
} DgsWork;
STATIC_ASSERT_SIZEOF(DgsWork, 0x10);

extern AnimationSet* D_dryfield_gas_station_80182E30[5];
extern GpEvsCmd      D_dryfield_gas_station_80182E8C[];
extern GpEvsCmd      D_dryfield_gas_station_8018303C[];

extern SVECTOR D_dryfield_gas_station_80183144;

// Indexed views below share one contiguous table.
void func_dryfield_gas_station_80180944(void);
void func_dryfield_gas_station_80180B2C(s16);

extern GpGridParams   D_dryfield_gas_station_80183EA4[1];
extern GpObj4C        D_dryfield_gas_station_80184350[11];
extern GpObj4C        D_dryfield_gas_station_80184694[10];
extern GpRoomCoordSet D_dryfield_gas_station_80184B48[1];
extern TaskDesc       D_80142604;
extern TaskDesc       D_8014D8A4;
void                  func_dryfield_gas_station_801807E0(Task*);
void                  func_dryfield_gas_station_80180984(Task*);
void                  func_dryfield_gas_station_80180A60(void);

TaskDesc D_dryfield_gas_station_80181E7C[3] = {
    { 0, 192, func_dryfield_gas_station_801802C0, { .model = NULL } },
    { 0, 192, func_dryfield_gas_station_8017FFE4, { .model = NULL } },
    { 0, 192, screenFadeInTask, { .model = NULL } },
};

AnimationPackedPose D_dryfield_gas_station_80181EA0[2] = {
#include "assets/dryfield_gas_station_animation_04A70_bank1.inc"
};

AnimationPackedRotation D_dryfield_gas_station_80181EB8[8] = {
#include "assets/dryfield_gas_station_animation_04A70_bank4.inc"
};

AnimationRecord D_dryfield_gas_station_80181ED8[76] = {
#include "assets/dryfield_gas_station_animation_04A70_records.inc"
};

u16 D_dryfield_gas_station_80182008[20] = {
#include "assets/dryfield_gas_station_animation_04A70_indices.inc"
};

AnimationSet D_dryfield_gas_station_80182030 = {
    D_dryfield_gas_station_80181ED8,
    D_dryfield_gas_station_80182008,
    { NULL, D_dryfield_gas_station_80181EA0, NULL, NULL, D_dryfield_gas_station_80181EB8, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_gas_station_80182058[13] = {
#include "assets/dryfield_gas_station_animation_05210_bank1.inc"
};

AnimationPackedRotation D_dryfield_gas_station_801820F4[179] = {
#include "assets/dryfield_gas_station_animation_05210_bank4.inc"
};

AnimationRecord D_dryfield_gas_station_801823C0[250] = {
#include "assets/dryfield_gas_station_animation_05210_records.inc"
};

u16 D_dryfield_gas_station_801827A8[20] = {
#include "assets/dryfield_gas_station_animation_05210_indices.inc"
};

AnimationSet D_dryfield_gas_station_801827D0 = {
    D_dryfield_gas_station_801823C0,
    D_dryfield_gas_station_801827A8,
    { NULL, D_dryfield_gas_station_80182058, NULL, NULL, D_dryfield_gas_station_801820F4, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_gas_station_801827F8[7] = {
#include "assets/dryfield_gas_station_animation_055A4_bank1.inc"
};

AnimationPackedRotation D_dryfield_gas_station_8018284C[65] = {
#include "assets/dryfield_gas_station_animation_055A4_bank4.inc"
};

AnimationRecord D_dryfield_gas_station_80182950[123] = {
#include "assets/dryfield_gas_station_animation_055A4_records.inc"
};

u16 D_dryfield_gas_station_80182B3C[20] = {
#include "assets/dryfield_gas_station_animation_055A4_indices.inc"
};

AnimationSet D_dryfield_gas_station_80182B64 = {
    D_dryfield_gas_station_80182950,
    D_dryfield_gas_station_80182B3C,
    { NULL, D_dryfield_gas_station_801827F8, NULL, NULL, D_dryfield_gas_station_8018284C, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_gas_station_80182B8C[2] = {
#include "assets/dryfield_gas_station_animation_05848_bank1.inc"
};

AnimationPackedRotation D_dryfield_gas_station_80182BA4[48] = {
#include "assets/dryfield_gas_station_animation_05848_bank4.inc"
};

AnimationRecord D_dryfield_gas_station_80182C64[95] = {
#include "assets/dryfield_gas_station_animation_05848_records.inc"
};

u16 D_dryfield_gas_station_80182DE0[20] = {
#include "assets/dryfield_gas_station_animation_05848_indices.inc"
};

AnimationSet D_dryfield_gas_station_80182E08 = {
    D_dryfield_gas_station_80182C64,
    D_dryfield_gas_station_80182DE0,
    { NULL, D_dryfield_gas_station_80182B8C, NULL, NULL, D_dryfield_gas_station_80182BA4, NULL, NULL, NULL },
};

AnimationSet* D_dryfield_gas_station_80182E30[5] = {
    &D_dryfield_gas_station_80182030,
    &D_dryfield_gas_station_80182B64,
    &D_dryfield_gas_station_80182E08,
    &D_dryfield_gas_station_801827D0,
    NULL,
};

ActorTransform D_dryfield_gas_station_80182E44[3] = {
    { { 14408, 0, -2630, 0 }, { 0, 3584, 0, 0 } },
    { { 14158, 0, -2380, 0 }, { 0, 2560, 0, 0 } },
    { { 14158, 0, -2380, 0 }, { 0, 3072, 0, 0 } },
};

GpEvsCmd D_dryfield_gas_station_80182E8C[18] = {
    { 1, { .value = 6 }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_gas_station_80180944 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackS16 = func_dryfield_gas_station_80180B2C }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 9, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

GpEvsCmd D_dryfield_gas_station_8018303C[10] = {
    { 24, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 3, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 13, { .callbackNoArg = func_dryfield_gas_station_80180A60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 23, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 25, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 4, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { 1, { .value = 3 }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { -1, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

TaskDesc D_dryfield_gas_station_8018312C[2] = {
    { 0, 192, func_dryfield_gas_station_801807E0, { .model = NULL } },
    { 0, 192, func_dryfield_gas_station_80180984, { .model = NULL } },
};

SVECTOR D_dryfield_gas_station_80183144 = { 4378, -1383, -215, 0 };

GpRoomObjRec D_dryfield_gas_station_8018314C[1] = {
    { D_dryfield_gas_station_80183EA4, D_dryfield_gas_station_80184350, D_dryfield_gas_station_80184694, NULL },
};

u8* D_dryfield_gas_station_8018315C[1] = {
    D_8010CAF8,
};

GpRoomCoordRec D_dryfield_gas_station_80183160[1] = {
    { D_dryfield_gas_station_80184B48, NULL },
};

GpViewCountRec D_dryfield_gas_station_80183168[1] = {
    { { .bytes = { 14, 0 } } },
};

GpWarpRec D_dryfield_gas_station_8018316C[3] = {
    { { .words = { 2816, 0x3848, 0, -2630 } }, { 0, 0, 0, 0 }, { .words = { 2816, 0x3848, 0, -1440 } }, { 0, 0, 0, 0 }, 0, 0, 0, 3, 0, 0 },
    { { .words = { 1024, 294, -5, -3482 } }, { 0, 0, 0, 0 }, { .words = { 1024, 294, -5, -4466 } }, { 0, 0, 0, 0 }, 0x52010002, 0x52010001, 0, 4, 0, 488 },
    { { .words = { 2048, 3039, 0, -433 } }, { 0, 0, 0, 0 }, { .words = { 2048, 4316, 0, -535 } }, { 0, 0, 0, 0 }, 0x52010004, 0x52010003, 0x5201000F, 6, 0, 489 },
};

SVECTOR D_dryfield_gas_station_80183214[32] = {
#include "assets/dryfield_gas_station_collision_068E4_normals.inc"
};

SVECTOR D_dryfield_gas_station_80183314[126] = {
#include "assets/dryfield_gas_station_collision_068E4_verts.inc"
};

GpGridFace D_dryfield_gas_station_80183704[63] = {
#include "assets/dryfield_gas_station_collision_068E4_faces.inc"
};

s16 D_dryfield_gas_station_801839F8[502] = {
#include "assets/dryfield_gas_station_collision_068E4_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_gas_station_801839F8[i])
s16* D_dryfield_gas_station_80183DE4[48] = {
#include "assets/dryfield_gas_station_collision_068E4_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_gas_station_80183EA4[1] = {
    { NULL, D_dryfield_gas_station_80183214, D_dryfield_gas_station_80183314, D_dryfield_gas_station_80183704, D_dryfield_gas_station_80183DE4, 5300, 0x3A98, 8, 6, 4000, 63 },
};

GpViewRec D_dryfield_gas_station_80183EC8[14] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2710, 0x7530, 1000 } }, 380 },
    { { { { 1609, 0, -3766 }, { -1268, 3856, -541 }, { 3546, 1379, 1515 } }, { -7536, 2780, 4972 } }, 230 },
    { { { { 1495, 0, -3813 }, { -23, 4095, -9 }, { 3813, 25, 1495 } }, { -1316, 1337, 5204 } }, 230 },
    { { { { 1473, 0, 3821 }, { -736, 4019, 283 }, { -3750, -788, 1445 } }, { -9489, 595, 5193 } }, 230 },
    { { { { 546, 0, -4059 }, { -3221, 2492, -433 }, { 2469, 3250, 332 } }, { -0x2EA0, 3781, 2877 } }, 329 },
    { { { { 3396, 0, 2288 }, { -180, 4083, 267 }, { -2281, -322, 3386 } }, { -5346, 792, 3101 } }, 230 },
    { { { { 1107, 0, -3943 }, { -2662, 3021, -747 }, { 2908, 2765, 816 } }, { -0x3206, 1776, 5127 } }, 230 },
    { { { { 3985, 0, 945 }, { 289, 3899, -1220 }, { -900, 1254, 3793 } }, { -4447, 1449, 747 } }, 230 },
    { { { { 1679, 0, -3735 }, { 590, 4044, 265 }, { 3689, -646, 1658 } }, { -0x2D82, 940, 3440 } }, 541 },
    { { { { 3799, 0, 1530 }, { 233, 4047, -580 }, { -1512, 626, 3754 } }, { -0x3815, 1503, 2328 } }, 230 },
    { { { { 2694, 0, 3085 }, { -516, 4038, 451 }, { -3041, -685, 2656 } }, { -0x29C2, 716, 3608 } }, 230 },
    { { { { 1200, 0, 3916 }, { -1855, 3607, 568 }, { -3448, -1940, 1057 } }, { -9280, 950, 3960 } }, 230 },
    { { { { 97, 0, -4094 }, { -3605, 1942, -85 }, { 1941, 3606, 46 } }, { -0x33FE, 930, 3500 } }, 230 },
    { { { { 3432, 0, -2234 }, { -527, 3980, -810 }, { 2171, 967, 3335 } }, { -0x2DBF, 1537, 1164 } }, 230 },
};

SpriteBatch D_dryfield_gas_station_801840C0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_gas_station_801840D0[6] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, -32, 2362, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -40, 2329, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -40, 2325, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, -40, 2325, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -40, 2325, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 40, -40, 2325, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_gas_station_80184148[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_gas_station_80184160[6] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 0, 3701, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, -8, 3300, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 0, 3424, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 0, 2250, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 0, 2250, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 0, 2342, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_gas_station_801841D8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_801841F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184208[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184218[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184228[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184238[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184248[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184258[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184268[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184278[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184288[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_gas_station_80184298[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_gas_station_801842A8[14] = {
    { { .empty = D_dryfield_gas_station_801840C0 }, D_dryfield_gas_station_801840C0, NULL },
    { { .elements = D_dryfield_gas_station_801840D0 }, D_dryfield_gas_station_80184148, NULL },
    { { .elements = D_dryfield_gas_station_80184160 }, D_dryfield_gas_station_801841D8, NULL },
    { { .empty = D_dryfield_gas_station_801841F8 }, D_dryfield_gas_station_801841F8, NULL },
    { { .empty = D_dryfield_gas_station_80184208 }, D_dryfield_gas_station_80184208, NULL },
    { { .empty = D_dryfield_gas_station_80184218 }, D_dryfield_gas_station_80184218, NULL },
    { { .empty = D_dryfield_gas_station_80184228 }, D_dryfield_gas_station_80184228, NULL },
    { { .empty = D_dryfield_gas_station_80184238 }, D_dryfield_gas_station_80184238, NULL },
    { { .empty = D_dryfield_gas_station_80184248 }, D_dryfield_gas_station_80184248, NULL },
    { { .empty = D_dryfield_gas_station_80184258 }, D_dryfield_gas_station_80184258, NULL },
    { { .empty = D_dryfield_gas_station_80184268 }, D_dryfield_gas_station_80184268, NULL },
    { { .empty = D_dryfield_gas_station_80184278 }, D_dryfield_gas_station_80184278, NULL },
    { { .empty = D_dryfield_gas_station_80184288 }, D_dryfield_gas_station_80184288, NULL },
    { { .empty = D_dryfield_gas_station_80184298 }, D_dryfield_gas_station_80184298, NULL },
};

GpObj4C D_dryfield_gas_station_80184350[11] = {
    { NULL, NULL, NULL, { 5791, -2544, -3505, 0 }, { { 147, -3568, 2869, 0 }, { -146, -3568, -2868, 0 }, { 147, 3568, 2869, 0 }, { -146, 3568, -2868, 0 } }, { -4101, 0, 209, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 5503, -2544, -3521, 0 }, { { -146, -3568, -2868, 0 }, { 147, -3568, 2869, 0 }, { -146, 3568, -2868, 0 }, { 147, 3568, 2869, 0 } }, { 4100, 0, -210, 0 }, { 0, 0, 4096, 0 }, 4579, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 4319, -2544, -1009, 0 }, { { -210, -3568, -804, 0 }, { 211, -3568, 805, 0 }, { -210, 3568, -804, 0 }, { 211, 3568, 805, 0 } }, { 3963, 0, -1038, 0 }, { 0, 0, 4096, 0 }, 3656, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 2752, -2528, -1264, 0 }, { { -1410, -3552, 524, 0 }, { 1411, -3552, -523, 0 }, { -1410, 3552, 524, 0 }, { 1411, 3552, -523, 0 } }, { -1432, 0, -3857, 0 }, { 0, 0, 4096, 0 }, 3857, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 2623, -2608, -1345, 0 }, { { 1716, -3632, -661, 0 }, { -1716, -3632, 662, 0 }, { 1716, 3632, -661, 0 }, { -1716, 3632, 662, 0 } }, { 1474, 0, 3824, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 4431, -2624, -1297, 0 }, { { 283, -3648, 1064, 0 }, { -283, -3648, -1064, 0 }, { 283, 3648, 1064, 0 }, { -283, 3648, -1064, 0 } }, { -3966, 0, 1054, 0 }, { 0, 0, 4096, 0 }, 3805, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x28FF, -2608, -1952, 0 }, { { -933, -3632, -2562, 0 }, { 911, -3632, 2537, 0 }, { -933, 3632, -2562, 0 }, { 911, 3632, 2537, 0 } }, { 3855, 0, -1395, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2A06, -2592, -1961, 0 }, { { 875, -3616, 2397, 0 }, { -875, -3616, -2396, 0 }, { 875, 3616, 2397, 0 }, { -875, 3616, -2396, 0 } }, { -3858, 0, 1407, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x361F, -2624, -2482, 0 }, { { 857, -3616, -1637, 0 }, { -856, -3616, 1638, 0 }, { 857, 3616, -1637, 0 }, { -856, 3616, 1638, 0 } }, { 3633, 0, 1900, 0 }, { 0, 0, 4096, 0 }, 4055, 0, 5, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x29DD, -2624, -5092, 0 }, { { -867, -3616, 888, 0 }, { 867, -3616, -888, 0 }, { -867, 3616, 888, 0 }, { 867, 3616, -888, 0 } }, { -2937, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x28AD, -2688, -5043, 0 }, { { 899, -3616, -888, 0 }, { -899, -3616, 888, 0 }, { 899, 3616, -888, 0 }, { -899, 3616, 888, 0 } }, { 2895, 0, 2931, 0 }, { 0, 0, 4096, 0 }, 3822, 0, 2, 3, 129, 0 },
};

GpObj4C D_dryfield_gas_station_80184694[10] = {
    { NULL, NULL, NULL, { 2657, -108, -331, 0 }, { { -1024, 0, -384, 0 }, { 1024, 0, -384, 0 }, { -1024, 0, 384, 0 }, { 1024, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1093, 0, 3, 49, 2, 0 },
    { NULL, NULL, NULL, { 192, -80, -3328, 0 }, { { 384, 0, -1440, 0 }, { 384, 0, 1024, 0 }, { -384, 0, -1440, 0 }, { -384, 0, 1024, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1487, 0, 2, 33, 2, 0 },
    { NULL, NULL, NULL, { 0x3430, -96, -224, 0 }, { { -1776, 0, -384, 0 }, { 1776, 0, -384, 0 }, { -1776, 0, 384, 0 }, { 1776, 0, 384, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1814, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x36EF, -96, -4817, 0 }, { { -1877, 0, -131, 0 }, { -397, 0, -2069, 0 }, { -1650, 0, 1270, 0 }, { 694, 0, -764, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 2095, 2, 9, 0, 4, 0 },
    { NULL, NULL, NULL, { 6960, -64, -1104, 0 }, { { -1407, 0, -624, 0 }, { 1408, 0, -624, 0 }, { -1407, 0, 624, 0 }, { 1408, 0, 624, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1536, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 4448, -96, 256, 0 }, { { -880, 0, -1408, 0 }, { 720, 0, -1408, 0 }, { -880, 0, -320, 0 }, { 720, 0, -320, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1659, 2, 1, 255, 4, 0 },
    { NULL, NULL, NULL, { 0x36C0, -64, -3488, 0 }, { { -501, 0, -973, 0 }, { 1016, 0, 403, 0 }, { -1497, 0, -20, 0 }, { 20, 0, 1356, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1492, 2, 15, 0, 4, 0 },
    { NULL, NULL, NULL, { 0x4120, -64, 0, 0 }, { { -736, 0, -624, 0 }, { 736, 0, -624, 0 }, { -736, 0, 624, 0 }, { 736, 0, 624, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 964, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 9264, -64, -1184, 0 }, { { -911, 0, -624, 0 }, { 912, 0, -624, 0 }, { -911, 0, 624, 0 }, { 912, 0, 624, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1101, 2, 26, 0, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, -528, 0 }, { { -544, 0, -576, 0 }, { 544, 0, -576, 0 }, { -544, 0, 576, 0 }, { 544, 0, 576, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 791, 2, 1, 255, 130, 0 },
};

GpAreaTmdRec D_dryfield_gas_station_8018498C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_gas_station_80184998[2] = {
    { 44, 44, 0, 0, { 0, 0 }, &D_80142604 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_gas_station_801849B0[5] = {
    { 44, 0, 0, 4226, 0, -4480, 0, 0, 0, 2, 0 },
    { 44, 0, 1, 472, -4624, -2512, 1024, 0, 0, 2, 0 },
    { 44, 0, 0, 7277, 0, -3553, 512, 0, 0, 2, 0 },
    { 44, 0, 0, 6255, 0, -5322, -1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaTmdRec D_dryfield_gas_station_80184A00[2] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_gas_station_80184A18[2] = {
    { 1, 0, 0, 6255, 0, -5322, -1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_dryfield_gas_station_80184A38[12] = {
    { NULL, NULL },
    { D_map_dryfield_8017AD34, D_dryfield_gas_station_8018498C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_gas_station_801849B0, D_dryfield_gas_station_80184998 },
    { D_dryfield_gas_station_80184A18, D_dryfield_gas_station_80184A00 },
    { NULL, NULL },
};

WorldCoordLight D_dryfield_gas_station_80184A98[2] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -4000, -2000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x2856, 8167, 7585 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2000, 2000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 1966, 2048 }, { 0, 0 } },
};

GpRoomCoordSet D_dryfield_gas_station_80184B48[1] = {
    { 2, D_dryfield_gas_station_80184A98, 0, NULL, 0, NULL },
};

s32 D_dryfield_gas_station_80184B60[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

s32 D_dryfield_gas_station_80184B6C[3] = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

s32 D_dryfield_gas_station_80184B78[3] = {
    0x10000015,
    0x10000017,
    0x10000015,
};

GpRoomParamRec D_dryfield_gas_station_80184B84[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_gas_station_80184B8C[1] = {
    { 0, 0, 1, 0, D_dryfield_gas_station_80184B60 },
};

GpRoomParamRec D_dryfield_gas_station_80184B94[1] = {
    { 0, 0, 1, 0, D_dryfield_gas_station_80184B6C },
};

GpRoomParamRec D_dryfield_gas_station_80184B9C[1] = {
    { 0, 0, 1, 0, D_dryfield_gas_station_80184B78 },
};

GpRoomParamRec D_dryfield_gas_station_80184BA4[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec* D_dryfield_gas_station_80184BAC[8] = {
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184BA4,
    D_dryfield_gas_station_80184B94,
    D_dryfield_gas_station_80184B9C,
    D_dryfield_gas_station_80184B8C,
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184B84,
    D_dryfield_gas_station_80184B84,
};

Task* D_dryfield_gas_station_80184BCC = NULL;

RoomCutsceneRec D_dryfield_gas_station_80184BD8;

static void func_dryfield_gas_station_801803C0(Task* task);
static void func_dryfield_gas_station_80181058(GfxCoord* coord, SVECTOR* data, s32 arg2, s32 arg3);

/// Carries out the script command in `DgsWork::field_4`, then clears it (the
/// multi-frame commands return early until they finish). 1 places the owner at
/// the first of three 0x3E9 placements and plays two sounds; 2 and 3 hand the
/// owner a `D_dryfield_gas_station_80182E30` script record as msg 0x3F4, 2 also
/// sending msg 0x3FD and 3 placing the owner at the second placement first.
/// 4 walks the owner from the first placement to the third over 30 frames with
/// msg 0x3FE before its closing 0x3F4; 5 is `func_dryfield_gas_station_80180A60`
/// written out again; 6 spawns entry 1 of `D_dryfield_gas_station_8018312C`,
/// waits a frame and turns the display back on.
static void func_dryfield_gas_station_801803C0(Task* task)
{
    DgsWork* work;
    DgsWork* cur;
    DgsWork* eff;
    Task*    shared;
    union {
        AnimationPlayRequest rec;
        GpMoveArg            move;
    } msg;
    AnimationPlayRequest  script;
    AnimationPlayRequest* rec;
    u16                   step;

    work = (DgsWork*)task->work;
    switch (work->field_4) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_gas_station_80182E44[0], 0);
            SndEvt_EnqueueType6(0x52010011, 0, 0);
            SndEvt_EnqueueType6(0x52010012, 0, 0);
            break;
        case 2:
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                msg.rec.animationId          = 1;
                msg.rec.blend                = ANIMATION_BLEND_RESET;
                msg.rec.blendFrames          = 0;
                msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                Gp_DispatchMsgPtr((Task*)cur->owner, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
            }
            Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
            break;
        case 3:
            SndEvt_EnqueueType6(0x52010013, 0, 0);
            Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_gas_station_80182E5C, 0);
            cur = (DgsWork*)task->work;
            if (cur->owner != NULL) {
                msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                msg.rec.animationId          = 2;
                msg.rec.blend                = ANIMATION_BLEND_INTERPOLATE;
                msg.rec.blendFrames          = 0x1E;
                msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                Gp_DispatchMsgPtr((Task*)cur->owner, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
            }
            break;
        case 4:
            step = work->field_6;
            switch (step) {
                case 0:
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                        msg.rec.animationId          = 3;
                        msg.rec.blend                = ANIMATION_BLEND_RESET;
                        msg.rec.blendFrames          = 0;
                        msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        Gp_DispatchMsgPtr((Task*)cur->owner, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
                    }
                    Gp_DispatchMsg((Task*)work->owner, 0x3FD, 8, 0);
                    Gp_DispatchMsg((Task*)work->owner, 0x3FC, 0, 0);
                    work->field_8 = 0;
                    work->field_6++;
                    return;
                case 1:
                    msg.move.x        = (D_dryfield_gas_station_80182E44[2].pos.vx - D_dryfield_gas_station_80182E44[0].pos.vx) / 30;
                    msg.move.y        = 0;
                    msg.move.z        = (D_dryfield_gas_station_80182E44[2].pos.vz - D_dryfield_gas_station_80182E44[0].pos.vz) / 30;
                    msg.move.field_10 = 0;
                    Gp_DispatchMsgPtr((Task*)work->owner, 0x3FE, &msg.move, 0);
                    work->field_8++;
                    if (work->field_8 < 31) {
                        return;
                    }
                    // Taken before the owner check, the record's address is in
                    // $a2 early enough that the two register-valued fields are
                    // stored through it; the constant ones still go off $sp.
                    rec = &script;
                    cur = (DgsWork*)task->work;
                    if (cur->owner != NULL) {
                        script.source.sets          = D_dryfield_gas_station_80182E30;
                        script.animationId          = 0;
                        rec->blend                  = step;
                        rec->blendFrames            = 0xF;
                        script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        Gp_DispatchMsgPtr((Task*)cur->owner, ANIMATION_MESSAGE_INSTALL_AND_PLAY, rec, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 5:
            shared = D_dryfield_gas_station_80184BD4;
            eff    = (DgsWork*)shared->work;
            if (eff->playerEffActive != 0) {
                Gp_SpawnWeaponEff();
                eff->playerEffActive = 0;
                Gp_MsgPlayerWeapon(0);
            }
            Gp_DispatchMsgPtr((Task*)eff->owner, 0x3E9, &D_dryfield_gas_station_80182E74, 0);
            cur = (DgsWork*)shared->work;
            if (cur->owner != NULL) {
                msg.rec.source.sets          = D_dryfield_gas_station_80182E30;
                msg.rec.animationId          = 0;
                msg.rec.blend                = ANIMATION_BLEND_RESET;
                msg.rec.blendFrames          = 0;
                msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                Gp_DispatchMsgPtr((Task*)cur->owner, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
            }
            SndEvt_EnqueueType7(0x52010011, 0x3C);
            SetDispMask(1);
            break;
        case 6:
            switch (work->field_6) {
                case 0:
                    Task_SpawnFromTable(D_dryfield_gas_station_8018312C, 1, 0x1E, 0);
                case 1:
                    work->field_6++;
                    return;
                case 2:
                    SetDispMask(1);
                    break;
            }
            break;
    }
    work->field_4 = 0;
}

/// Spawns the gas station's cutscene owner. State 0 refuses to run twice (a
/// `Gp_StateC08.field_A` of 1 and a live `gDisplayState.pendingMode` both mean the cutscene is already
/// up), otherwise it parks the freshly zeroed 0x10-byte `DgsWork` block in
/// `Task::work`, fills `owner` from pointer slot 3 and republishes this task as
/// `D_dryfield_gas_station_80184BD4` so the room's script helpers can reach that block.
/// Two kills: a failed `Mem_Malloc` kills the task outright, and state 1 kills
/// it once the session has torn down (`gGameSession->eventState`). Between the two
/// it hands slot 3 the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4 -- only when a previous state 0 already found an owner, since the
/// reloaded `work` is dereferenced unconditionally.
void func_dryfield_gas_station_801807E0(Task* task)
{
    DgsWork*             work;
    DgsWork*             work2;
    AnimationPlayRequest script;

    switch (task->state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                work       = Mem_Malloc(0x10, false);
                task->work = work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    Mem_Set(work, 0, 0x10);
                    work->owner                     = gameGetPtrSlot(3);
                    D_dryfield_gas_station_80184BD4 = task;
                }
                work2 = (DgsWork*)task->work;
                if (work2->owner != 0) {
                    script.source.sets          = D_dryfield_gas_station_80182E30;
                    script.animationId          = 0;
                    script.blend                = ANIMATION_BLEND_RESET;
                    script.blendFrames          = 0;
                    script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    Gp_DispatchMsgPtr((Task*)work2->owner, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &script, 0);
                }
                func_800E3FAC(0xA2, 9);
                func_800E8634(D_dryfield_gas_station_80182E8C, 0,
                              D_dryfield_gas_station_8018303C);
                task->state = task->state + 1;
                return;
            }
            return;

        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
                return;
            }
            func_dryfield_gas_station_801803C0(task);
            break;
    }
}

/// Latches the player-effect flag and kills the effects once. The 1 is loaded
/// before the branch and stored in the `jal` delay slot.
void func_dryfield_gas_station_80180944(void)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;
    if (work->playerEffActive == 0) {
        work->playerEffActive = 1;
        Gp_KillPlayerEffs();
    }
}

/// A second copy of the fade-in task, which this file's own task table names.
#define screenFadeInTask func_dryfield_gas_station_80180984
#include "../../shared/screen_fade_in.inc.c"
#undef screenFadeInTask

/// Tells slot 3 that the cutscene is opening: it ends the weapon effect the
/// player may still be carrying (flag at `DgsWork::playerEffActive`), echoes the
/// equipped weapon back with msg 0x3E9 and, once the cutscene task has an owner,
/// hands that owner the `D_dryfield_gas_station_80182E30` script record as msg
/// 0x3F4. The record is a `AnimationPlayRequest` built on the stack, only its first field
/// (the script pointer) set.
void func_dryfield_gas_station_80180A60(void)
{
    Task*                task;
    DgsWork*             work;
    DgsWork*             work2;
    AnimationPlayRequest script;

    task = D_dryfield_gas_station_80184BD4;
    work = (DgsWork*)task->work;
    if (work->playerEffActive != 0) {
        Gp_SpawnWeaponEff();
        work->playerEffActive = 0;
        Gp_MsgPlayerWeapon(0);
    }
    Gp_DispatchMsgPtr((Task*)work->owner, 0x3E9, &D_dryfield_gas_station_80182E74, 0);
    work2 = (DgsWork*)task->work;
    if (work2->owner != 0) {
        script.source.sets          = D_dryfield_gas_station_80182E30;
        script.animationId          = 0;
        script.blend                = ANIMATION_BLEND_RESET;
        script.blendFrames          = 0;
        script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        Gp_DispatchMsgPtr((Task*)work2->owner, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &script, 0);
    }
    SndEvt_EnqueueType7(0x52010011, 0x3C);
    SetDispMask(1);
}

/// Hands the cutscene task the script command `arg0` to carry out, starting
/// it from its first step.
void func_dryfield_gas_station_80180B2C(s16 arg0)
{
    DgsWork* work = (DgsWork*)D_dryfield_gas_station_80184BD4->work;

    work->field_4 = arg0;
    work->field_6 = 0;
}

#include "../../shared/glow_draw_star_local.inc.c"

/// Draws a pulsing cyan glow at `data` in `coord`'s space: the point is
/// projected through `GsWSMATRIX`, and nothing is drawn when its `otz` is 16 or
/// less. Around the projected centre it lays a fan of gouraud `POLY_G4`
/// wedges of radius `rOuter`, each paired with a brighter one of half that
/// radius, then four quads reaching out from `rInner` towards `rOuter`.
/// The centre vertex's intensity is `rsin(animFrame * arg2) / 34 + 0x78`,
/// halved on the outer wedges and on the four quads.
static void func_dryfield_gas_station_80181058(GfxCoord* coord, SVECTOR* data, s32 arg2, s32 arg3)
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
            setRGB2(prim, 0, half, half);
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
            setRGB2(prim, 0, color, color);
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

        color = half;
        ang   = 0x200;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, 0, color, color);
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
            setRGB2(prim, 0, color, color);
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
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}

/// Per-frame effect: draws the gas station's shaft with the task's own
/// coordinate, then enables `gRoomEffectState->roomEffectMode`.
/// `Task::extra.coordBody->coord` is the coordinate both draws share. The
/// stage-visit byte `gGameSession->location.loc.view` is used as a bit index: bits 4, 6,
/// 11 and 12 (`0x1850`) select `glowDrawStarLocal` with the wide half-extent 0x80,
/// and any other non-zero bit selects `func_dryfield_gas_station_80181058`
/// with 0x40.
void func_dryfield_gas_station_80181A78(Task* arg0)
{
    s32       mask;
    GfxCoord* coord;

    mask  = 1 << gGameSession->location.loc.view;
    coord = arg0->extra.coordBody->coord;
    if (mask & 0x1850) {
        glowDrawStarLocal(coord, &D_dryfield_gas_station_80183144, 0x60, 0x80);
    } else if (mask != 0) {
        func_dryfield_gas_station_80181058(coord, &D_dryfield_gas_station_80183144, 0x60, 0x40);
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
