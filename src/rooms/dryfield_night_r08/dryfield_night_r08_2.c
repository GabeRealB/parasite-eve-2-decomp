#include "rooms/dryfield_night_r08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
#include "../../shared/glow_draw.h"

#define D_dryfield_night_r08_801805BC (D_dryfield_night_r08_801805AC + 2)
#define D_dryfield_night_r08_801805CC (D_dryfield_night_r08_801805AC + 4)
#define D_dryfield_night_r08_801805DC (D_dryfield_night_r08_801805AC + 6)
#define D_dryfield_night_r08_80180664 (D_dryfield_night_r08_801805AC + 23)

extern SVECTOR D_dryfield_night_r08_8018056C[];

extern WorldCollisionGrid   D_dryfield_night_r08_80181474[1];
extern WorldCoordRoomLights D_dryfield_night_r08_8018189C[1];

SVECTOR D_dryfield_night_r08_8018056C[8] = {
    { -667, -1910, -0x4A3D, 0 },
    { -667, -1910, -0x45ED, 0 },
    { -667, -1910, -0x4309, 0 },
    { -667, -1910, -0x3EB4, 0 },
    { 668, -1910, -0x4A3D, 0 },
    { 668, -1910, -0x45ED, 0 },
    { 668, -1910, -0x4309, 0 },
    { 668, -1910, -0x3EB4, 0 },
};

// Indexed views below share one contiguous table.
SVECTOR D_dryfield_night_r08_801805AC[24] = {
    { -3246, -2265, -10632, 0 },
    { -3246, -2265, -9370, 0 },
    { -3246, -2265, -2620, 0 },
    { -3246, -2265, -1381, 0 },
    { 3260, -2265, -10632, 0 },
    { 3260, -2265, -9370, 0 },
    { 3260, -2265, -2620, 0 },
    { 3260, -2265, -1381, 0 },
    { -365, -2226, -180, 0 },
    { 365, -2226, -180, 0 },
    { -1293, -4768, -13478, 0 },
    { -1293, -4768, -11488, 0 },
    { -1293, -4768, -9483, 0 },
    { -1293, -4768, -7483, 0 },
    { -1293, -4768, -5483, 0 },
    { -1293, -4768, -3483, 0 },
    { -1293, -4768, -1483, 0 },
    { 1293, -4768, -13478, 0 },
    { 1293, -4768, -11488, 0 },
    { 1293, -4768, -9483, 0 },
    { 1293, -4768, -7483, 0 },
    { 1293, -4768, -5483, 0 },
    { 1293, -4768, -3483, 0 },
    { 1293, -4768, -1483, 0 },
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCoordRoomLighting D_dryfield_night_r08_8018067C[1] = {
    { D_dryfield_night_r08_8018189C, NULL },
};

GpRoomObjRec D_dryfield_night_r08_80180684[1] = {
    { D_dryfield_night_r08_80181474, NULL, NULL, NULL },
};

u8* D_dryfield_night_r08_80180694[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_night_r08_80180698[1] = {
    { { .bytes = { 9, 0 } } },
};

GpWarpRec D_dryfield_night_r08_8018069C[1] = {
    { { .words = { 0, 0, 0, -0x4650 } }, { 0, 0, 0, 0 }, { .words = { 0, 0, 0, -0x4650 } }, { 0, 0, 0, 0 }, 0, 0, 0, 1, 0, 0 },
};

SVECTOR D_dryfield_night_r08_801806D4[34] = {
#include "assets/dryfield_night_r08_collision_03EB4_normals.inc"
};

SVECTOR D_dryfield_night_r08_801807E4[172] = {
#include "assets/dryfield_night_r08_collision_03EB4_verts.inc"
};

WorldCollisionGridFace D_dryfield_night_r08_80180D44[82] = {
#include "assets/dryfield_night_r08_collision_03EB4_faces.inc"
};

s16 D_dryfield_night_r08_8018111C[392] = {
#include "assets/dryfield_night_r08_collision_03EB4_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_r08_8018111C[i])
s16* D_dryfield_night_r08_8018142C[18] = {
#include "assets/dryfield_night_r08_collision_03EB4_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_r08_80181474[1] = {
    { NULL, D_dryfield_night_r08_801806D4, D_dryfield_night_r08_801807E4, D_dryfield_night_r08_80180D44, D_dryfield_night_r08_8018142C, 5398, 0x4D03, 3, 6, 4000, 82 },
};

GpViewRec D_dryfield_night_r08_80181498[9] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5EE9, 8000 } }, 230 },
    { { { { -4086, 0, 279 }, { 19, 4085, 285 }, { -278, 286, -4076 } }, { -92, 1285, 0x2769 } }, 380 },
    { { { { 4017, 0, -798 }, { -146, 4026, -738 }, { 784, 752, 3949 } }, { 853, 1783, 0x391E } }, 148 },
    { { { { 4026, 0, -753 }, { -13, 4095, -74 }, { 753, 75, 4025 } }, { 199, 1212, 0x30DF } }, 282 },
    { { { { -4095, 0, 2 }, { 0, 4095, 4 }, { -2, 4, -4095 } }, { 4, 1285, 8562 } }, 251 },
    { { { { 4071, 0, -448 }, { 66, 4050, 606 }, { 443, -609, 4026 } }, { 1561, 288, 0x38BC } }, 257 },
    { { { { -833, 0, 4010 }, { 882, 3995, 183 }, { -3912, 901, -812 } }, { -3391, 1940, 9236 } }, 246 },
    { { { { 3753, 0, -1640 }, { 59, 4093, 135 }, { 1639, -148, 3750 } }, { 717, 966, 0x3CDD } }, 257 },
    { { { { 3903, 0, -1241 }, { -46, 4093, -147 }, { 1240, 154, 3900 } }, { 530, 1231, 0x2C30 } }, 329 },
};

SpriteBatch D_dryfield_night_r08_801815DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_801815EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_801815FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_8018160C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_8018161C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_r08_8018162C[9] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -96, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 500, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -160, 0, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -136, 48, 500, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -80, 48, 500, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -24, 48, 500, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 32, 48, 450, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, 56, 412, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 72, 56, 375, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_r08_801816E0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_801816F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_80181708[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_r08_80181718[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_night_r08_80181728[9] = {
    { { .empty = D_dryfield_night_r08_801815DC }, D_dryfield_night_r08_801815DC, NULL },
    { { .empty = D_dryfield_night_r08_801815EC }, D_dryfield_night_r08_801815EC, NULL },
    { { .empty = D_dryfield_night_r08_801815FC }, D_dryfield_night_r08_801815FC, NULL },
    { { .empty = D_dryfield_night_r08_8018160C }, D_dryfield_night_r08_8018160C, NULL },
    { { .empty = D_dryfield_night_r08_8018161C }, D_dryfield_night_r08_8018161C, NULL },
    { { .elements = D_dryfield_night_r08_8018162C }, D_dryfield_night_r08_801816E0, NULL },
    { { .empty = D_dryfield_night_r08_801816F8 }, D_dryfield_night_r08_801816F8, NULL },
    { { .empty = D_dryfield_night_r08_80181708 }, D_dryfield_night_r08_80181708, NULL },
    { { .empty = D_dryfield_night_r08_80181718 }, D_dryfield_night_r08_80181718, NULL },
};

WorldCoordLight D_dryfield_night_r08_80181794[3] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2582, 402, 6341 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 5324, 5324, 5324 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CC5, -2046, -6920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 8192, 8192, 8192 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2899, 1476, -7757 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 6144, 6144, 6144 }, { 0, 0 } },
};

WorldCoordRoomLights D_dryfield_night_r08_8018189C[1] = {
    { ARRAY_SIZE(D_dryfield_night_r08_80181794), D_dryfield_night_r08_80181794, 0, NULL, 0, NULL },
};

GpAreaTmdRec D_dryfield_night_r08_801818B4[3] = {
    { 132, 357, 4, 0, { 0, 0 }, D_8013DADC },
    { 20, 358, 4, 0, { 0, 0 }, D_80146810 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_night_r08_801818D8[11] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017B508, D_dryfield_night_r08_801818B4 },
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

WorldCollisionFootstepSounds D_dryfield_night_r08_80181930 = {
    0x10000045,
    0x10000047,
    0x10000045,
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_8018193C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_80181944[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_8018194C[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_r08_80181954[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_r08_80181930 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_r08_8018195C[8] = {
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_8018193C,
    D_dryfield_night_r08_80181944,
    D_dryfield_night_r08_8018194C,
    D_dryfield_night_r08_80181954,
    D_dryfield_night_r08_8018193C,
};

/// On the task's first tick, stores three fixed ids into `D_80115758`,
/// `D_8011572C` and `D_80115750`, then draws the placements the current camera
/// view shows with the beam and sprite drawers below. The placement names are
/// windows onto one run of 8-byte `SVECTOR`s, so `80180664` is `805BC[21]`,
/// `805AC[23]` and `805CC[19]` as well; views 3 and 9 name it directly and the
/// compiler merges their last two calls into one tail.
void func_dryfield_night_r08_8017D718(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601C7;
        D_8011572C  = 0x601E3;
        D_80115750  = 0x601FF;
        arg0->state = 1;
    }

    switch (gGameSession->location.loc.view) {
        case 3:
            glowDrawBeam(&D_dryfield_night_r08_801805BC[0], 0x200, 0x800, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805BC[2], 0x200, 0, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805BC[4], 0x200, 0, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805BC[6], 0x200, 0x800, 0x100);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[11], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[12], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[13], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[14], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[18], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[19], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[20], 1, 0x300);
            glowDrawFlareClipped(D_dryfield_night_r08_80180664, 1, 0x300);
            break;
        case 2:
        case 5:
            glowDrawBeam(&D_dryfield_night_r08_8018056C[0], 0x200, 0x800, 0x111);
            glowDrawBeam(&D_dryfield_night_r08_8018056C[2], 0x200, 0x800, 0x111);
            glowDrawBeam(&D_dryfield_night_r08_8018056C[4], 0x200, 0, 0x111);
            glowDrawBeam(&D_dryfield_night_r08_8018056C[6], 0x200, 0, 0x111);
            break;
        case 4:
            glowDrawBeam(&D_dryfield_night_r08_801805BC[0], 0x200, 0x800, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805BC[4], 0x200, 0, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805BC[6], 0x200, 0x800, 0x100);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[14], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[20], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805BC[21], 1, 0x300);
            break;
        case 6:
            glowDrawBeam(&D_dryfield_night_r08_801805AC[0], 0x200, 0x800, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805AC[2], 0x200, 0x800, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805AC[6], 0x200, 0, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805AC[8], 0x200, 0x800, 0x100);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[13], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[14], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[15], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[16], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[20], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[21], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[22], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805AC[23], 1, 0x300);
            break;
        case 7:
            glowDrawBeam(D_dryfield_night_r08_801805AC, 0x200, 0x800, 0x10);
            break;
        case 8:
            glowDrawBeam(&D_dryfield_night_r08_801805CC[0], 0x200, 0, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805CC[2], 0x200, 0, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805CC[4], 0x200, 0x800, 0x100);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[9], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[10], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[11], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[12], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[16], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[17], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[18], 1, 0x300);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805CC[19], 1, 0x300);
            break;
        case 9:
            glowDrawBeam(&D_dryfield_night_r08_801805DC[0], 0x200, 0, 0x10);
            glowDrawBeam(&D_dryfield_night_r08_801805DC[2], 0x200, 0x800, 0x100);
            glowDrawFlareClipped(&D_dryfield_night_r08_801805DC[10], 1, 0x300);
            glowDrawFlareClipped(D_dryfield_night_r08_80180664, 1, 0x300);
            break;
    }
}

#include "../../shared/glow_draw_beam.inc.c"

#include "../../shared/glow_draw_flare_clipped.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_dryfield_night_r08_8017E5B0(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_dryfield_night_r08_8017F014(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_night_r08_8017F8FC(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
