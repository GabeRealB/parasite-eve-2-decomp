#include "rooms/shelter_b2_pod_bottom.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "shelter_b2_pod_bottom_private.h"

#include "gameplay/display.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"
// Exported instance: another image refers to this package's copy by name.
#define effectSpriteRiseTask shelterB2PodBottomEffectSpriteRiseTask
#include "../../shared/effect_sprite.h"

static void _effectSpriteDrawBanked(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _effectSpriteDrawRotated(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);

/// Number of points round the rim of a `_ShelterB2PodBottomDiscScratch`. A
/// power of two: the drawer wraps a rim index with
/// `& (SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT - 1)`.
#define SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT 32

/// Angle between neighbouring rim points, in the 4096-per-turn units of
/// `rsin` and `rcos`.
#define SHELTER_B2_POD_BOTTOM_DISC_RIM_ANGLE_STEP (0x1000 / SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT)

/// Scratch-stack workspace for drawing a filled disc as a fan of quads.
///
/// The drawer places the rim points evenly round a circle parallel to a
/// coordinate frame's local XY plane, turns each by that frame's rotation and
/// stores it back narrowed to 16 bits; `center` is the middle of the circle,
/// treated the same way, or the frame's origin. The centre is projected once
/// into `sxy0` and stays there as vertex 0 of every quad. Each quad then covers
/// two rim steps: vertices 1, 2 and 3 are `rim[i]`, `rim[i + 2]` and
/// `rim[i + 1]` for even `i`, wrapping at the last one, so half as many quads
/// as rim points close the disc.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order after drawing. Pointers into the block must not survive its release.
typedef struct {
    SVECTOR rim[SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT]; // Rim points after the frame's rotation, in signed 16-bit coordinate units
    SVECTOR center;                                          // Disc centre after the same rotation; zero when the disc sits on the frame's origin
    s32     otz;                                             // Ordering-table depth: SZ3 / 4 of the quad's last projected vertex
    s32     projectionFlags;                                 // GTE FLAG word after the centre's RTPS, then each quad's RTPT; bit 31 makes it negative and drops the disc or the quad
    DVECTOR sxy0;                                            // Screen position of the centre, vertex 0 of every quad
    DVECTOR sxy1;                                            // Screen position of the current quad's vertex 1
    DVECTOR sxy2;                                            // Screen position of vertex 2
    DVECTOR sxy3;                                            // Screen position of vertex 3
} _ShelterB2PodBottomDiscScratch;
STATIC_ASSERT_SIZEOF(_ShelterB2PodBottomDiscScratch, 0x120);

static void func_shelter_b2_pod_bottom_8017E788(GfxCoord* coord, s16 arg1, s16 arg2);
static void func_shelter_b2_pod_bottom_8017EEAC(EffectWork* work, GfxCoord* coord, s32 arg2);
static void func_shelter_b2_pod_bottom_8018101C(GfxCoord* coord, s16 size, u16 color, u16 scale);

extern u16 D_shelter_b2_pod_bottom_80188790[3][16];

static void func_shelter_b2_pod_bottom_8017F994(GfxCoord* coord, s32 arg1, u8* rgb);
static void func_shelter_b2_pod_bottom_801805A0(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);

// All eight angles are initialized before use. The package contains only
// the first twelve zero bytes of this sixteen-byte runtime allocation.
static s16 D_shelter_b2_pod_bottom_801887F0[8];

static TmdBone _gShelterB2PodBottomModel0A2A0Skeleton[1] = {
#include "assets/shelter_b2_pod_bottom_model_0A2A0_skeleton.inc"
};

static u32 _gShelterB2PodBottomModel0A2A0PartVerts[1] = {
#include "assets/shelter_b2_pod_bottom_model_0A2A0_partVerts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0A2A0Verts[16] = {
#include "assets/shelter_b2_pod_bottom_model_0A2A0_verts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0A2A0Normals[29] = {
#include "assets/shelter_b2_pod_bottom_model_0A2A0_normals.inc"
};

static u32 _gShelterB2PodBottomModel0A2A0Stream[172] = {
#include "assets/shelter_b2_pod_bottom_model_0A2A0_stream.inc"
};

TmdSource gShelterB2PodBottomModel0A2A0 = {
    0,
    1092,
    0,
    1,
    _gShelterB2PodBottomModel0A2A0PartVerts,
    _gShelterB2PodBottomModel0A2A0Verts,
    _gShelterB2PodBottomModel0A2A0Normals,
    _gShelterB2PodBottomModel0A2A0Skeleton,
    _gShelterB2PodBottomModel0A2A0Stream,
};

static TmdBone _gShelterB2PodBottomModel0A68CSkeleton[1] = {
#include "assets/shelter_b2_pod_bottom_model_0A68C_skeleton.inc"
};

static u32 _gShelterB2PodBottomModel0A68CPartVerts[1] = {
#include "assets/shelter_b2_pod_bottom_model_0A68C_partVerts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0A68CVerts[11] = {
#include "assets/shelter_b2_pod_bottom_model_0A68C_verts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0A68CNormals[19] = {
#include "assets/shelter_b2_pod_bottom_model_0A68C_normals.inc"
};

static u32 _gShelterB2PodBottomModel0A68CStream[114] = {
#include "assets/shelter_b2_pod_bottom_model_0A68C_stream.inc"
};

TmdSource gShelterB2PodBottomModel0A68C = {
    0,
    720,
    0,
    1,
    _gShelterB2PodBottomModel0A68CPartVerts,
    _gShelterB2PodBottomModel0A68CVerts,
    _gShelterB2PodBottomModel0A68CNormals,
    _gShelterB2PodBottomModel0A68CSkeleton,
    _gShelterB2PodBottomModel0A68CStream,
};

static TmdBone _gShelterB2PodBottomModel0AA08Skeleton[1] = {
#include "assets/shelter_b2_pod_bottom_model_0AA08_skeleton.inc"
};

static u32 _gShelterB2PodBottomModel0AA08PartVerts[1] = {
#include "assets/shelter_b2_pod_bottom_model_0AA08_partVerts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0AA08Verts[16] = {
#include "assets/shelter_b2_pod_bottom_model_0AA08_verts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0AA08Normals[29] = {
#include "assets/shelter_b2_pod_bottom_model_0AA08_normals.inc"
};

static u32 _gShelterB2PodBottomModel0AA08Stream[167] = {
#include "assets/shelter_b2_pod_bottom_model_0AA08_stream.inc"
};

TmdSource gShelterB2PodBottomModel0AA08 = {
    0,
    1064,
    0,
    1,
    _gShelterB2PodBottomModel0AA08PartVerts,
    _gShelterB2PodBottomModel0AA08Verts,
    _gShelterB2PodBottomModel0AA08Normals,
    _gShelterB2PodBottomModel0AA08Skeleton,
    _gShelterB2PodBottomModel0AA08Stream,
};

static TmdBone _gShelterB2PodBottomModel0AE48Skeleton[1] = {
#include "assets/shelter_b2_pod_bottom_model_0AE48_skeleton.inc"
};

static u32 _gShelterB2PodBottomModel0AE48PartVerts[1] = {
#include "assets/shelter_b2_pod_bottom_model_0AE48_partVerts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0AE48Verts[15] = {
#include "assets/shelter_b2_pod_bottom_model_0AE48_verts.inc"
};

static SVECTOR _gShelterB2PodBottomModel0AE48Normals[28] = {
#include "assets/shelter_b2_pod_bottom_model_0AE48_normals.inc"
};

static u32 _gShelterB2PodBottomModel0AE48Stream[145] = {
#include "assets/shelter_b2_pod_bottom_model_0AE48_stream.inc"
};

TmdSource gShelterB2PodBottomModel0AE48 = {
    0,
    928,
    0,
    1,
    _gShelterB2PodBottomModel0AE48PartVerts,
    _gShelterB2PodBottomModel0AE48Verts,
    _gShelterB2PodBottomModel0AE48Normals,
    _gShelterB2PodBottomModel0AE48Skeleton,
    _gShelterB2PodBottomModel0AE48Stream,
};

WorldCollisionTrigger D_shelter_b2_pod_bottom_80188670[1] = {
    { NULL, NULL, NULL, { -2336, -2176, 6848, 0 }, { { -576, 0, -1024, 0 }, { 576, 0, -1024, 0 }, { -576, 0, 1024, 0 }, { 576, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1173, WORLD_COLLISION_TRIGGER_ACTION_WARP, 35, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_shelter_b2_pod_bottom_801886BC[17] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b2_pod_bottom_801886BC) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 820, 820, 820, 820 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_shelter_b2_pod_bottom_80188744 = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_bottom_80188750[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_bottom_80188758[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_pod_bottom_80188744 },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_bottom_80188760[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b2_pod_bottom_80188768[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b2_pod_bottom_80188744 },
};

WorldCollisionSurfaceProperties* D_shelter_b2_pod_bottom_80188770[8] = {
    D_shelter_b2_pod_bottom_80188750,
    D_shelter_b2_pod_bottom_80188758,
    D_shelter_b2_pod_bottom_80188760,
    D_shelter_b2_pod_bottom_80188768,
    D_shelter_b2_pod_bottom_80188750,
    D_shelter_b2_pod_bottom_80188750,
    D_shelter_b2_pod_bottom_80188750,
    D_shelter_b2_pod_bottom_80188750,
};

u16 D_shelter_b2_pod_bottom_80188790[3][16] = { 0 };

static void func_shelter_b2_pod_bottom_80180A4C(GfxCoord* coord, s16 radius, SVECTOR* center);

/// On its first frame (state 0) fills three rows of 16 random bytes in
/// `D_shelter_b2_pod_bottom_80188790` from the gameplay LCG and turns off
/// `groundTraceEnabled`; every frame, disables the ground shadow in view 0xF and
/// selects shade row 0 elsewhere.
void func_shelter_b2_pod_bottom_8017D760(Task* task)
{
    s32 i;

    if (task->state == 0) {
        for (i = 0; i < 16; i++) {
            gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_shelter_b2_pod_bottom_80188790[0][i] = (gRandomLcgState >> 16) & 0xFF;
            gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_shelter_b2_pod_bottom_80188790[1][i] = (gRandomLcgState >> 16) & 0xFF;
            gRandomLcgState                        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_shelter_b2_pod_bottom_80188790[2][i] = (gRandomLcgState >> 16) & 0xFF;
        }
        task->state                          = 1;
        gRoomEffectState->groundTraceEnabled = false;
    }
    if ((Gp_GetViewIndex() & 0xFF) == 0xF) {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_DISABLED;
    } else {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_UNMODULATED;
    }
}

/// Names this room's externally linked drift-effect callback, declared in its public header.
///
/// Bind a function identifier with signature `void (Task*)` immediately before
/// the drift fragment. It uses the name once for the definition and clears the
/// binding afterwards; there are no arguments, captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_TASK shelterB2PodBottomEffectSpriteDriftTask
#include "../../shared/effect_sprite_drift.inc.c"

#define EFFECT_SPRITE_BANKED_FIRST_TEXEL_ROW 112
#define EFFECT_SPRITE_BANKED_DEPTH_BIAS      1
#include "../../shared/effect_sprite_draw_banked.inc.c"

#define EFFECT_SPRITE_ROTATED_DEPTH_BIAS 1
#include "../../shared/effect_sprite_draw_rotated.inc.c"

/// Draws a glowing band: two 16-vertex rings in the XZ plane, the inner of
/// radius `arg1` at a height of -0x180 and the outer of radius `arg1 + 0x200`
/// at 0, are rotated by `coord`'s `workm` and offset by its translation, then
/// each of the 16 segments is projected through `GsWSMATRIX` as one
/// `POLY_G4`. The inner edge carries the `(arg2 >> 1, arg2 >> 1, arg2)` colour
/// and the outer edge fades to black; a negative `gte_stflg` drops the
/// segment.
static void func_shelter_b2_pod_bottom_8017E788(GfxCoord* coord, s16 arg1, s16 arg2)
{
    EffectBandScratch* block;
    SVECTOR*           op;
    POLY_G4*           prim;
    s32                i;
    s32                next;
    s32                ang;
    s16                r0;
    s16                r1;
    u8                 red;
    u8                 grn;
    u8                 blu;

    r1    = arg1 + 0x200;
    red   = arg2 >> 1;
    grn   = arg2 >> 1;
    blu   = arg2;
    block = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    r0 = arg1;
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        ang                  = i << 8;
        block->topRing[i].vx = (rsin(ang) * r0) >> 12;
        block->topRing[i].vy = -0x180;
        block->topRing[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx    = (u16)block->topRing[i].vx + (u16)coord->workm.t[0];
        block->topRing[i].vy    = (u16)block->topRing[i].vy + (u16)coord->workm.t[1];
        block->topRing[i].vz    = (u16)block->topRing[i].vz + (u16)coord->workm.t[2];
        block->bottomRing[i].vx = (rsin(ang) * r1) >> 12;
        op                      = &block->topRing[i] + EFFECT_BAND_SEGMENT_COUNT;
        op->vy                  = 0;
        op->vz                  = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx = (u16)block->bottomRing[i].vx + (u16)coord->workm.t[0];
        op->vy                  = (u16)op->vy + (u16)coord->workm.t[1];
        op->vz                  = (u16)op->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        gte_stsxy(&block->sxy0);
        next = (i + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
        gte_ldv3(&block->topRing[next], &block->bottomRing[i], &block->bottomRing[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, red, grn, blu);
            setRGB1(prim, red, grn, blu);
            setRGB2(prim, 0, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

/// State 0 resets the coordinate frame's rotation to identity and starts the
/// colour ramp at 0xA0. State 1 steps the ramps while no event is running
/// (holding the tick otherwise), calls `func_shelter_b2_pod_bottom_8017EEAC`
/// for indices 0-2, then draws three arcs stacked up the frame's Y axis and a
/// fade quad in the ramp colour. The work is released once the ramp reaches 8
/// or an event of state 4 or above starts.
void func_shelter_b2_pod_bottom_8017EC78(Task* task)
{
    EffectWork*       work;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    u16               tick;
    u8                rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        tick                = work->age;
        work->age           = tick + 1;
        switch (task->state) {
            case 0:
                rot         = (GfxRotationWords*)&coord->coord;
                rot->m00M01 = ONE;
                rot->m02M10 = 0;
                rot->m11M12 = ONE;
                rot->m20M21 = 0;
                rot->m22    = ONE;
                work->scale = 0xA0;
                task->state++;
                return;
            case 1:
                if (work->scale < 9) {
                    break;
                }
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    work->scale  -= 8;
                    work->angle  += 0x80;
                    work->period -= 0x20;
                    work->step   += 0x20;
                } else {
                    work->age = tick;
                }
                func_shelter_b2_pod_bottom_8017EEAC(work, coord, 0);
                func_shelter_b2_pod_bottom_8017EEAC(work, coord, 1);
                func_shelter_b2_pod_bottom_8017EEAC(work, coord, 2);
                rgb[0] = rgb[1]    = work->scale;
                rgb[2]             = work->scale * 3 / 2;
                coord->workm.t[1] -= work->age * 0x30;
                Gp_DrawArc(coord, (s16)(work->age << 6), 0x100, rgb);
                coord->workm.t[1] -= work->age * 0x30;
                Gp_DrawArc(coord, (s16)(work->age << 7), 0x100, rgb);
                coord->workm.t[1] -= work->age * 0x30;
                Gp_DrawArc(coord, (s16)(work->age * 0xC0), 0x100, rgb);
                Gp_DrawFadeQuad(rgb, 1);
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Draws band `arg2` of the effect as sixteen semi-transparent `POLY_FT4`
/// segments. Builds two 16-vertex rings in the XZ plane - one at the frame's
/// origin height, one raised and wider - from the work's ramps plus the row's
/// `D_shelter_b2_pod_bottom_80181C94` offsets, moves them into world space
/// through `coord`, then projects each segment between the rings and picks its
/// texture cell from the row's `D_shelter_b2_pod_bottom_80188790` value and the
/// work's tick.
static void func_shelter_b2_pod_bottom_8017EEAC(EffectWork* work, GfxCoord* coord, s32 arg2)
{
    EffectBandScratch* block;
    SVECTOR*           op;
    POLY_FT4*          prim;
    EffectBandShape*   row;
    s32                i;
    s32                next;
    s32                ang;
    s32                u;
    u16                idx;
    s16                r0;
    s16                r1;
    u16                y;
    u16                f28;

    row   = &D_shelter_b2_pod_bottom_80181C94[arg2];
    f28   = work->period;
    r1    = work->angle;
    y     = f28 + row->lift;
    r1   += row->baseRadius;
    r0    = r1 + work->step + row->spread;
    block = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        ang                  = i << 8;
        block->topRing[i].vx = (rsin(ang) * r0) >> 12;
        block->topRing[i].vy = -y;
        block->topRing[i].vz = (rcos(ang) * r0) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->topRing[i]);
        gte_rtv0();
        gte_stsv(&block->topRing[i]);
        block->topRing[i].vx    = (u16)block->topRing[i].vx + (u16)coord->workm.t[0];
        block->topRing[i].vy    = (u16)block->topRing[i].vy + (u16)coord->workm.t[1];
        block->topRing[i].vz    = (u16)block->topRing[i].vz + (u16)coord->workm.t[2];
        block->bottomRing[i].vx = (rsin(ang) * r1) >> 12;
        op                      = &block->topRing[i] + EFFECT_BAND_SEGMENT_COUNT;
        op->vy                  = 0;
        op->vz                  = (rcos(ang) * r1) >> 12;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->bottomRing[i]);
        gte_rtv0();
        gte_stsv(&block->bottomRing[i]);
        block->bottomRing[i].vx = (u16)block->bottomRing[i].vx + (u16)coord->workm.t[0];
        op->vy                  = (u16)op->vy + (u16)coord->workm.t[1];
        op->vz                  = (u16)op->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (i = 0; i < EFFECT_BAND_SEGMENT_COUNT; i++) {
        gte_ldv0(&block->topRing[i]);
        gte_rtps();
        idx = (D_shelter_b2_pod_bottom_80188790[arg2][i] + work->age) % 6;
        gte_stsxy(&block->sxy0);
        next = (i + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
        gte_ldv3(&block->topRing[next], &block->bottomRing[i], &block->bottomRing[next]);
        gte_rtpt();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            gte_stszotz(&block->otz);
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyFT4(prim);
            setRGB0(prim, *(u8*)&work->scale, *(u8*)&work->scale, *(u8*)&work->scale);
            setSemiTrans(prim, 1);
            prim->tpage = 0x2A;
            prim->clut  = 0x42C1;
            u           = idx * 0x28;
            setUV4(prim, u, 0x60, u + 0x27, 0x60, u, 0x87, u + 0x27, 0x87);
            prim->x0 = (u16)block->sxy0.vx;
            prim->y0 = (u16)block->sxy0.vy;
            prim->x1 = (u16)block->sxy1.vx;
            prim->y1 = (u16)block->sxy1.vy;
            prim->x2 = (u16)block->sxy2.vx;
            prim->y2 = (u16)block->sxy2.vy;
            prim->x3 = (u16)block->sxy3.vx;
            prim->y3 = (u16)block->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

/// State 0 resets the coordinate frame's rotation to identity, starts the
/// colour ramp at 0 and the ring radius at 0x80, derives the colour step from
/// `Task::spawnArg1` and picks a random tint row. State 1 grows both while no
/// event is running (holding the tick otherwise) and draws two rings, at the
/// radius and twice it, plus an arc whose sweep cycles with `spawnArg1 % 10`;
/// when `spawnArg1` reaches 0 the colour is set to 0xFF and state 2 begins.
/// State 2 draws the two rings, fading the colour by 0x10 per frame. Each
/// channel is the colour shifted right by the tint row's entry for it, and
/// the row is re-rolled below 18 on every animating frame. The work is
/// released once the fade reaches 0x10 or an event of state 4 or above starts.
void func_shelter_b2_pod_bottom_8017F448(Task* task)
{
    EffectWork*       work;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s32               sum;
    u8                rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case 0:
                rot                 = (GfxRotationWords*)&coord->coord;
                rot->m00M01         = ONE;
                rot->m02M10         = 0;
                rot->m11M12         = ONE;
                rot->m20M21         = 0;
                rot->m22            = ONE;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = 0;
                work->angle         = 0x80;
                work->step          = 0xC0 / task->spawnArg1.value;
                task->state         = 1;
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->index         = (gRandomLcgState >> 16) % 18;
            case 1:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    work->age--;
                    rgb[0] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][0];
                    rgb[1] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][1];
                    rgb[2] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][2];
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)(task->spawnArg1.value % 10 * (work->scale << 2)), 0x80, rgb);
                    return;
                }
                sum         = (u16)work->scale + (u16)work->step;
                work->scale = sum;
                work->angle = sum * 4 + 0x80;
                task->spawnArg1.value--;
                rgb[0] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][0];
                rgb[1] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][1];
                rgb[2] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][2];
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)(task->spawnArg1.value % 10 * (work->scale << 2)), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale  = 0xFF;
                    task->state  = 2;
                    work->period = 0x300;
                    work->step   = 0;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->index     = (gRandomLcgState >> 16) % 18;
                return;
            case 2:
                if (work->scale < 0x11) {
                    break;
                }
                rgb[0] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][0];
                rgb[1] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][1];
                rgb[2] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][2];
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->scale    -= 0x10;
                    work->index     = (gRandomLcgState >> 16) % 18;
                }
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Projects `coord`'s world position through `GsWSMATRIX` into a scratch block
/// popped from the scratch stack and, when the GTE flag is non-negative, queues
/// twenty gouraud `POLY_G4` wedges fanned about the projected point. The radii
/// are `(s16)arg1 * 64 / otz` (outer) and `(s16)arg1 * 8 / otz` (inner). Each
/// of the first eight steps draws a half-bright wedge at the outer radius and
/// a full-bright one at half of it; four half-bright spikes follow, reaching
/// twice the outer radius between two inner-radius corners. Only the apex at
/// the projected point is coloured, from `rgb`; every rim corner is black.
static void func_shelter_b2_pod_bottom_8017F994(GfxCoord* coord, s32 arg1, u8* rgb)
{
    EffectShapeScratch* block;
    POLY_G4*            prim;
    s32                 ang;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->extent.burst.outer = ((s16)arg1 * 64) / block->depth;
        block->extent.burst.inner = ((s16)arg1 * 8) / block->depth;

        ang = 0;
        do {
            s32 mid;
            s32 next;

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0] >> 1, rgb[1] >> 1, rgb[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->extent.burst.outer * rsin(ang)) >> 12);
            mid      = ang + 0x100;
            prim->y0 = block->screenY + ((block->extent.burst.outer * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->extent.burst.outer * rsin(mid)) >> 12);
            prim->y1 = block->screenY + ((block->extent.burst.outer * rcos(mid)) >> 12);
            next     = ang + 0x200;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->extent.burst.outer * rsin(next)) >> 12);
            prim->y3 = block->screenY + ((block->extent.burst.outer * rcos(next)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->extent.burst.outer * rsin(ang)) >> 13);
            prim->y0 = block->screenY + ((block->extent.burst.outer * rcos(ang)) >> 13);
            prim->x1 = block->screenX + ((block->extent.burst.outer * rsin(mid)) >> 13);
            prim->y1 = block->screenY + ((block->extent.burst.outer * rcos(mid)) >> 13);
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->extent.burst.outer * rsin(next)) >> 13);
            prim->y3 = block->screenY + ((block->extent.burst.outer * rcos(next)) >> 13);
            ang      = next;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            s32 next;
            s32 far;

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0] >> 1, rgb[1] >> 1, rgb[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenX + ((block->extent.burst.inner * rsin(ang)) >> 12);
            next     = ang + 0x400;
            prim->y0 = block->screenY + ((block->extent.burst.inner * rcos(ang)) >> 12);
            prim->x1 = block->screenX + ((block->extent.burst.outer * rsin(next)) >> 11);
            prim->y1 = block->screenY + ((block->extent.burst.outer * rcos(next)) >> 11);
            far      = ang + 0x800;
            prim->x2 = block->screenX;
            prim->y2 = block->screenY;
            prim->x3 = block->screenX + ((block->extent.burst.inner * rsin(far)) >> 12);
            prim->y3 = block->screenY + ((block->extent.burst.inner * rcos(far)) >> 12);
            ang      = next;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// State 0 resets the coordinate frame's rotation to identity, starts the
/// colour ramp at 0 and the size ramp at 0x80, derives the colour step from
/// `Task::spawnArg1`, and fills `D_shelter_b2_pod_bottom_801887F0` with eight
/// angles, one random angle inside each eighth of the circle. State 1 grows
/// both ramps once per frame while no event is running (holding the tick
/// otherwise) and draws the ramp-coloured effect, a ring and an arc that
/// shrinks as `spawnArg1` counts down; when it reaches 0 the colour is set to
/// 0xFF and state 2 begins. State 2 draws the effect, the ring and a blade at
/// each of the eight angles, fading the colour by 0x10 per frame. While
/// animating, `index` is re-rolled to a random value below 18 every frame.
/// The work is released once the fade reaches 0x10 or an event of state 4 or
/// above starts.
void func_shelter_b2_pod_bottom_8018016C(Task* task)
{
    EffectWork*       work;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s32               i;
    s32               sum;
    u8                rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case 0:
                rot                 = (GfxRotationWords*)&coord->coord;
                rot->m00M01         = ONE;
                rot->m02M10         = 0;
                rot->m11M12         = ONE;
                rot->m20M21         = 0;
                rot->m22            = ONE;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = 0;
                work->angle         = 0x80;
                work->step          = 0xC0 / task->spawnArg1.value;
                task->state         = 1;
                for (i = 0; i < 8; i++) {
                    gRandomLcgState                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_shelter_b2_pod_bottom_801887F0[i] = (i << 9) + ((gRandomLcgState >> 16) & 0x1FF);
                }
            case 1:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    work->age--;
                    rgb[0] = work->scale;
                    rgb[1] = work->scale;
                    rgb[2] = work->scale >> 1;
                    func_shelter_b2_pod_bottom_8017F994(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 4), rgb);
                    Gp_DrawArc(coord, (s16)(task->spawnArg1.value * work->step * 16), 0x80, rgb);
                    return;
                }
                sum         = (u16)work->scale + (u16)work->step;
                work->scale = sum;
                work->angle = sum * 4 + 0x80;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale;
                rgb[2] = work->scale >> 1;
                func_shelter_b2_pod_bottom_8017F994(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 4), rgb);
                Gp_DrawArc(coord, (s16)(task->spawnArg1.value * work->step * 16), 0x80, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale  = 0xFF;
                    task->state  = 2;
                    work->period = 0x300;
                    work->step   = 0;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->index     = (gRandomLcgState >> 16) % 18;
                return;
            case 2:
                if (work->scale < 0x11) {
                    break;
                }
                rgb[0] = work->scale;
                rgb[1] = work->scale;
                rgb[2] = work->scale >> 1;
                func_shelter_b2_pod_bottom_8017F994(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 4), rgb);
                for (i = 0; i < 8; i++) {
                    func_shelter_b2_pod_bottom_801805A0(coord, (s16)((u16)work->angle * 2), D_shelter_b2_pod_bottom_801887F0[i], rgb);
                }
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->scale    -= 0x10;
                    work->index     = (gRandomLcgState >> 16) % 18;
                }
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Draws one Gouraud triangle as a fan blade about `arg2`. `arg0`'s world
/// position is projected through `GsWSMATRIX` into a scratch block popped from
/// the scratch stack; the apex sits on that point in `rgb`, and the two black
/// outer corners sit at angles `arg2 - 0x20` and `arg2 + 0x20`, `arg1` scaled
/// down by the projected depth away. A negative GTE flag drops the triangle.
static void func_shelter_b2_pod_bottom_801805A0(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    void**               scratch;
    u8*                  head;
    EffectCentreScratch* block;
    SVECTOR*             vec;
    POLY_G3*             prim;
    s32                  ang;
    s32                  ang2;
    u16                  vz;

    scratch                                                                     = SCRATCH_STACK_CURSOR_SLOT;
    head                                                                        = *scratch;
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)arg0->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = (u16)arg0->workm.t[1];
    vz                                                                          = (u16)arg0->workm.t[2];
    *scratch                                                                    = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        setRGB0(prim, rgb[0], rgb[1], rgb[2]);
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->screenExtent = ((s16)arg1 * 128) / block->depth;
        ang                 = (s16)arg2;
        ang2                = ang - 0x20;
        prim->x0            = block->screenX;
        prim->y0            = block->screenY;
        prim->x1            = block->screenX + ((block->screenExtent * rsin(ang2)) >> 12);
        prim->y1            = block->screenY + ((block->screenExtent * rcos(ang2)) >> 12);
        ang                += 0x20;
        prim->x2            = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
        prim->y2            = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#include "../../shared/effect_sprite_rise.inc.c"

/// Draws a flat white disc of radius `radius` in `coord`'s local XY plane,
/// centred on `center` (the frame's origin when it is NULL). 32 rim points are
/// rotated by `coord->workm`, the centre is projected through the full
/// `workm`, and the disc is queued as 16 `POLY_F4` fans, each joining the
/// centre to three consecutive rim points; any piece with a negative GTE flag
/// is dropped.
static void func_shelter_b2_pod_bottom_80180A4C(GfxCoord* coord, s16 radius, SVECTOR* center)
{
    _ShelterB2PodBottomDiscScratch* block;
    POLY_F4*                        prim;
    s32                             i;
    s32                             ang;

    block = SCRATCH_STACK_RESERVE_BLOCK(_ShelterB2PodBottomDiscScratch);
    gte_SetTransMatrix(&coord->workm);
    if (center != NULL) {
        for (i = 0; i < SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT; i++) {
            block->rim[i].vx = (u16)center->vx + ((rsin(i * SHELTER_B2_POD_BOTTOM_DISC_RIM_ANGLE_STEP) * radius) >> 12);
            block->rim[i].vy = (u16)center->vy + ((rcos(i * SHELTER_B2_POD_BOTTOM_DISC_RIM_ANGLE_STEP) * radius) >> 12);
            block->rim[i].vz = center->vz;
            gte_SetRotMatrix(&coord->workm);
            gte_ldv0(&block->rim[i]);
            gte_rtv0();
            gte_stsv(&block->rim[i]);
        }
        block->center.vx = center->vx;
        block->center.vy = center->vy;
        block->center.vz = center->vz;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&block->center);
        gte_rtv0();
        gte_stsv(&block->center);
    } else {
        for (i = 0; i < SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT; i++) {
            ang              = i * SHELTER_B2_POD_BOTTOM_DISC_RIM_ANGLE_STEP;
            block->rim[i].vx = (rsin(ang) * radius) >> 12;
            block->rim[i].vy = (rcos(ang) * radius) >> 12;
            block->rim[i].vz = 0;
            gte_SetRotMatrix(&coord->workm);
            gte_ldv0(&block->rim[i]);
            gte_rtv0();
            gte_stsv(&block->rim[i]);
        }
        block->center.vx = 0;
        block->center.vy = 0;
        block->center.vz = 0;
    }
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->center);
    gte_rtps();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        for (i = 0; i < SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT; i += 2) {
            gte_ldv3(&block->rim[i], &block->rim[(i + 2) & (SHELTER_B2_POD_BOTTOM_DISC_RIM_POINT_COUNT - 1)], &block->rim[i + 1]);
            gte_rtpt();
            gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
            gte_stflg(&block->projectionFlags);
            if (block->projectionFlags >= 0) {
                gte_stszotz(&block->otz);
                prim           = gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyF4(prim);
                setRGB0(prim, 0xFF, 0xFF, 0xFF);
                prim->x0 = block->sxy0.vx;
                prim->y0 = block->sxy0.vy;
                prim->x1 = block->sxy1.vx;
                prim->y1 = block->sxy1.vy;
                prim->x2 = block->sxy2.vx;
                prim->y2 = block->sxy2.vy;
                prim->x3 = block->sxy3.vx;
                prim->y3 = block->sxy3.vy;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        prim);
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(_ShelterB2PodBottomDiscScratch);
}

void func_shelter_b2_pod_bottom_80180F10(Task* arg0)
{
    EffectWork* work;
    GfxCoord*   coord;
    u32         rnd;

    work  = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rnd;
            func_shelter_b2_pod_bottom_8018101C(coord, 0x100, (rnd >> 16) & 0x777, 0x10);
            return;
        }
        effectKillTask(work, arg0);
        return;
    }
    work->age++;
    coord->coord.t[1]  += arg0->spawnArg1.value;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->age < 8) {
        func_shelter_b2_pod_bottom_8018101C(coord, 0x100, 0xCCC, 0x10);
        return;
    }
    func_shelter_b2_pod_bottom_8018101C(coord, 0x100, 0xCCC, (u16)((0x10 - work->age) * 2));
    if (work->age >= 0x10) {
        effectKillTask(work, arg0);
    }
}

/// Queues a beam of gouraud `POLY_G4` wedges along `coord`'s local up axis:
/// the tip sits `size * 16` units above the coordinate's world position, and
/// both ends are projected. Two passes widen the wedge radii (`size * 64` and
/// `size * 128` over each end's depth) and draw a fan at each end plus a
/// connecting quad. `color` packs `[r][g][b]` nibbles scaled by `scale`, with
/// `gDisplayState.animFrame & 1` adding a 16-unit flicker to every channel.
static void func_shelter_b2_pod_bottom_8018101C(GfxCoord* coord, s16 size, u16 color, u16 scale)
{
    RoomBeamScratch* block;
    POLY_G4*         prim;
    s32              pass;
    u8               r;
    u8               g;
    u8               b;
    s32              limit;
    s32              angStart;
    s32              scaled;
    s32              ang;
    s32              next;
    s32              mid;
    s32              blend;

    block            = SCRATCH_STACK_RESERVE_BLOCK(RoomBeamScratch);
    block->point1.vy = -(size << 4);
    block->point1.vx = 0;
    block->point1.vz = 0;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&block->point1);
    gte_rtv0();
    gte_stsv(&block->point1);
    block->point0.vx  = coord->workm.t[0];
    block->point0.vy  = coord->workm.t[1];
    block->point0.vz  = coord->workm.t[2];
    block->point1.vx += block->point0.vx;
    block->point1.vy += block->point0.vy;
    block->point1.vz += block->point0.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->point0);
    gte_rtps();
    gte_stsxy(&block->pair.sx0);
    gte_stflg(&block->pair.flag);
    if (block->pair.flag >= 0) {
        gte_stszotz(&block->pair.otz0);
        gte_ldv0(&block->point1);
        gte_rtps();
        gte_stsxy(&block->pair.sx1);
        gte_stflg(&block->pair.flag);
        gte_stszotz(&block->pair.otz1);
        if (block->pair.flag >= 0) {
            blend = ((u8)gDisplayState.animFrame & 1) << 4;
            r     = scale * ((color >> 8) & 0xF) + blend;
            g     = scale * ((color >> 4) & 0xF) + blend;
            b     = scale * (color & 0xF) + blend;
            for (pass = 1; pass < 3; pass++) {
                scaled              = size * (pass << 6);
                block->pair.radius0 = scaled / block->pair.otz0;
                block->pair.radius1 = scaled / block->pair.otz1;
                ang                 = (s16)ratan2((s16)block->pair.sy1 - (s16)block->pair.sy0, (s16)block->pair.sx0 - (s16)block->pair.sx1);
                if (ang < ang + 0x800) {
                    angStart = ang;
                    limit    = ang + 0x800;
                    do {
                        prim           = gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0x800)) >> 12);
                        prim->y0 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0x800)) >> 12);
                        prim->x1 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0xA00)) >> 12);
                        prim->y1 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0xA00)) >> 12);
                        prim->x2 = block->pair.sx1;
                        prim->y2 = block->pair.sy1;
                        prim->x3 = block->pair.sx1 + ((block->pair.radius1 * rsin(ang + 0xC00)) >> 12);
                        prim->y3 = block->pair.sy1 + ((block->pair.radius1 * rcos(ang + 0xC00)) >> 12);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz1);

                        prim           = gGpuPrimCursor;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, 0, 0, 0);
                        prim->x0 = block->pair.sx0 + ((block->pair.radius0 * rsin(ang)) >> 12);
                        prim->y0 = block->pair.sy0 + ((block->pair.radius0 * rcos(ang)) >> 12);
                        prim->x1 = block->pair.sx0 + ((block->pair.radius0 * rsin(ang + 0x200)) >> 12);
                        prim->y1 = block->pair.sy0 + ((block->pair.radius0 * rcos(ang + 0x200)) >> 12);
                        next     = ang + 0x400;
                        prim->x2 = block->pair.sx0;
                        prim->y2 = block->pair.sy0;
                        prim->x3 = block->pair.sx0 + ((block->pair.radius0 * rsin(next)) >> 12);
                        prim->y3 = block->pair.sy0 + ((block->pair.radius0 * rcos(next)) >> 12);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz0);

                        prim           = gGpuPrimCursor;
                        mid            = angStart + (ang - angStart) * 2;
                        gGpuPrimCursor = prim + 1;
                        setPolyG4(prim);
                        setRGB0(prim, 0, 0, 0);
                        setRGB1(prim, 0, 0, 0);
                        setRGB2(prim, r, g, b);
                        setRGB3(prim, r, g, b);
                        prim->x0 = block->pair.sx0 + ((block->pair.radius0 * rsin(mid)) >> 12);
                        prim->y0 = block->pair.sy0 + ((block->pair.radius0 * rcos(mid)) >> 12);
                        prim->x1 = block->pair.sx1 + ((block->pair.radius1 * rsin(mid)) >> 12);
                        prim->y1 = block->pair.sy1 + ((block->pair.radius1 * rcos(mid)) >> 12);
                        prim->x2 = block->pair.sx0;
                        prim->y2 = block->pair.sy0;
                        prim->x3 = block->pair.sx1;
                        prim->y3 = block->pair.sy1;
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                prim);
                        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->pair.otz1);
                        ang = next;
                    } while (ang < limit);
                }
            }
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomBeamScratch);
}

void func_shelter_b2_pod_bottom_80181940(Task* arg0)
{
    GfxCoord* coord;
    u32       rnd;

    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        coord           = &arg0->extra.tmd->coords[(u16)((rnd >> 16) % 18) + 2];
        Gp_SpawnEff(EFFECT_FLASH_BURST, coord, 0x10300, 0);
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        if ((rnd >> 16) & 1) {
            Gp_SpawnEff(EFFECT_SPARK_FADE, coord, 0x10300, 0);
        }
    }
}

void func_shelter_b2_pod_bottom_80181A48(Task* arg0)
{
    GfxCoord* coord;
    u32       rnd;

    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        coord           = &arg0->extra.tmd->coords[(u16)((rnd >> 16) % 18) + 2];
        Gp_SpawnEff(EFFECT_RISING_ENERGY_SPARK, coord, 0x8600, 0);
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        if (!((rnd >> 16) & 1)) {
            Gp_SpawnEff(EFFECT_RISING_ENERGY_SPARK, coord, 0x8600, 0);
        }
    }
}

void func_shelter_b2_pod_bottom_80181B48(Task* arg0)
{
    EffectWork* work;
    GfxCoord*   coord;
    s16         y;

    work  = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, arg0);
        return;
    }
    work->age++;
    switch (arg0->state) {
        case 0:
            gfxRotMatrixX(&coord->coord, arg0->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
            work->scale = 0xC0;
            work->angle = 0x180;
            arg0->state = 1;
        case 1:
            func_shelter_b2_pod_bottom_8017E788(coord, work->angle, work->scale);
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->angle += 0x60;
                y            = work->scale - 0x18;
                work->scale  = y;
                if (y < 0x18) {
                    effectKillTask(work, arg0);
                }
            } else {
                work->age--;
            }
            break;
    }
}
