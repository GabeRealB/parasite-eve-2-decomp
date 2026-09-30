#include "rooms/acropolis_cafeteria.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "acropolis_cafeteria_private.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/area_flags.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/inventory.h"
#include "gameplay/items.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

/// 0xD8 work block the falling-debris task keeps at `Task::work`
/// (`memCalloc(0xD8)` in `func_acropolis_cafeteria_801818DC`, released by
/// `func_acropolis_cafeteria_80181E3C` through `Gp_UnlinkObj`).
///
/// It opens with the `GpObj` list node linked onto `Gp_ObjLists[4]`, whose
/// `field_C` points at the six `WorldCollisionContact` slots that follow it in the same
/// block. `field_B0` is the spawn-time random seed / countdown
/// (`(rand() & 0xFFF) + 0x3000`, decremented every frame);
/// `field_B4` / `field_B8` / `field_BC` are the per-axis velocities added into
/// the object's coordinate; `field_C4` is the rotation handed to `RotMatrix`
/// and `field_CC` the normalised surface direction from `Gfx_MatrixCol2` /
/// `VectorNormalSS`; `field_D4` is the task's own sub-state.
typedef struct AcropolisCafeteriaDebris {
    /* 0x00 */ GpObj                 obj;
    /* 0x20 */ WorldCollisionContact slots[6];
    /* 0xB0 */ s32                   field_B0;
    /* 0xB4 */ s32                   field_B4;
    /* 0xB8 */ s32                   field_B8;
    /* 0xBC */ s32                   field_BC;
    /* 0xC0 */ byte                  pad_C0[4];
    /* 0xC4 */ SVECTOR               field_C4;
    /* 0xCC */ SVECTOR               field_CC;
    /* 0xD4 */ u16                   field_D4;
    /* 0xD6 */ byte                  pad_D6[2];
} AcropolisCafeteriaDebris;
STATIC_ASSERT_SIZEOF(AcropolisCafeteriaDebris, 0xD8);

extern MATRIX  D_acropolis_cafeteria_8018D5A0;
extern MATRIX  D_acropolis_cafeteria_8018D5C0;
extern MATRIX  D_acropolis_cafeteria_8018D5E0;
extern MATRIX  D_acropolis_cafeteria_8018D600;
extern MATRIX  D_acropolis_cafeteria_8018D620;
extern MATRIX  D_acropolis_cafeteria_8018D640;
extern MATRIX  D_acropolis_cafeteria_8018D660;
extern MATRIX  D_acropolis_cafeteria_8018D680;
extern SVECTOR D_acropolis_cafeteria_8018D6AC;

static void func_acropolis_cafeteria_8017FBEC(GfxCoord* coord, s32 arg1, s32 arg2, u8* rgb);
static void func_acropolis_cafeteria_80180018(GfxCoord* coord, s16 arg1, u8* rgb);
static void func_acropolis_cafeteria_8018089C(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_acropolis_cafeteria_80180F1C(GfxCoord* coord, s16 arg1, u8* rgb);
static void func_acropolis_cafeteria_80181E3C(Task* arg0);

extern u32     D_acropolis_cafeteria_8018D278[1];
extern SVECTOR D_acropolis_cafeteria_8018D27C[22];
extern SVECTOR D_acropolis_cafeteria_8018D32C[18];
extern TmdBone D_acropolis_cafeteria_8018D254[1];
extern u32     D_acropolis_cafeteria_8018D3BC[112];

extern GpDrawAreaRec D_acropolis_cafeteria_8018B3A4[2];
extern SpriteBatch   D_acropolis_cafeteria_8018AA30[2];
extern SpriteBatch   D_acropolis_cafeteria_8018AD60[10];
extern SpriteBatch   D_acropolis_cafeteria_8018B2EC[19];
extern SpriteBatch   D_acropolis_cafeteria_8018B394[2];
extern SpriteBatch   D_acropolis_cafeteria_8018B6C4[6];
extern SpriteBatch   D_acropolis_cafeteria_8018B6F4[2];
extern SpriteBatch   D_acropolis_cafeteria_8018B704[2];
extern SpriteBatch   D_acropolis_cafeteria_8018B714[2];
extern SpriteBatch   D_acropolis_cafeteria_8018B724[2];
extern SpriteBatch   D_acropolis_cafeteria_8018B7C0[3];
extern SpriteBatch   D_acropolis_cafeteria_8018B800[3];
extern SpriteBatch   D_acropolis_cafeteria_8018B818[2];
extern SpriteBatch   D_acropolis_cafeteria_8018BA94[17];
extern SpriteBatch   D_acropolis_cafeteria_8018BC0C[3];
extern SpriteBatch   D_acropolis_cafeteria_8018BC24[2];
extern SpriteBatch   D_acropolis_cafeteria_8018BC34[2];
extern SpriteBatch   D_acropolis_cafeteria_8018BC44[2];
extern SpriteBatch   D_acropolis_cafeteria_8018BC54[2];
extern SpriteBatch   D_acropolis_cafeteria_8018BC64[2];
extern SpriteBatch   D_acropolis_cafeteria_8018C250[19];
extern GpSprtElem    D_acropolis_cafeteria_8018AA40[40];
extern GpSprtElem    D_acropolis_cafeteria_8018ADB0[67];
extern GpSprtElem    D_acropolis_cafeteria_8018B3B8[39];
extern GpSprtElem    D_acropolis_cafeteria_8018B734[7];
extern GpSprtElem    D_acropolis_cafeteria_8018B7D8[2];
extern GpSprtElem    D_acropolis_cafeteria_8018B828[31];
extern GpSprtElem    D_acropolis_cafeteria_8018BB1C[12];
extern GpSprtElem    D_acropolis_cafeteria_8018BC74[75];

GpRoomCoordSet D_acropolis_cafeteria_8018AA18[1] = {
    { 0, NULL, 15, D_acropolis_cafeteria_80189E24, 1, D_acropolis_cafeteria_8018A3C4.active },
};

SpriteBatch D_acropolis_cafeteria_8018AA30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018AA40[40] = {
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 72, 48, 751, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 72, 56, 709, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 72, 64, 677, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 80, 80, 627, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, 96, 659, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, 40, 874, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 56, 871, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 56, 867, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 56, 867, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 48, 770, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 56, 696, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 136, 56, 770, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 72, 638, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 128, 72, 864, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 72, 700, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 80, 697, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 80, 699, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 88, 672, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 96, 649, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 88, 650, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 48, 96, 664, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 48, 104, 728, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 88, 660, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 104, 104, 616, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -8, 1350, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -32, 1275, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 88, -32, 1225, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -24, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, -16, 1050, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 136, -8, 975, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, -32, 925, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, -24, 925, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 152, -8, 900, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, -24, 1000, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, -32, 1075, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 88, -40, 1175, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, -48, 1175, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 112, -104, 1000, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -120, 975, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, -120, 950, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018AD60[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 4, 0, 0, { 1, 0 } },
    { 9, 5, 0, 0, { 4, 0 } },
    { 14, 8, 0, 0, { 0, 0 } },
    { 22, 2, 0, 0, { 5, 0 } },
    { 24, 0, 0, 0, { 3, 0 } },
    { 24, 13, 0, 0, { 7, 0 } },
    { 37, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018ADB0[67] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, -24, 1444, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 1443, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, 0, 1460, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -16, 1620, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -16, 1601, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -16, 1365, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, -16, 1324, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, -8, 1319, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 8, 1370, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -88, 16, 875, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 16, 1038, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -72, 24, 1052, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 873, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1035, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -88, 40, 888, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 1052, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 56, 902, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 72, 906, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -104, 32, 925, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 32, 894, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 56, 926, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 120, 48, 804, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 56, 688, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 112, 80, 810, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 96, 780, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 80, 40, 809, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 80, 48, 808, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 80, 64, 667, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, 80, 862, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 80, 88, 647, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, 104, 601, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 80, 678, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 88, 662, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 96, 622, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 96, 589, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 80, 112, 668, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -56, 1850, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -48, 1875, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -32, 1900, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -48, 1775, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -40, 1825, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 48, -40, 1725, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -40, 1375, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -32, 1775, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, -32, 1625, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 88, -32, 1375, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 1675, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 72, -24, 1425, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -16, 1450, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 88, -16, 1250, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -8, 1325, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 8, 1100, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1125, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -8, 1250, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 0, 1125, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 0, 1200, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -8, 1200, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -96, 1700, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -96, 1525, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -104, 1450, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -104, 1325, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, -104, 1200, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -104, 1150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -56, 1150, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -16, 1100, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, -16, 1125, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 8, 1150, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B2EC[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 6, 0 } },
    { 0, 3, 0, 0, { 9, 0 } },
    { 3, 2, 0, 0, { 2, 0 } },
    { 5, 1, 0, 0, { 10, 0 } },
    { 6, 1, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 11, 0 } },
    { 9, 1, 0, 0, { 1, 0 } },
    { 10, 8, 0, 0, { 12, 0 } },
    { 18, 1, 0, 0, { 8, 0 } },
    { 19, 2, 0, 0, { 13, 0 } },
    { 21, 4, 0, 0, { 4, 0 } },
    { 25, 6, 0, 0, { 14, 0 } },
    { 31, 5, 0, 0, { 3, 0 } },
    { 36, 0, 0, 0, { 15, 0 } },
    { 36, 0, 0, 0, { 7, 0 } },
    { 36, 21, 0, 0, { 16, 0 } },
    { 57, 10, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B384[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B394[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_acropolis_cafeteria_8018B3A4[2] = {
    { { 72, 0, 248, 239 }, 707 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_acropolis_cafeteria_8018B3B8[39] = {
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -160, -120, 525, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -96, -120, 525, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, -32, -120, 525, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, 32, -120, 525, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 64 } }, 96, -120, 525, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, -56, 925, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -56, 950, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, 64, -8, 925, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 80, 0, 925, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 24, 0, 875, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, -8, 875, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, -16, 875, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -24, 0, 850, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -64, 0, 825, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -96, -8, 800, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 16 } }, -128, 24, 800, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 8, 800, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, 8, 775, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 40, 825, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 80, 850, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 40 } }, -128, 40, 850, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, -128, 80, 875, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -72, 40, 875, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -72, 72, 900, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -24, 32, 900, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -24, 64, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, 24, 24, 925, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 32 } }, 24, 56, 950, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 80, 16, 975, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 80, 48, 1000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, -8, 950, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 32, 1025, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 88, 634, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 0, 88, 250, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, 96, 225, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 96, 225, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, 112, 200, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 104, 225, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 8, 112, 200, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B6C4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 32, 0, 0, { 3, 0 } },
    { 32, 4, 0, 0, { 0, 0 } },
    { 36, 1, 0, 0, { 2, 0 } },
    { 37, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B6F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B704[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B714[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B724[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018B734[7] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 8, 64, 525, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, 72, 492, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 32, 72, 636, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -24, 80, 499, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 16 } }, 32, 80, 515, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, -32, 96, 506, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, 40, 96, 250, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B7C0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018B7D8[2] = {
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -48, 0, 675, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 0, 675, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018B800[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018B818[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018B828[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 40, 1100, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, -16, 975, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 48, 1000, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, -16, 975, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -128, -16, 1000, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 0, 1025, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -112, 8, 1037, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 40, 1025, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 32, 1050, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 32, 1075, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 56, 787, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, 40, 825, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 32, 1163, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 8, 950, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 16, 925, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 32, 937, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 0, 950, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 40, 925, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -80, 16, 900, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -72, 8, 925, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, 40, 962, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 8, 975, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 16, 950, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -96, 0, 1350, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -104, -40, 1325, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, -112, -72, 1300, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 144 } }, -120, -112, 1275, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 72 } }, -136, -120, 1125, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 72 } }, -160, -120, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -160, -48, 1100, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 56 } }, -136, -48, 1125, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018BA94[17] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 8, 0 } },
    { 10, 2, 0, 0, { 3, 0 } },
    { 12, 4, 0, 0, { 12, 0 } },
    { 16, 4, 0, 0, { 2, 0 } },
    { 20, 3, 0, 0, { 10, 0 } },
    { 23, 0, 0, 0, { 1, 0 } },
    { 23, 0, 0, 0, { 13, 0 } },
    { 23, 0, 0, 0, { 4, 0 } },
    { 23, 0, 0, 0, { 9, 0 } },
    { 23, 0, 0, 0, { 7, 0 } },
    { 23, 0, 0, 0, { 14, 0 } },
    { 23, 0, 0, 0, { 6, 0 } },
    { 23, 0, 0, 0, { 11, 0 } },
    { 23, 0, 0, 0, { 5, 0 } },
    { 23, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018BB1C[12] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, -120, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -160, -40, 750, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, 32, 750, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -120, -120, 775, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, -40, 775, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -120, 32, 775, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -88, -120, 800, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -88, -40, 800, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 64 } }, -88, 32, 800, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -56, -120, 825, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, -56, -40, 825, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 56 } }, -56, 32, 825, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018BC0C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC54[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018BC64[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018BC74[75] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, -24, 1444, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -8, 1443, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 0, 0, 1460, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -16, 1620, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -16, 1601, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -16, 1365, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, -16, 1324, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, -8, 1319, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 8, 1370, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -88, 16, 875, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 16, 1038, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -72, 24, 1052, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 32, 873, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 32, 1035, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -88, 40, 888, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 1052, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 56, 902, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 72, 906, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -104, 32, 925, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 56, 724, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 64, 40, 1019, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 40, 896, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 56, 688, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, 48, 724, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 120, 48, 804, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 56, 688, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 112, 80, 810, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 120, 96, 780, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 80, 40, 809, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 80, 48, 808, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, 80, 64, 667, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, 80, 862, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 80, 88, 647, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, 104, 601, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 80, 678, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, 88, 662, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 88, 96, 622, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, 96, 589, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 80, 112, 668, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -32, 1900, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, -56, 1850, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -48, 1875, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -48, 1775, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, -40, 1825, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -40, 1375, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 88, -32, 1375, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -32, 1775, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, 8, 1100, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 128, 8, 1125, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 128, 0, 1125, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, 0, 1200, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -8, 1200, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, -8, 1250, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -8, 1325, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -24, 1675, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -16, 1450, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, -40, 1725, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, -40, 1725, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -32, 1625, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, -32, 1625, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 112, -16, 1186, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1250, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 104, -24, 1196, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, -24, 1425, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, -24, 1375, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -96, 1700, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -96, 1525, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -104, 1450, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -104, 1325, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, -104, 1200, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, -104, 1150, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -56, 1150, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -16, 1100, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 144, -16, 1125, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 136, 8, 1150, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018C250[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 6, 0 } },
    { 0, 3, 0, 0, { 9, 0 } },
    { 3, 2, 0, 0, { 2, 0 } },
    { 5, 1, 0, 0, { 10, 0 } },
    { 6, 1, 0, 0, { 0, 0 } },
    { 7, 2, 0, 0, { 11, 0 } },
    { 9, 1, 0, 0, { 1, 0 } },
    { 10, 8, 0, 0, { 12, 0 } },
    { 18, 1, 0, 0, { 8, 0 } },
    { 19, 5, 0, 0, { 13, 0 } },
    { 24, 4, 0, 0, { 4, 0 } },
    { 28, 6, 0, 0, { 14, 0 } },
    { 34, 5, 0, 0, { 3, 0 } },
    { 39, 0, 0, 0, { 15, 0 } },
    { 39, 0, 0, 0, { 7, 0 } },
    { 39, 26, 0, 0, { 16, 0 } },
    { 65, 10, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_cafeteria_8018C2E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018C2F8[16] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -160, -120, 525, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -120, -120, 475, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -96, -120, 425, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -64, -120, 400, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -120, 375, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, -120, 350, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -120, 325, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 48, -120, 300, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 80, -120, 276, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -160, 48, 300, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, -144, 56, 287, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -88, 64, 275, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -40, 72, 275, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 8, 80, 275, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 56, 88, 237, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, 104, 96, 225, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018C438[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 7, 0, 0, { 2, 0 } },
    { 16, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_acropolis_cafeteria_8018C460[1] = {
    { 143, 0x3FC0, { .fields = { 88, 24 } }, -80, 96, 672, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_cafeteria_8018C474[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_acropolis_cafeteria_8018C48C[24] = {
    { { .empty = D_acropolis_cafeteria_8018AA30 }, D_acropolis_cafeteria_8018AA30, NULL },
    { { .elements = D_acropolis_cafeteria_8018AA40 }, D_acropolis_cafeteria_8018AD60, NULL },
    { { .elements = D_acropolis_cafeteria_8018ADB0 }, D_acropolis_cafeteria_8018B2EC, NULL },
    { { .empty = D_acropolis_cafeteria_8018AA30 }, D_acropolis_cafeteria_8018AA30, NULL },
    { { .empty = D_acropolis_cafeteria_8018B394 }, D_acropolis_cafeteria_8018B394, D_acropolis_cafeteria_8018B3A4 },
    { { .elements = D_acropolis_cafeteria_8018B3B8 }, D_acropolis_cafeteria_8018B6C4, NULL },
    { { .empty = D_acropolis_cafeteria_8018B6F4 }, D_acropolis_cafeteria_8018B6F4, NULL },
    { { .empty = D_acropolis_cafeteria_8018B704 }, D_acropolis_cafeteria_8018B704, NULL },
    { { .empty = D_acropolis_cafeteria_8018B714 }, D_acropolis_cafeteria_8018B714, NULL },
    { { .empty = D_acropolis_cafeteria_8018B724 }, D_acropolis_cafeteria_8018B724, NULL },
    { { .elements = D_acropolis_cafeteria_8018B734 }, D_acropolis_cafeteria_8018B7C0, NULL },
    { { .elements = D_acropolis_cafeteria_8018B7D8 }, D_acropolis_cafeteria_8018B800, NULL },
    { { .empty = D_acropolis_cafeteria_8018B818 }, D_acropolis_cafeteria_8018B818, NULL },
    { { .elements = D_acropolis_cafeteria_8018B828 }, D_acropolis_cafeteria_8018BA94, NULL },
    { { .elements = D_acropolis_cafeteria_8018BB1C }, D_acropolis_cafeteria_8018BC0C, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC24 }, D_acropolis_cafeteria_8018BC24, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC34 }, D_acropolis_cafeteria_8018BC34, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC44 }, D_acropolis_cafeteria_8018BC44, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC54 }, D_acropolis_cafeteria_8018BC54, NULL },
    { { .empty = D_acropolis_cafeteria_8018BC64 }, D_acropolis_cafeteria_8018BC64, NULL },
    { { .elements = D_acropolis_cafeteria_8018BC74 }, D_acropolis_cafeteria_8018C250, NULL },
    { { .elements = D_acropolis_cafeteria_8018B828 }, D_acropolis_cafeteria_8018BA94, NULL },
    { { .elements = D_acropolis_cafeteria_8018C2F8 }, D_acropolis_cafeteria_8018C438, NULL },
    { { .elements = D_acropolis_cafeteria_8018C460 }, D_acropolis_cafeteria_8018C474, NULL },
};

GpViewRec D_acropolis_cafeteria_8018C5AC[24] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1000, 0x59D8, 1500 } }, 380 },
    { { { { 3792, 0, -1548 }, { -816, 3480, -1998 }, { 1315, 2158, 3222 } }, { 4570, 3240, 520 } }, 207 },
    { { { { 3878, 0, -1317 }, { -530, 3748, -1562 }, { 1205, 1650, 3549 } }, { 4720, 2950, 4230 } }, 207 },
    { { { { -3772, 0, -1594 }, { -753, 3610, 1782 }, { 1405, 1934, -3325 } }, { 4640, 3150, 20 } }, 207 },
    { { { { -1722, 0, -3715 }, { -621, 4038, 287 }, { 3663, 684, -1698 } }, { 3510, 2090, 3480 } }, 207 },
    { { { { -953, 0, -3983 }, { -1242, 3891, 297 }, { 3784, 1278, -905 } }, { 3870, 2600, -1340 } }, 257 },
    { { { { -3525, 0, -2084 }, { -966, 3629, 1634 }, { 1846, 1898, -3124 } }, { 150, 2740, -1420 } }, 207 },
    { { { { 3548, 0, -2045 }, { 441, 3999, 765 }, { 1997, -883, 3464 } }, { 150, 1000, 2080 } }, 207 },
    { { { { 3963, 0, -1031 }, { -276, 3945, -1063 }, { 993, 1098, 3818 } }, { -1180, 1880, -1890 } }, 257 },
    { { { { -1719, 0, -3717 }, { -972, 3953, 449 }, { 3588, 1071, -1660 } }, { -2450, 1700, 1840 } }, 207 },
    { { { { 3850, 0, -1396 }, { -577, 3728, -1593 }, { 1271, 1694, 3505 } }, { 3160, 2330, 3700 } }, 289 },
    { { { { -3566, 0, -2014 }, { 400, 4014, -709 }, { 1974, -814, -3494 } }, { 3790, 610, 520 } }, 207 },
    { { { { -3940, 0, -1116 }, { -1024, 1627, 3616 }, { 443, 3758, -1566 } }, { 10, 1940, 910 } }, 289 },
    { { { { -3772, 0, -1594 }, { -753, 3610, 1782 }, { 1405, 1934, -3325 } }, { 4640, 3150, 20 } }, 207 },
    { { { { -1722, 0, -3715 }, { -621, 4038, 287 }, { 3663, 684, -1698 } }, { 3510, 2090, 3480 } }, 207 },
    { { { { -3831, 0, -1448 }, { -1361, 1402, 3599 }, { 495, 3848, -1311 } }, { 3820, 2840, 960 } }, 207 },
    { { { { 630, 0, -4047 }, { -3556, 1955, -553 }, { 1932, 3598, 300 } }, { 4220, 3110, 1600 } }, 230 },
    { { { { -4088, 0, -245 }, { 44, 4029, -735 }, { 241, -736, -4022 } }, { 3790, 1260, -590 } }, 329 },
    { { { { 4094, 0, -102 }, { 1, 4095, 45 }, { 102, -45, 4094 } }, { 3470, 1610, 840 } }, 447 },
    { { { { 3987, 0, -935 }, { 234, 3965, 997 }, { 905, -1024, 3860 } }, { 3900, 600, 3260 } }, 257 },
    { { { { 3878, 0, -1317 }, { -530, 3748, -1562 }, { 1205, 1650, 3549 } }, { 4720, 2950, 4230 } }, 207 },
    { { { { -3772, 0, -1594 }, { -753, 3610, 1782 }, { 1405, 1934, -3325 } }, { 4640, 3150, 20 } }, 207 },
    { { { { 2289, 0, -3396 }, { 208, 4088, 140 }, { 3390, -251, 2285 } }, { 1520, 1730, -80 } }, 257 },
    { { { { -3916, 0, -1197 }, { 691, 3344, -2261 }, { 977, -2365, -3197 } }, { 3690, 380, -80 } }, 257 },
};

GpRoomBoundVec D_acropolis_cafeteria_8018C90C[25] = {
    { 24, 0, 0, 0 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 16, 16, 16, 16 },
    { 500, 1000, 1831, 916 },
    { 500, 1000, 1831, 916 },
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

GpAreaApplyRec D_acropolis_cafeteria_8018C9D4[3] = {
    { 1, 3, 2, 0 },
    { 1, 9, 2, 1 },
    { 255, 0, 0, 0 },
};

s32 D_acropolis_cafeteria_8018C9E0[3] = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

s32 D_acropolis_cafeteria_8018C9EC[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

s32 D_acropolis_cafeteria_8018C9F8[3] = {
    0x10000001,
    0x10000003,
    0x10000005,
};

GpRoomParamRec D_acropolis_cafeteria_8018CA04[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_acropolis_cafeteria_8018CA0C[1] = {
    { 0, 0, 1, 0, D_acropolis_cafeteria_8018C9E0 },
};

GpRoomParamRec D_acropolis_cafeteria_8018CA14[1] = {
    { 0, 0, 1, 0, D_acropolis_cafeteria_8018C9EC },
};

GpRoomParamRec D_acropolis_cafeteria_8018CA1C[1] = {
    { 0, 0, 1, 0, D_acropolis_cafeteria_8018C9F8 },
};

GpRoomParamRec D_acropolis_cafeteria_8018CA24[1] = {
    { 0, 1, 0, 0, D_acropolis_cafeteria_8018C9F8 },
};

GpRoomParamRec* D_acropolis_cafeteria_8018CA2C[8] = {
    D_acropolis_cafeteria_8018CA04,
    D_acropolis_cafeteria_8018CA0C,
    D_acropolis_cafeteria_8018CA14,
    D_acropolis_cafeteria_8018CA1C,
    D_acropolis_cafeteria_8018CA24,
    D_acropolis_cafeteria_8018CA04,
    D_acropolis_cafeteria_8018CA04,
    D_acropolis_cafeteria_8018CA04,
};

TmdBone D_acropolis_cafeteria_8018CA4C[1] = {
#include "assets/acropolis_cafeteria_model_0FC70_skeleton.inc"
};

u32 D_acropolis_cafeteria_8018CA70[1] = {
#include "assets/acropolis_cafeteria_model_0FC70_partVerts.inc"
};

SVECTOR D_acropolis_cafeteria_8018CA74[41] = {
#include "assets/acropolis_cafeteria_model_0FC70_verts.inc"
};

SVECTOR D_acropolis_cafeteria_8018CBBC[53] = {
#include "assets/acropolis_cafeteria_model_0FC70_normals.inc"
};

u32 D_acropolis_cafeteria_8018CD64[307] = {
#include "assets/acropolis_cafeteria_model_0FC70_stream.inc"
};

TmdSource D_acropolis_cafeteria_8018D230 = {
    0,
    2168,
    0,
    1,
    D_acropolis_cafeteria_8018CA70,
    D_acropolis_cafeteria_8018CA74,
    D_acropolis_cafeteria_8018CBBC,
    D_acropolis_cafeteria_8018CA4C,
    D_acropolis_cafeteria_8018CD64,
};

TmdBone D_acropolis_cafeteria_8018D254[1] = {
#include "assets/acropolis_cafeteria_model_0FFBC_skeleton.inc"
};

u32 D_acropolis_cafeteria_8018D278[1] = {
#include "assets/acropolis_cafeteria_model_0FFBC_partVerts.inc"
};

SVECTOR D_acropolis_cafeteria_8018D27C[22] = {
#include "assets/acropolis_cafeteria_model_0FFBC_verts.inc"
};

SVECTOR D_acropolis_cafeteria_8018D32C[18] = {
#include "assets/acropolis_cafeteria_model_0FFBC_normals.inc"
};

u32 D_acropolis_cafeteria_8018D3BC[112] = {
#include "assets/acropolis_cafeteria_model_0FFBC_stream.inc"
};

TmdSource D_acropolis_cafeteria_8018D57C = {
    0,
    756,
    0,
    1,
    D_acropolis_cafeteria_8018D278,
    D_acropolis_cafeteria_8018D27C,
    D_acropolis_cafeteria_8018D32C,
    D_acropolis_cafeteria_8018D254,
    D_acropolis_cafeteria_8018D3BC,
};

MATRIX D_acropolis_cafeteria_8018D5A0 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D5C0 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D5E0 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 4096, 4096, 4096 } };

MATRIX D_acropolis_cafeteria_8018D600 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D620 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 4096, 4096, 4096 } };

MATRIX D_acropolis_cafeteria_8018D640 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

MATRIX D_acropolis_cafeteria_8018D660 = { { { 2048, 2048, 2048 }, { 2048, 2048, 2048 }, { 2048, 2048, 2048 } }, { 4096, 4096, 4096 } };

MATRIX D_acropolis_cafeteria_8018D680 = { { { 128, 128, 128 }, { -2048, -2048, -2048 }, { -2048, -2048, -2048 } }, { 2048, 2048, 2048 } };

s32 D_acropolis_cafeteria_8018D6A0 = 0;

s32 D_acropolis_cafeteria_8018D6A4 = 0;

s32 D_acropolis_cafeteria_8018D6A8 = 0;

SVECTOR D_acropolis_cafeteria_8018D6AC = { 0 };

static void func_acropolis_cafeteria_801818DC(Task* task);
static void func_acropolis_cafeteria_80181A3C(Task* task);
static void func_acropolis_cafeteria_80181E30(Task* arg0);
static s32  func_acropolis_cafeteria_80181ED4(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2);
static s32  func_acropolis_cafeteria_80182078(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);
static void func_acropolis_cafeteria_80182954(Task* task);
static void func_acropolis_cafeteria_80182A08(Task* task);

void func_acropolis_cafeteria_8017E47C(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot((u8*)&key, 0, 0);
    CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    task->killCountdown   = 0;
    task->spawnArg1.value = 0;
    task->state           = task->state + 1;
    return;

L_case3:
    if (++task->killCountdown == 0x443) {
        Stage_RequestFromAreaTable(0);
        task->spawnArg1.value = 1;
    }
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    if (task->spawnArg1.value == 0) {
        Stage_RequestFromAreaTable(0);
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

/// Fades the screen to white, four steps of the kill countdown per frame, and
/// kills the task once the countdown reaches 0x100.
void func_acropolis_cafeteria_8017E658(Task* arg0)
{
    u16 temp_v0;

    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

void func_acropolis_cafeteria_8017E6B8(Task* arg0)
{
    Display_SpawnWithOt(D_acropolis_cafeteria_80184178, 2, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_acropolis_cafeteria_8017E708(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;

    work  = (GpEffWork*)task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (task->state != 0) {
        return;
    }
    task->msgTable = D_acropolis_cafeteria_80184CEC;
    Game_SetPtrSlot(task, 5);
    vec                            = &work->move;
    D_acropolis_cafeteria_80184CFC = 0;
    work->move.vx                  = 0x220;
    work->move.vy                  = -0x12C;
    work->move.vz                  = -0x6A0;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->move.vx = 0x400;
    work->move.vy = -0x12C;
    work->move.vz = -0x260;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    work->move.vx = 0x370;
    work->move.vy = -0x12C;
    work->move.vz = -0x860;
    Gp_SpawnEff(0x60064, coord, 0, vec);
    task->state   = task->state + 1;
    work->move.vx = 0xBB8;
    work->move.vy = -0x834;
    work->move.vz = -0x7D0;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    work->move.vx = 0xB22;
    work->move.vy = -0x834;
    work->move.vz = -0x900;
    Gp_SpawnEff(0x60064, coord, 1, vec);
    D_80115758 = 0x6028D;
    D_8011572C = 0x6028E;
    D_80115750 = 0x6028F;
}
/// Spawns 40 effects on entry to session mode 9, then two per tick while it
/// remains active. Releases the work block when the room effect gate clears.
void func_acropolis_cafeteria_8017E89C(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        i;
    u16        count;
    s32        flags;
    s32        spawnArg;
    u8         mode;
    u16        rnd;

    work  = (GpEffWork*)task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (D_acropolis_cafeteria_80184CFC == 0) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    mode = gGameSession->at4.loc.view;
    if (mode == 9) {
        count = 0x28;
        if (work->scale != mode) {
            flags = 0x1000;
        } else {
            count = 2;
            flags = 0;
        }
        for (i = 0; i < count; i++) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vx = (u32)rnd % 2620 + 0x230;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vy = -0x12C - (u16)((u32)rnd % 5) * 0x190;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->move.vz = (rnd & 0x3FF) + 0xB00;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            spawnArg      = flags + 0x180;
            Gp_SpawnEff(0x60061, coord, (rnd & 0xFF) + spawnArg, &work->move);
        }
    }
    work->scale = gGameSession->at4.loc.view;
}

/// While the room's effect gate is set and the session view is mode 9, draws
/// the effect as a semi-transparent billboard animated through a 5-column
/// sheet of 48-pixel cells, one cell every `step` frames. The first frame it
/// projects in front of the camera seeds a random spin, drift and frame
/// period; spawn flag `0x1000` starts it ten frames in. Each frame it drifts
/// along Z until Z reaches `0xB00` and along Y after that. The effect is
/// released once it has shown all ten cells, or as soon as the gate or the
/// view mode no longer hold.
void func_acropolis_cafeteria_8017EA90(Task* task)
{
    GpEffWork*            work;
    GfxCoord*             coord;
    OverlaySpriteScratch* head;
    OverlaySpriteScratch* block;
    POLY_FT4*             prim;
    u8                    mode;
    u8                    shade;
    s32                   quot;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (D_acropolis_cafeteria_80184CFC != 0) {
        mode = gGameSession->at4.loc.view;
        if (mode == 9) {
            Gp_UpdateCoord(coord);
            head = SCRATCH_HEAD(OverlaySpriteScratch);
            SCRATCH_PUSH(OverlaySpriteScratch);
            block         = SCRATCH_HEAD(OverlaySpriteScratch);
            block->vec.vx = coord->workm.t[0];
            block->vec.vy = coord->workm.t[1];
            block->vec.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&head[-1].vec);
            gte_rtps();
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setcode(prim, 0x2C);
            setlen(prim, mode);
            gte_stsxy(&head[-1].sxy);
            gte_stszotz(&block->otz);
            if (head[-1].otz > 16 && work->age == 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
                work->angle   = task->spawnArg1.halves.low & 0xFFF;
                work->move.vx = 0;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vy = (((u32)Gp_LcgState >> 16) & 0xF) + 4;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->move.vz = -(((u32)Gp_LcgState >> 16) & 0xF) - 4;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                work->step    = (((u32)Gp_LcgState >> 16) & 3) + 3;
                if (task->spawnArg1.value & 0x1000) {
                    work->age = 10;
                }
            }
            if (work->age < 10) {
                shade = work->age * 4;
                setRGB0(prim, shade, shade, shade);
            } else {
                shade = 40;
                setRGB0(prim, shade, shade, shade);
            }
            prim->tpage = 0x2B;
            prim->clut  = 0x4380;
            setSemiTrans(prim, 1);
            quot      = work->age / work->step;
            prim->u0  = (quot % 5) * 48;
            quot      = work->age / work->step;
            prim->v0  = (quot / 5) * 48;
            quot      = work->age / work->step;
            prim->u1  = (quot % 5) * 48 + 47;
            quot      = work->age / work->step;
            prim->v1  = (quot / 5) * 48;
            quot      = work->age / work->step;
            prim->u2  = (quot % 5) * 48;
            quot      = work->age / work->step;
            prim->v2  = (quot / 5) * 48 + 47;
            quot      = work->age / work->step;
            prim->u3  = (quot % 5) * 48 + 47;
            quot      = work->age / work->step;
            prim->v3  = (quot / 5) * 48 + 47;
            block->dx = (((work->angle * 47) / block->otz) * rsin(work->scale)) >> 12;
            block->dy = (((work->angle * 47) / block->otz) * rcos(work->scale)) >> 12;
            prim->x0  = (u16)block->sxy.vx + (u16)block->dx;
            prim->x3  = (u16)block->sxy.vx - (u16)block->dx;
            prim->y0  = (u16)block->sxy.vy - (u16)block->dy;
            prim->y3  = (u16)block->sxy.vy + (u16)block->dy;
            block->dx = (((work->angle * 47) / block->otz) * rsin(work->scale + 0x400)) >> 12;
            block->dy = (((work->angle * 47) / block->otz) * rcos(work->scale + 0x400)) >> 12;
            prim->x1  = (u16)block->sxy.vx + (u16)block->dx;
            prim->x2  = (u16)block->sxy.vx - (u16)block->dx;
            prim->y1  = (u16)block->sxy.vy - (u16)block->dy;
            prim->y2  = (u16)block->sxy.vy + (u16)block->dy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            SCRATCH_POP_BYTES(0x18);
            if (coord->coord.t[2] > 0xB00) {
                coord->coord.t[2] += work->move.vz;
            } else {
                coord->coord.t[1] += work->move.vy;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->age++;
            if (work->age <= work->step * 10 - 1) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_acropolis_cafeteria_8017F390(Task* task)
{
    TmdObject*  obj;
    GpEffWork*  work;
    GfxCoord*   coord;
    GpMtxWords* rot;
    s16         state;
    s32         v;
    s32         w;
    s32         n;
    s32         k; // one variable for both branches' LCG addend; literal constants allocate differently
    s32         pan;

    obj   = task->extra.tmd;
    work  = (GpEffWork*)task->spawnArg2.pointer;
    state = Gp_State1C->eventState;
    coord = obj->coords;
    if (state >= 4) {
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    if (state != 0) {
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    if (task->state == 0) {
        obj->flags &= ~TMD_OBJECT_HIDDEN;
        if (task->spawnArg1.value != 0) {
            work->period = 0xD90;
            work->angle  = 0;
            work->index  = 2;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->scale  = ((u32)Gp_LcgState >> 16) & 0xF00;
        } else {
            work->scale  = 0x400;
            work->angle  = 0;
            work->period = 0xB00;
        }
        Gfx_RotMatrixY(&coord->coord, work->scale, 0);
        task->state++;
        return;
    }
    switch (work->index) {
        case 0:
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->angle  = 0;
            work->scale -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 30) == 0) {
                work->index = 1;
            }
            if (work->age >= 0x79) {
                work->index = 4;
            }
            break;
        case 1:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                v = work->scale;
                if (v > 0x400) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    n           = v - 0x10;
                    n          -= ((u32)Gp_LcgState >> 16) & 0x3F;
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    n           = v + 0x10;
                    n          += ((u32)Gp_LcgState >> 16) & 0x3F;
                }
                work->scale = n;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = 0x200;
            if ((u16)(((u32)Gp_LcgState >> 16) % 30) == 0) {
                work->index = 0;
            }
            if (work->age >= 0x79) {
                work->index = 4;
            }
            break;
        case 2:
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            work->scale -= (((u32)Gp_LcgState >> 16) & 0xFF) - 0x80;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 240) == 0) {
                work->scale = 0x400;
                work->angle = 0x200;
                work->index = 3;
            }
            break;
        case 3:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 7) == 0) {
                work->angle = 0;
                work->index = 2;
            }
            break;
        case 4:
            w = work->scale;
            if (w > 0x400) {
                k           = 0x71357911;
                Gp_LcgState = Gp_LcgState * 5 + k;
                w          -= 0x10;
                w          -= ((u32)Gp_LcgState >> 16) & 0x3F;
            } else {
                k           = 0x71357911;
                Gp_LcgState = Gp_LcgState * 5 + k;
                w          += 0x10;
                w          += ((u32)Gp_LcgState >> 16) & 0x3F;
            }
            work->scale = w;
            work->angle = 0x300;
            if ((Gp_GetViewIndex() & 0xFF) == 7 && work->step == 0) {
                pan = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(0x51040006, pan, (s8)gpGetObjDepth(coord));
                work->step = 1;
            }
            break;
    }
    rot          = (GpMtxWords*)&coord->coord;
    rot->m00_m01 = 0x1000;
    rot->m02_m10 = 0;
    rot->m11_m12 = 0x1000;
    rot->m20_m21 = 0;
    rot->m22     = 0x1000;
    Gfx_RotMatrixY(&coord->coord, work->scale, 0);
    gte_ReadMatrixColumn(&coord->coord, 2, &work->move);
    work->move.vx       = (work->move.vx * work->angle) >> 16;
    work->move.vy       = (work->move.vy * work->angle) >> 16;
    work->move.vz       = (work->move.vz * work->angle) >> 16;
    coord->coord.t[0]  += work->move.vx;
    coord->coord.t[1]  += work->move.vy;
    coord->coord.t[2]  += work->move.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->period < coord->coord.t[0]) {
        Gp_ReleaseState1CMem(work, task);
    }
}

s32 func_acropolis_cafeteria_8017F908(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    GfxCoord* coord;

    coord                          = task->extra.tmd->coords;
    D_acropolis_cafeteria_80184CFC = arg2;
    if (arg2 != 0) {
        Gp_SpawnEff(0x6009D, coord, 0, NULL);
    }
    return 0;
}

/// Flash effect on the object's coordinate. Over `spawnArg1` frames it
/// brightens and widens two wedge bursts and a ring, all tinted with red at
/// full, blue at half and green at quarter intensity; it then lays a fade quad
/// and shrinks a billboard glow by sixteen shades a frame until it is gone.
/// Paused while the room event state is non-zero, released once it reaches 4.
void func_acropolis_cafeteria_8017F948(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1.value;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_acropolis_cafeteria_80180018(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_acropolis_cafeteria_80180018(coord, (s16)((u16)work->angle * 2), rgb);
                func_acropolis_cafeteria_8017FBEC(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_acropolis_cafeteria_80180F1C(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags an error, queues a ring of sixteen gouraud `POLY_G4` quads
/// around the projected point. `arg1` is the ring's inner radius and
/// `arg1 + arg2` its outer one, both in world units scaled by depth. The inner
/// edge takes `rgb` and the outer edge is black.
static void func_acropolis_cafeteria_8017FBEC(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s16                blackRadius = arg1;
    s16                tintRadius  = arg1 + arg2;

    block         = SCRATCH_PUSH(RoomDraw02Scratch);
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
        block->rOuter = (blackRadius * 64) / block->otz;
        block->rInner = (tintRadius * 64) / block->otz;

        ang = 0;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, rgb[0], rgb[1], rgb[2]);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomDraw02Scratch);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags an error, queues eight gouraud `POLY_G4` wedges that fill a
/// disc around the projected point. `arg1` is the radius in world units scaled
/// by depth; each wedge is `rgb` at the centre and black at the rim.
static void func_acropolis_cafeteria_80180018(GfxCoord* arg0, s16 arg1, u8* rgb)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

void func_acropolis_cafeteria_801803AC(Task* task)
{
    GfxCoord   coord;
    GfxCoord*  coords;
    GfxCoord*  objCoord;
    GfxCoord*  dst;
    GpEffWork* work;
    SVECTOR*   vec;
    s32        i;

    coords   = task->work;
    work     = (GpEffWork*)task->spawnArg2.pointer;
    objCoord = task->extra.coordBody->coord;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = memCalloc(sizeof(GfxCoord[16]), 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work             = coords;
                objCoord->parent       = work->parent;
                objCoord->coord.t[0]   = D_acropolis_cafeteria_80184E80[0].vx;
                objCoord->coord.t[1]   = D_acropolis_cafeteria_80184E80[0].vy;
                objCoord->coord.t[2]   = D_acropolis_cafeteria_80184E80[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_acropolis_cafeteria_80184E80[1];
                coord.coord.t[0]   = vec->vx;
                coord.coord.t[1]   = vec->vy;
                coord.coord.t[2]   = vec->vz;
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst         = &coords[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &coords[i + 8];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                coord.parent = work->parent;
                {
                    SVECTOR* edge    = &D_acropolis_cafeteria_80184E80[1];
                    coord.coord.t[0] = edge->vx;
                    coord.coord.t[1] = edge->vy;
                    coord.coord.t[2] = edge->vz;
                }
                coord.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&coord);
                dst         = &coords[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &coords[(work->age & 7) + 8];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &coords[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &coords[i + 8];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                func_acropolis_cafeteria_8018089C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the beam between two eight-slot coordinate trails as seven gouraud
/// `POLY_G4` quads, walking back from slot `arg2`. Each quad joins two adjacent
/// slots of `arg0` and `arg1`, and fades with age: the newer edge is scaled by
/// `0x40 - 9 * i` and the older one by nine less. `arg3` packs the beam colour
/// as 2-bit multipliers for red, green and blue at bits 8, 4 and 0. A quad the
/// GTE flags as invalid is skipped.
static void func_acropolis_cafeteria_8018089C(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GfxCoord*          a;
    GfxCoord*          b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    blk = SCRATCH_PUSH(RoomDraw03Scratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP(RoomDraw03Scratch);
}

void func_acropolis_cafeteria_80180C94(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1.value != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_acropolis_cafeteria_8017FBEC(objCoord, 0x100, 0x100, rgb);
            func_acropolis_cafeteria_8017FBEC(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags an error, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges around the projected point: sixteen around the full circle, half of
/// them at full radius in half-intensity `rgb` and half at half radius in full
/// `rgb`, then four spikes a quarter turn apart, two reaching the full radius
/// and two twice it. `arg1` sizes it in world units scaled by depth; every
/// wedge fades to black at its rim.
static void func_acropolis_cafeteria_80180F1C(GfxCoord* arg0, s16 arg1, u8* arg2)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP(RoomBillboardScratch);
}

static void func_acropolis_cafeteria_801818DC(Task* task)
{
    TmdObject*                obj;
    GfxCoord*                 coord;
    AcropolisCafeteriaDebris* work;
    GfxCoord*                 player;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0xD8, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work         = work;
    task->exitCallback = func_acropolis_cafeteria_80181E3C;
    task->state        = task->state + 1;
    Mem_Set(work, 0, 0xD8);
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->flags          = 0;
    RotMatrix(&work->field_C4, &coord->coord);
    work->field_B0     = (rand() & 0xFFF) + 0x3000;
    player             = gameGetPtrSlot(3)->extra.tmd->coords;
    coord->coord.t[0]  = player->coord.t[0];
    coord->coord.t[1]  = player->coord.t[1] - 0x800;
    coord->coord.t[2]  = player->coord.t[2] + 0x800;
    work->obj.ctx.recs = work->slots;
    work->obj.key      = 0x50000;
    work->obj.radius   = 0xFA;
    work->obj.coord    = coord;
    work->obj.pos.vx   = 0;
    work->obj.pos.vy   = 0;
    work->obj.pos.vz   = 0;
    work->obj.flags    = 1;
    Gp_LinkObj(4, &work->obj);
    Gp_InitRec18Table(work->obj.ctx.recs, 6, 0);
    work->obj.flags |= 0x8000;
}

static void func_acropolis_cafeteria_80181A3C(Task* task)
{
    MATRIX*                   head;
    AcropolisCafeteriaDebris* work;
    GfxCoord*                 coord;
    SVECTOR*                  direction;
    s32                       speed;

    head                 = SCRATCH_HEAD(MATRIX);
    SCRATCH_HEAD(MATRIX) = head - 1;
    work                 = (AcropolisCafeteriaDebris*)task->work;
    coord                = task->extra.tmd->coords;
    work->field_B0--;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]  += 0x80;
    Gp_UpdateCoord(coord);
    switch (work->field_D4) {
        case 0:
            if (Gp_FindRec18(work->obj.ctx.recs, 0)) {
                work->field_D4++;
                head[-1]  = coord->coord;
                direction = &work->field_CC;
                Gfx_MatrixCol2(Player_Status.coordMtx, direction);
                VectorNormalSS(direction, direction);
                rand();
                speed          = work->field_B0;
                speed        >>= 1;
                speed          = (speed * speed) >> 6;
                work->field_B8 = -0x100;
                work->field_B4 = (work->field_CC.vx * speed) >> 24;
                work->field_BC = (work->field_CC.vz * speed) >> 24;
            }
            break;
        case 1:
            work->field_B8 += 0x10;
            if (work->field_B8 > 0) {
                work->field_B8 = 0;
                work->field_D4++;
            } else {
                work->field_C4.vx += (work->field_B0 >> 6) + (rand() & 0x7F);
                work->field_C4.vy += (work->field_B0 >> 6) + (rand() & 0x7F);
                work->field_C4.vz += (work->field_B0 >> 6) + (rand() & 0x7F);
            }
        case 2:
            work->field_B4 = (work->field_B4 * 6) / 7;
            if (ABS(work->field_B4) < 9) {
                work->field_B4 = 0;
            }
            work->field_BC = (work->field_BC * 6) / 7;
            if (ABS(work->field_BC) < 9) {
                work->field_BC = 0;
            }
            if ((work->field_B4 | work->field_BC) == 0) {
                work->field_D4++;
            }
            coord->coord.t[0] += work->field_B4;
            coord->coord.t[1] += work->field_B8;
            coord->coord.t[2] += work->field_BC;
            break;
        case 3:
            work->field_C4.vx = (work->field_C4.vx * 2) / 3;
            if (ABS(work->field_C4.vx) < 9) {
                work->field_C4.vx = 0;
            }
            work->field_C4.vz = (work->field_C4.vz * 2) / 3;
            if (ABS(work->field_C4.vz) < 9) {
                work->field_C4.vz = 0;
            }
            if (((u16)work->field_C4.vx | (u16)work->field_C4.vz) == 0) {
                work->field_D4 = 0;
            }
            break;
    }
    RotMatrix(&work->field_C4, &coord->coord);
    Gp_ClearRec18Occupied(work->slots);
    SCRATCH_POP(MATRIX);
}

static void func_acropolis_cafeteria_80181E30(Task* arg0)
{
    arg0->state = 3;
}

static void func_acropolis_cafeteria_80181E3C(Task* arg0)
{
    Gp_UnlinkObj(arg0->work);
    taskKill(arg0);
}

/// State handlers of the falling-debris task: set-up, the per-frame update, a
/// step that moves the task to state 3, and the exit that unlinks and kills it.
static const TaskFuncTable4 D_acropolis_cafeteria_8017D69C = { {
    func_acropolis_cafeteria_801818DC,
    func_acropolis_cafeteria_80181A3C,
    func_acropolis_cafeteria_80181E30,
    func_acropolis_cafeteria_80181E3C,
} };

/// Runs the task's current state through a stack copy of the room's
/// four-entry state table.
void func_acropolis_cafeteria_80181E70(Task* task)
{
    TaskFuncTable4 states;

    states = D_acropolis_cafeteria_8017D69C;
    states.funcs[task->state](task);
}

/// Gets a 16.16 X/Y/Z displacement for `rec` from `func_800E0C10` and, when it
/// reports one, adds its X and Z to the coordinate's translation, rounding a
/// fractional part away from zero. The whole-unit displacement is also left in
/// `D_acropolis_cafeteria_8018D6AC`. Returns non-zero when the X or Z
/// displacement is non-zero.
static s32 func_acropolis_cafeteria_80181ED4(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2)
{
    OverlayDeltaFlag* s;
    s32               val;

    s        = SCRATCH_PUSH(OverlayDeltaFlag);
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                += s->delta.vx.w >> 16;
        coord->coord.t[2]                += s->delta.vz.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vx = s->delta.vx.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vy = s->delta.vy.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vz = s->delta.vz.w >> 16;
        val                               = s->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_cafeteria_8018D6AC.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_cafeteria_8018D6AC.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_cafeteria_8018D6AC.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_cafeteria_8018D6AC.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP(OverlayDeltaFlag);
    return s->moved;
}

/// Measures the bearing of each type-1 or type-3 record in `recs` (up to
/// `count`, or the first zero key) from the coordinate's world position,
/// relative to the direction it faces. For a record that has every other such
/// record within a quarter turn of it, moves the coordinate `push` units back
/// along that record's bearing, in X and Z. Returns non-zero if it moved the
/// coordinate; returns 0 at once while `gGameSession->viewReady` is 1.
static s32 func_acropolis_cafeteria_80182078(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push)
{
    OverlayBisectorScratch* st;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    SCRATCH_PUSH(OverlayBisectorScratch);
    st         = SCRATCH_HEAD(OverlayBisectorScratch);
    st->eye.vx = (u16)coord->coord.t[0];
    st->eye.vy = (u16)coord->coord.t[1];
    st->eye.vz = (u16)coord->coord.t[2];

    overlayToWorld(coord->parent, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key.value == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key.value & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->angle[st->i] = ratan2(st->delta.vx, st->delta.vz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)st->aim.vz - (u16)st->eye.vz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, st->delta.vz);

            st->angle[st->i] = overlayWrapAngle(st->angle[st->i]);
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                st->diff = overlayWrapAngle(st->diff);
                if (abs(st->diff) > 0x400) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    hit = st->hit;
    SCRATCH_POP(OverlayBisectorScratch);
    return hit;
}

void func_acropolis_cafeteria_801827C4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;

    obj = (GpItemObj8*)task->spawnArg2.pointer;
    tmd = task->extra.tmd;
    if (Gp_GetCurBit2Flag(obj->field_8) != 2) {
        tmd->lightMtx = &D_acropolis_cafeteria_8018D5C0;
        tmd->colorMtx = &D_acropolis_cafeteria_8018D5A0;
        tmd->flags    = 0;
    } else {
        tmd->flags |= TMD_OBJECT_HIDDEN;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 0xC:
            tmd->otOffset = 7;
            break;
        case 0x18:
            tmd->otOffset = 4;
            break;
        default:
            tmd->otOffset = -2;
            break;
    }
}
void func_acropolis_cafeteria_8018286C(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2.pointer;
    tmd  = task->extra.tmd;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->flags = TMD_OBJECT_HIDDEN;
        return;
    }
    if (obj->field_8 == 0xA) {
        Gfx_RotMatrixX(&task->extra.tmd->coords->coord, 0x400, 1);
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D600;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D5E0;
    if (flag == 2) {
        tmd->flags &= (u16)~TMD_OBJECT_FLAGGED_PASS;
        Task_CallExit(task);
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
static void func_acropolis_cafeteria_80182954(Task* task)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->flags = TMD_OBJECT_HIDDEN;
        return;
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D640;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D620;
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        tmd->flags |= TMD_OBJECT_HIDDEN;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, 0x400, 1);
}
static void func_acropolis_cafeteria_80182A08(Task* task)
{
    TmdObject* tmd;

    tmd = task->extra.tmd;
    switch (Gp_GetViewIndex() & 0xFF) {
        case 6:
        case 7:
        case 0xA:
            tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
            tmd->lightMtx = &D_acropolis_cafeteria_8018D680;
            tmd->colorMtx = &D_acropolis_cafeteria_8018D660;
            break;
        default:
            tmd->flags |= TMD_OBJECT_HIDDEN;
            return;
    }
    if (Gp_GetCurBit2Flag(0xB) == 2) {
        tmd->flags |= TMD_OBJECT_HIDDEN;
    } else {
        tmd->flags    = TMD_OBJECT_FLAGGED_PASS;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
