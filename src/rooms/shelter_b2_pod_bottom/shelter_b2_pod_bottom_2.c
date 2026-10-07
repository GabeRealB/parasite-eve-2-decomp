#include "rooms/shelter_b2_pod_bottom.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
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
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"
/// Binds the pod bottom's exported `void (Task*)` callback for the rising-sprite instance.
///
/// Supply a function identifier before the first inclusion of `effect_sprite.h`.
/// Keep it defined through `effect_sprite_rise.inc.c`, which undefines it.
/// Gameplay imports this package's compiled callback for effect slot 0x1BF.
#define EFFECT_SPRITE_RISE_TASK shelterB2PodBottomEffectSpriteRiseTask
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

enum {
    SHELTER_B2_POD_BOTTOM_EFFECT_INIT    = 0,
    SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE  = 1,
    SHELTER_B2_POD_BOTTOM_EFFECT_FADE    = 2,
    SHELTER_B2_POD_BOTTOM_TRIG_SHIFT     = 12,
    SHELTER_B2_POD_BOTTOM_FULL_TURN      = 0x1000,
    SHELTER_B2_POD_BOTTOM_HALF_TURN      = 0x800,
    SHELTER_B2_POD_BOTTOM_QUARTER_TURN   = 0x400,
    SHELTER_B2_POD_BOTTOM_EIGHTH_TURN    = 0x200,
    SHELTER_B2_POD_BOTTOM_SIXTEENTH_TURN = 0x100
};

static void _shelterB2PodBottomDrawShockRing(const GfxCoord* coord, s16 innerRadius, s16 brightness);
static void _shelterB2PodBottomDrawArcFlashBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex);
static void _shelterB2PodBottomDrawLightBeam(const GfxCoord* coord, s16 radiusScale, u16 packedColor, u16 brightness);

extern u16 D_shelter_b2_pod_bottom_80188790[3][16];

static void _shelterB2PodBottomDrawStarburst(const GfxCoord* coord, s32 radiusScale, const u8* rgb);
static void _shelterB2PodBottomDrawBurstBlade(const GfxCoord* coord, s32 radiusScale, s32 bladeAngle, const u8* rgb);

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

void shelterB2PodBottomShadowTask(Task* task)
{
    enum {
        SHELTER_B2_POD_BOTTOM_SHADOW_HIDDEN_VIEW = 15,
        SHELTER_B2_POD_BOTTOM_MAPPED_VIEW_MASK   = 0xFF
    };

    s32 segmentIndex;

    if (task->state == SHELTER_B2_POD_BOTTOM_EFFECT_INIT) {
        for (segmentIndex = 0; segmentIndex < (s32)ARRAY_SIZE(D_shelter_b2_pod_bottom_80188790[0]); segmentIndex++) {
            gRandomLcgState                                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_shelter_b2_pod_bottom_80188790[0][segmentIndex] = (gRandomLcgState >> 16) & 0xFF;
            gRandomLcgState                                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_shelter_b2_pod_bottom_80188790[1][segmentIndex] = (gRandomLcgState >> 16) & 0xFF;
            gRandomLcgState                                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            D_shelter_b2_pod_bottom_80188790[2][segmentIndex] = (gRandomLcgState >> 16) & 0xFF;
        }
        task->state                          = SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE;
        gRoomEffectState->groundTraceEnabled = false;
    }
    if ((viewGetMappedIndex() & SHELTER_B2_POD_BOTTOM_MAPPED_VIEW_MASK) == SHELTER_B2_POD_BOTTOM_SHADOW_HIDDEN_VIEW) {
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

/// Projects the four corners of one shock-ring quad into screen coordinates.
///
/// Borrows a writable `EffectBandScratch` whose two rims contain world-space
/// signed-halfword positions. Requires the GTE camera matrices and projection
/// settings to be loaded and `segmentIndex` in 0..EFFECT_BAND_SEGMENT_COUNT-1.
/// Screen corners 0/1 come from the current/next top vertex and corners 2/3
/// from the current/next bottom vertex; the next index wraps at the seam.
/// Stores only the final RTPT's flags, ignoring the first corner's RTPS flags.
/// Leaves corner 3's depth in GTE SZ3 for the caller to sort with; `scratch->otz`
/// is untouched. Retains no pointer and leaves both rims intact.
static inline void _shelterB2PodBottomProjectShockRingSegment(EffectBandScratch* scratch, s32 segmentIndex)
{
    s32 nextSegmentIndex;

    // Save corner 0 before RTPT replaces the SXY FIFO with corners 1..3.
    gte_ldv0(&scratch->topRing[segmentIndex]);
    gte_rtps();
    gte_stsxy(&scratch->sxy0);
    nextSegmentIndex = (segmentIndex + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);
    gte_ldv3(&scratch->topRing[nextSegmentIndex], &scratch->bottomRing[segmentIndex], &scratch->bottomRing[nextSegmentIndex]);
    gte_rtpt();
    gte_stsxy3(&scratch->sxy1, &scratch->sxy2, &scratch->sxy3);
    gte_stflg(&scratch->projectionFlags);
}

/// Draws the blue shock ring with a raised bright rim and a wider black rim.
///
/// Borrows a composed `coord`. The 16-vertex XZ rims have radii `innerRadius`
/// and `innerRadius + 512`, at local Y -384 and 0, with signed-halfword wrap.
/// Bright RGB is (brightness >> 1, brightness >> 1, brightness), narrowed to bytes.
/// Projects through GsWSMATRIX and queues up to 16 additive Gouraud quads.
/// Only the final RTPT flags reject a segment; sorting uses its last SZ3/4.
/// Requires initialized scratch and packet space; retains no pointer.
static void _shelterB2PodBottomDrawShockRing(const GfxCoord* coord, s16 innerRadius, s16 brightness)
{
    enum {
        SHELTER_B2_POD_BOTTOM_SHOCK_RING_WIDTH = 512,
        SHELTER_B2_POD_BOTTOM_SHOCK_RING_LIFT  = 384
    };

    EffectBandScratch* scratch;
    SVECTOR*           outerVertex;
    POLY_G4*           quad;
    s32                segmentIndex;
    s32                angle;
    s16                raisedRadius;
    s16                outerRadius;
    u8                 red;
    u8                 green;
    u8                 blue;

    outerRadius = innerRadius + SHELTER_B2_POD_BOTTOM_SHOCK_RING_WIDTH;
    red         = brightness >> 1;
    green       = brightness >> 1;
    blue        = brightness;
    scratch     = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    raisedRadius = innerRadius;
    // Build the raised bright rim and ground-level dark rim before projection.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        angle                             = segmentIndex * SHELTER_B2_POD_BOTTOM_SIXTEENTH_TURN;
        scratch->topRing[segmentIndex].vx = (rsin(angle) * raisedRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        scratch->topRing[segmentIndex].vy = -SHELTER_B2_POD_BOTTOM_SHOCK_RING_LIFT;
        scratch->topRing[segmentIndex].vz = (rcos(angle) * raisedRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx    = (u16)scratch->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        scratch->topRing[segmentIndex].vy    = (u16)scratch->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        scratch->topRing[segmentIndex].vz    = (u16)scratch->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (rsin(angle) * outerRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        // This byte view stays within the complete scratch object.
        outerVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        outerVertex->vy = 0;
        outerVertex->vz = (rcos(angle) * outerRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        scratch->bottomRing[segmentIndex].vx = (u16)scratch->bottomRing[segmentIndex].vx + (u16)coord->workm.t[0];
        outerVertex->vy                      = (u16)outerVertex->vy + (u16)coord->workm.t[1];
        outerVertex->vz                      = (u16)outerVertex->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        _shelterB2PodBottomProjectShockRingSegment(scratch, segmentIndex);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, red, green, blue);
            setRGB1(quad, red, green, blue);
            setRGB2(quad, 0, 0, 0);
            setRGB3(quad, 0, 0, 0);
            quad->x0 = scratch->sxy0.vx;
            quad->y0 = scratch->sxy0.vy;
            quad->x1 = scratch->sxy1.vx;
            quad->y1 = scratch->sxy1.vy;
            quad->x2 = scratch->sxy2.vx;
            quad->y2 = scratch->sxy2.vy;
            quad->x3 = scratch->sxy3.vx;
            quad->y3 = scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->otz);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

void shelterB2PodBottomArcFlashTask(Task* task)
{
    enum {
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_INITIAL_BRIGHTNESS = 160,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_FADE_STEP          = 8,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_VISIBLE_MIN        = 9,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_RADIUS_STEP        = 128,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_LIFT_STEP          = 32,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_Y_STEP        = 48,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_WIDTH         = 256
    };

    EffectWork* work;
    GfxCoord*   coord;
    u16         previousAge;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        previousAge         = work->age;
        work->age           = previousAge + 1;
        switch (task->state) {
            case SHELTER_B2_POD_BOTTOM_EFFECT_INIT:
                gfxSetRotIdentity(&coord->coord);
                work->scale = SHELTER_B2_POD_BOTTOM_ARC_FLASH_INITIAL_BRIGHTNESS;
                task->state++;
                return;
            case SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE:
                if (work->scale < SHELTER_B2_POD_BOTTOM_ARC_FLASH_VISIBLE_MIN) {
                    break;
                }
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    work->scale  -= SHELTER_B2_POD_BOTTOM_ARC_FLASH_FADE_STEP;
                    work->angle  += SHELTER_B2_POD_BOTTOM_ARC_FLASH_RADIUS_STEP;
                    work->period -= SHELTER_B2_POD_BOTTOM_ARC_FLASH_LIFT_STEP;
                    work->step   += SHELTER_B2_POD_BOTTOM_ARC_FLASH_LIFT_STEP;
                } else {
                    work->age = previousAge;
                }
                _shelterB2PodBottomDrawArcFlashBand(work, coord, 0);
                _shelterB2PodBottomDrawArcFlashBand(work, coord, 1);
                _shelterB2PodBottomDrawArcFlashBand(work, coord, 2);
                rgb[0] = rgb[1] = work->scale;
                rgb[2]          = work->scale * 3 / 2;
                // Stack the glow rings by displacing the cache cumulatively.
                coord->workm.t[1] -= work->age * SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_Y_STEP;
                effectDrawOuterGlowBand(coord, (s16)(work->age << 6), SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_WIDTH, rgb);
                coord->workm.t[1] -= work->age * SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_Y_STEP;
                effectDrawOuterGlowBand(coord, (s16)(work->age << 7), SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_WIDTH, rgb);
                coord->workm.t[1] -= work->age * SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_Y_STEP;
                effectDrawOuterGlowBand(coord, (s16)(work->age * 0xC0), SHELTER_B2_POD_BOTTOM_ARC_FLASH_RING_WIDTH, rgb);
                effectDrawScreenTint(rgb, GPU_BLEND_ADD);
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Projects a band segment while preserving the first corner before RTPT.
///
/// Borrows a live, initialized scratch block with both 16-point rims. The GTE
/// projection matrices must be set, and `segmentIndex` is 0..15, wrapping at
/// the seam. Updates the four screen corners and only the final RTPT flags.
/// Leaves the final SZ3 in the GTE for depth sorting; retains no pointer.
/// `bandIndex` is 0..2 in the initialized phase table. Writes signed age plus
/// phase modulo six, narrowed to u16, to `textureFrame`; `nextSegmentIndex`
/// receives the wrapped neighbour. The phase lookup precedes the first FIFO read.
/// `scratch`, `work`, `bandIndex` and `segmentIndex` are evaluated repeatedly;
/// all arguments must be side-effect-free, and the outputs writable scalars.
/// Captures the initialized room phase table and the frame-count enum in its
/// drawer. This is a statement sequence: use only inside braces, as below.
#define SHELTER_B2_POD_BOTTOM_PROJECT_ARC_FLASH_SEGMENT(scratch, work, bandIndex, segmentIndex, textureFrame, nextSegmentIndex)                   \
    gte_ldv0(&(scratch)->topRing[(segmentIndex)]);                                                                                                \
    gte_rtps();                                                                                                                                   \
    (textureFrame) = (D_shelter_b2_pod_bottom_80188790[(bandIndex)][(segmentIndex)] + (work)->age) % SHELTER_B2_POD_BOTTOM_ARC_FLASH_FRAME_COUNT; \
    gte_stsxy(&(scratch)->sxy0);                                                                                                                  \
    (nextSegmentIndex) = ((segmentIndex) + 1) & (EFFECT_BAND_SEGMENT_COUNT - 1);                                                                  \
    gte_ldv3(&(scratch)->topRing[(nextSegmentIndex)], &(scratch)->bottomRing[(segmentIndex)], &(scratch)->bottomRing[(nextSegmentIndex)]);        \
    gte_rtpt();                                                                                                                                   \
    gte_stsxy3(&(scratch)->sxy1, &(scratch)->sxy2, &(scratch)->sxy3);                                                                             \
    gte_stflg(&(scratch)->projectionFlags);

/// Draws one of the arc flash's three animated, raised textured bands.
///
/// `bandIndex` is 0..2 in the shape and initialized per-segment phase tables.
/// Angle/step/period in `work` supply base radius/spread/lift, in coordinate
/// units with halfword wrapping; scale's low byte modulates all RGB channels.
/// Age plus each phase selects one of six 40-texel frames by signed remainder.
/// Normal ages are nonnegative. Borrows a composed `coord` and projects through
/// GsWSMATRIX; only the final RTPT flags reject a quad, sorted at its last SZ3/4.
/// Queues at most 16 semitransparent FT4s; needs scratch and packet capacity.
/// Retains no pointer.
static void _shelterB2PodBottomDrawArcFlashBand(const EffectWork* work, const GfxCoord* coord, s32 bandIndex)
{
    enum {
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_FRAME_COUNT  = 6,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_FRAME_TEXELS = 40,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_TOP_V        = 96,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_BOTTOM_V     = 135,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_TEXTURE_PAGE = 0x2A,
        SHELTER_B2_POD_BOTTOM_ARC_FLASH_CLUT         = 0x42C1
    };

    EffectBandScratch*     scratch;
    SVECTOR*               baseVertex;
    POLY_FT4*              quad;
    const EffectBandShape* shape;
    s32                    segmentIndex;
    s32                    nextSegmentIndex;
    s32                    angle;
    s32                    leftU;
    u16                    textureFrame;
    s16                    raisedRadius;
    s16                    baseRadius;
    u16                    lift;
    u16                    animatedLift;

    shape        = &D_shelter_b2_pod_bottom_80181C94[bandIndex];
    animatedLift = work->period;
    baseRadius   = work->angle;
    lift         = animatedLift + shape->lift;
    baseRadius  += shape->baseRadius;
    raisedRadius = baseRadius + work->step + shape->spread;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(EffectBandScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    // Wrap rotated local positions to halfwords, then translate both rims.
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        angle                             = segmentIndex * SHELTER_B2_POD_BOTTOM_SIXTEENTH_TURN;
        scratch->topRing[segmentIndex].vx = (rsin(angle) * raisedRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        scratch->topRing[segmentIndex].vy = -lift;
        scratch->topRing[segmentIndex].vz = (rcos(angle) * raisedRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->topRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->topRing[segmentIndex]);
        scratch->topRing[segmentIndex].vx    = (u16)scratch->topRing[segmentIndex].vx + (u16)coord->workm.t[0];
        scratch->topRing[segmentIndex].vy    = (u16)scratch->topRing[segmentIndex].vy + (u16)coord->workm.t[1];
        scratch->topRing[segmentIndex].vz    = (u16)scratch->topRing[segmentIndex].vz + (u16)coord->workm.t[2];
        scratch->bottomRing[segmentIndex].vx = (rsin(angle) * baseRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        // Address the base rim through the complete scratch object.
        baseVertex     = (SVECTOR*)((u8*)scratch + segmentIndex * sizeof(SVECTOR) + sizeof(scratch->topRing));
        baseVertex->vy = 0;
        baseVertex->vz = (rcos(angle) * baseRadius) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&scratch->bottomRing[segmentIndex]);
        gte_rtv0();
        gte_stsv(&scratch->bottomRing[segmentIndex]);
        scratch->bottomRing[segmentIndex].vx = (u16)scratch->bottomRing[segmentIndex].vx + (u16)coord->workm.t[0];
        baseVertex->vy                       = (u16)baseVertex->vy + (u16)coord->workm.t[1];
        baseVertex->vz                       = (u16)baseVertex->vz + (u16)coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    for (segmentIndex = 0; segmentIndex < EFFECT_BAND_SEGMENT_COUNT; segmentIndex++) {
        SHELTER_B2_POD_BOTTOM_PROJECT_ARC_FLASH_SEGMENT(scratch, work, bandIndex, segmentIndex, textureFrame, nextSegmentIndex);
        if (scratch->projectionFlags >= 0) {
            gte_stszotz(&scratch->otz);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyFT4(quad);
            // Read the low brightness byte directly; this is an intentional byte view.
            setRGB0(quad, *(const u8*)&work->scale, *(const u8*)&work->scale, *(const u8*)&work->scale);
            setSemiTrans(quad, 1);
            quad->tpage = SHELTER_B2_POD_BOTTOM_ARC_FLASH_TEXTURE_PAGE;
            quad->clut  = SHELTER_B2_POD_BOTTOM_ARC_FLASH_CLUT;
            leftU       = textureFrame * SHELTER_B2_POD_BOTTOM_ARC_FLASH_FRAME_TEXELS;
            setUV4(quad, leftU, SHELTER_B2_POD_BOTTOM_ARC_FLASH_TOP_V, leftU + SHELTER_B2_POD_BOTTOM_ARC_FLASH_FRAME_TEXELS - 1, SHELTER_B2_POD_BOTTOM_ARC_FLASH_TOP_V, leftU, SHELTER_B2_POD_BOTTOM_ARC_FLASH_BOTTOM_V, leftU + SHELTER_B2_POD_BOTTOM_ARC_FLASH_FRAME_TEXELS - 1, SHELTER_B2_POD_BOTTOM_ARC_FLASH_BOTTOM_V);
            quad->x0 = scratch->sxy0.vx;
            quad->y0 = scratch->sxy0.vy;
            quad->x1 = scratch->sxy1.vx;
            quad->y1 = scratch->sxy1.vy;
            quad->x2 = scratch->sxy2.vx;
            quad->y2 = scratch->sxy2.vy;
            quad->x3 = scratch->sxy3.vx;
            quad->y3 = scratch->sxy3.vy;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBandScratch);
}

#undef SHELTER_B2_POD_BOTTOM_PROJECT_ARC_FLASH_SEGMENT

void shelterB2PodBottomEnergyRingTask(Task* task)
{
    enum {
        SHELTER_B2_POD_BOTTOM_ENERGY_RING_BRIGHTNESS_SPAN = 192,
        SHELTER_B2_POD_BOTTOM_ENERGY_RING_INITIAL_RADIUS  = 128,
        SHELTER_B2_POD_BOTTOM_ENERGY_RING_MAX_BRIGHTNESS  = 255,
        SHELTER_B2_POD_BOTTOM_ENERGY_RING_FADE_STEP       = 16,
        SHELTER_B2_POD_BOTTOM_ENERGY_RING_VISIBLE_MIN     = 17,
        SHELTER_B2_POD_BOTTOM_ENERGY_RING_RING_WIDTH      = 128,
        SHELTER_B2_POD_BOTTOM_ENERGY_RING_CYCLE_UPDATES   = 10
    };

    EffectWork* work;
    GfxCoord*   coord;
    s32         nextBrightness;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case SHELTER_B2_POD_BOTTOM_EFFECT_INIT:
                gfxSetRotIdentity(&coord->coord);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = 0;
                work->angle         = SHELTER_B2_POD_BOTTOM_ENERGY_RING_INITIAL_RADIUS;
                work->step          = SHELTER_B2_POD_BOTTOM_ENERGY_RING_BRIGHTNESS_SPAN / task->spawnArg1.value;
                task->state         = SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE;
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->index         = (gRandomLcgState >> 16) % ARRAY_SIZE(D_shelter_b2_pod_bottom_80181CA8);
            // Pausing retains drawing without consuming the charge countdown.
            case SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    work->age--;
                    rgb[0] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][0];
                    rgb[1] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][1];
                    rgb[2] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][2];
                    effectDrawGouraudDisc(coord, work->angle, rgb);
                    effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                    effectDrawOuterGlowBand(coord, (s16)(task->spawnArg1.value % SHELTER_B2_POD_BOTTOM_ENERGY_RING_CYCLE_UPDATES * (work->scale << 2)), SHELTER_B2_POD_BOTTOM_ENERGY_RING_RING_WIDTH, rgb);
                    return;
                }
                nextBrightness = (u16)work->scale + (u16)work->step;
                work->scale    = nextBrightness;
                work->angle    = nextBrightness * 4 + SHELTER_B2_POD_BOTTOM_ENERGY_RING_INITIAL_RADIUS;
                task->spawnArg1.value--;
                rgb[0] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][0];
                rgb[1] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][1];
                rgb[2] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][2];
                effectDrawGouraudDisc(coord, work->angle, rgb);
                effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                effectDrawOuterGlowBand(coord, (s16)(task->spawnArg1.value % SHELTER_B2_POD_BOTTOM_ENERGY_RING_CYCLE_UPDATES * (work->scale << 2)), SHELTER_B2_POD_BOTTOM_ENERGY_RING_RING_WIDTH, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale  = SHELTER_B2_POD_BOTTOM_ENERGY_RING_MAX_BRIGHTNESS;
                    task->state  = SHELTER_B2_POD_BOTTOM_EFFECT_FADE;
                    work->period = 0x300;
                    work->step   = 0;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->index     = (gRandomLcgState >> 16) % ARRAY_SIZE(D_shelter_b2_pod_bottom_80181CA8);
                return;
            // The full-bright flash is drawn before each running fade step.
            case SHELTER_B2_POD_BOTTOM_EFFECT_FADE:
                if (work->scale < SHELTER_B2_POD_BOTTOM_ENERGY_RING_VISIBLE_MIN) {
                    break;
                }
                rgb[0] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][0];
                rgb[1] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][1];
                rgb[2] = work->scale >> D_shelter_b2_pod_bottom_80181CA8[work->index][2];
                effectDrawGouraudDisc(coord, work->angle, rgb);
                effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 2), rgb);
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->scale    -= SHELTER_B2_POD_BOTTOM_ENERGY_RING_FADE_STEP;
                    work->index     = (gRandomLcgState >> 16) % ARRAY_SIZE(D_shelter_b2_pod_bottom_80181CA8);
                }
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Initializes a Gouraud glow wedge's packet header and vertex colours.
///
/// Borrows one writable `POLY_G4`. Vertex 2 receives the supplied RGB bytes;
/// vertices 0, 1 and 3 are black. The light-beam caps place vertex 2 at their
/// projected centre and the black vertices on their circular rim.
/// Sets the G4 length and opaque command. Coordinates and the ordering-table
/// address are untouched; the caller supplies them and enables blending.
/// Retains no pointer.
static inline void _shelterB2PodBottomInitGlowWedge(POLY_G4* quad, u8 red, u8 green, u8 blue)
{
    setPolyG4(quad);
    setRGB0(quad, 0, 0, 0);
    setRGB1(quad, 0, 0, 0);
    setRGB2(quad, red, green, blue);
    setRGB3(quad, 0, 0, 0);
}

/// Initializes a starburst wedge, reading each borrowed RGB byte at its store.
///
/// `brightnessShift` is 0 for the inner layer or 1 for the outer layer/spikes.
/// The writable G4 gets its length, command, three black rim vertices and a
/// coloured vertex 2. Coordinates, linkage and blending belong to the caller.
/// Arguments are evaluated repeatedly and must be side-effect-free; captures
/// no caller identifiers. This is a statement sequence used inside loop braces.
#define SHELTER_B2_POD_BOTTOM_INIT_STARBURST_WEDGE(quad, rgb, brightnessShift)                                    \
    setPolyG4((quad));                                                                                            \
    setRGB0((quad), 0, 0, 0);                                                                                     \
    setRGB1((quad), 0, 0, 0);                                                                                     \
    setRGB2((quad), (rgb)[0] >> (brightnessShift), (rgb)[1] >> (brightnessShift), (rgb)[2] >> (brightnessShift)); \
    setRGB3((quad), 0, 0, 0);

/// Draws a two-layer eight-wedge glow with four longer spikes at its centre.
///
/// Borrows a composed coordinate and three RGB bytes for the call. Signed low
/// halfword `radiusScale` gives outer/inner pixel radii as scale*64/depth and
/// scale*8/depth, with depth SZ3/4 nonzero. The outer fan and long spikes are
/// half bright; the half-radius fan is full bright. Rim vertices are black.
/// A negative centre projection flag rejects all twenty additive G4 packets.
/// Requires initialized scratch and packet capacity; retains no pointer.
static void _shelterB2PodBottomDrawStarburst(const GfxCoord* coord, s32 radiusScale, const u8* rgb)
{
    enum {
        SHELTER_B2_POD_BOTTOM_STARBURST_OUTER_SCALE = 64,
        SHELTER_B2_POD_BOTTOM_STARBURST_INNER_SCALE = 8
    };

    EffectShapeScratch* scratch;
    POLY_G4*            quad;
    s32                 angle;

    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        gte_stszotz(&scratch->depth);
        scratch->extent.burst.outer = ((s16)radiusScale * SHELTER_B2_POD_BOTTOM_STARBURST_OUTER_SCALE) / scratch->depth;
        scratch->extent.burst.inner = ((s16)radiusScale * SHELTER_B2_POD_BOTTOM_STARBURST_INNER_SCALE) / scratch->depth;

        // Layer a half-bright outer fan over a full-bright half-radius fan.
        angle = 0;
        do {
            s32 halfStepAngle;
            s32 nextAngle;

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            SHELTER_B2_POD_BOTTOM_INIT_STARBURST_WEDGE(quad, rgb, 1);
            quad->x0      = scratch->screenX + ((scratch->extent.burst.outer * rsin(angle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            halfStepAngle = angle + SHELTER_B2_POD_BOTTOM_SIXTEENTH_TURN;
            quad->y0      = scratch->screenY + ((scratch->extent.burst.outer * rcos(angle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            quad->x1      = scratch->screenX + ((scratch->extent.burst.outer * rsin(halfStepAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            quad->y1      = scratch->screenY + ((scratch->extent.burst.outer * rcos(halfStepAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            nextAngle     = angle + SHELTER_B2_POD_BOTTOM_EIGHTH_TURN;
            quad->x2      = scratch->screenX;
            quad->y2      = scratch->screenY;
            quad->x3      = scratch->screenX + ((scratch->extent.burst.outer * rsin(nextAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            quad->y3      = scratch->screenY + ((scratch->extent.burst.outer * rcos(nextAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            SHELTER_B2_POD_BOTTOM_INIT_STARBURST_WEDGE(quad, rgb, 0);
            quad->x0 = scratch->screenX + ((scratch->extent.burst.outer * rsin(angle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT + 1));
            quad->y0 = scratch->screenY + ((scratch->extent.burst.outer * rcos(angle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT + 1));
            quad->x1 = scratch->screenX + ((scratch->extent.burst.outer * rsin(halfStepAngle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT + 1));
            quad->y1 = scratch->screenY + ((scratch->extent.burst.outer * rcos(halfStepAngle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT + 1));
            quad->x2 = scratch->screenX;
            quad->y2 = scratch->screenY;
            quad->x3 = scratch->screenX + ((scratch->extent.burst.outer * rsin(nextAngle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT + 1));
            quad->y3 = scratch->screenY + ((scratch->extent.burst.outer * rcos(nextAngle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT + 1));
            angle    = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
        } while (angle < SHELTER_B2_POD_BOTTOM_FULL_TURN);

        // Add four long spikes between the two inner-radius shoulders.
        angle = SHELTER_B2_POD_BOTTOM_EIGHTH_TURN;
        do {
            s32 nextAngle;
            s32 oppositeAngle;

            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            SHELTER_B2_POD_BOTTOM_INIT_STARBURST_WEDGE(quad, rgb, 1);
            quad->x0      = scratch->screenX + ((scratch->extent.burst.inner * rsin(angle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            nextAngle     = angle + SHELTER_B2_POD_BOTTOM_QUARTER_TURN;
            quad->y0      = scratch->screenY + ((scratch->extent.burst.inner * rcos(angle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            quad->x1      = scratch->screenX + ((scratch->extent.burst.outer * rsin(nextAngle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT - 1));
            quad->y1      = scratch->screenY + ((scratch->extent.burst.outer * rcos(nextAngle)) >> (SHELTER_B2_POD_BOTTOM_TRIG_SHIFT - 1));
            oppositeAngle = angle + SHELTER_B2_POD_BOTTOM_HALF_TURN;
            quad->x2      = scratch->screenX;
            quad->y2      = scratch->screenY;
            quad->x3      = scratch->screenX + ((scratch->extent.burst.inner * rsin(oppositeAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            quad->y3      = scratch->screenY + ((scratch->extent.burst.inner * rcos(oppositeAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
            angle         = nextAngle;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->depth);
        } while (angle < SHELTER_B2_POD_BOTTOM_FULL_TURN);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

#undef SHELTER_B2_POD_BOTTOM_INIT_STARBURST_WEDGE

void shelterB2PodBottomChargeBurstTask(Task* task)
{
    enum {
        SHELTER_B2_POD_BOTTOM_CHARGE_BURST_BRIGHTNESS_SPAN = 192,
        SHELTER_B2_POD_BOTTOM_CHARGE_BURST_INITIAL_RADIUS  = 128,
        SHELTER_B2_POD_BOTTOM_CHARGE_BURST_MAX_BRIGHTNESS  = 255,
        SHELTER_B2_POD_BOTTOM_CHARGE_BURST_FADE_STEP       = 16,
        SHELTER_B2_POD_BOTTOM_CHARGE_BURST_VISIBLE_MIN     = 17,
        SHELTER_B2_POD_BOTTOM_CHARGE_BURST_RING_WIDTH      = 128
    };

    EffectWork* work;
    GfxCoord*   coord;
    s32         bladeIndex;
    s32         nextBrightness;
    u8          rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        work->age++;
        switch (task->state) {
            case SHELTER_B2_POD_BOTTOM_EFFECT_INIT:
                gfxSetRotIdentity(&coord->coord);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->scale         = 0;
                work->angle         = SHELTER_B2_POD_BOTTOM_CHARGE_BURST_INITIAL_RADIUS;
                work->step          = SHELTER_B2_POD_BOTTOM_CHARGE_BURST_BRIGHTNESS_SPAN / task->spawnArg1.value;
                task->state         = SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE;
                for (bladeIndex = 0; bladeIndex < (s32)ARRAY_SIZE(D_shelter_b2_pod_bottom_801887F0); bladeIndex++) {
                    gRandomLcgState                              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_shelter_b2_pod_bottom_801887F0[bladeIndex] = (bladeIndex * SHELTER_B2_POD_BOTTOM_EIGHTH_TURN) + ((gRandomLcgState >> 16) & (SHELTER_B2_POD_BOTTOM_EIGHTH_TURN - 1));
                }
            // Pausing retains drawing without consuming the charge countdown.
            case SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE:
                if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                    work->age--;
                    rgb[0] = work->scale;
                    rgb[1] = work->scale;
                    rgb[2] = work->scale >> 1;
                    _shelterB2PodBottomDrawStarburst(coord, (s16)((u16)work->angle * 2), rgb);
                    effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 4), rgb);
                    effectDrawOuterGlowBand(coord, (s16)(task->spawnArg1.value * work->step * 16), SHELTER_B2_POD_BOTTOM_CHARGE_BURST_RING_WIDTH, rgb);
                    return;
                }
                nextBrightness = (u16)work->scale + (u16)work->step;
                work->scale    = nextBrightness;
                work->angle    = nextBrightness * 4 + SHELTER_B2_POD_BOTTOM_CHARGE_BURST_INITIAL_RADIUS;
                task->spawnArg1.value--;
                rgb[0] = work->scale;
                rgb[1] = work->scale;
                rgb[2] = work->scale >> 1;
                _shelterB2PodBottomDrawStarburst(coord, (s16)((u16)work->angle * 2), rgb);
                effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 4), rgb);
                effectDrawOuterGlowBand(coord, (s16)(task->spawnArg1.value * work->step * 16), SHELTER_B2_POD_BOTTOM_CHARGE_BURST_RING_WIDTH, rgb);
                if (task->spawnArg1.value == 0) {
                    work->scale  = SHELTER_B2_POD_BOTTOM_CHARGE_BURST_MAX_BRIGHTNESS;
                    task->state  = SHELTER_B2_POD_BOTTOM_EFFECT_FADE;
                    work->period = 0x300;
                    work->step   = 0;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->index     = (gRandomLcgState >> 16) % 18;
                return;
            // The full-bright flash is drawn before each running fade step.
            case SHELTER_B2_POD_BOTTOM_EFFECT_FADE:
                if (work->scale < SHELTER_B2_POD_BOTTOM_CHARGE_BURST_VISIBLE_MIN) {
                    break;
                }
                rgb[0] = work->scale;
                rgb[1] = work->scale;
                rgb[2] = work->scale >> 1;
                _shelterB2PodBottomDrawStarburst(coord, (s16)((u16)work->angle * 2), rgb);
                effectDrawGouraudDisc(coord, (s16)((u16)work->angle * 4), rgb);
                for (bladeIndex = 0; bladeIndex < (s32)ARRAY_SIZE(D_shelter_b2_pod_bottom_801887F0); bladeIndex++) {
                    _shelterB2PodBottomDrawBurstBlade(coord, (s16)((u16)work->angle * 2), D_shelter_b2_pod_bottom_801887F0[bladeIndex], rgb);
                }
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->scale    -= SHELTER_B2_POD_BOTTOM_CHARGE_BURST_FADE_STEP;
                    work->index     = (gRandomLcgState >> 16) % 18;
                }
                return;
            default:
                return;
        }
    }
    effectKillTask(work, task);
}

/// Draws a narrow additive blade from the projected centre toward `bladeAngle`.
///
/// Borrows a composed coordinate and three RGB bytes. Signed low halfwords of
/// `radiusScale` and `bladeAngle` give pixel length scale*128/(SZ3/4) and the
/// centre angle in 4096-per-turn units. The black outer corners are at angle
/// +/-32; the centre has the supplied RGB. Projection depth must be nonzero.
/// A negative GTE flag rejects the triangle. Needs scratch and packet space;
/// retains no pointer.
static void _shelterB2PodBottomDrawBurstBlade(const GfxCoord* coord, s32 radiusScale, s32 bladeAngle, const u8* rgb)
{
    enum {
        SHELTER_B2_POD_BOTTOM_BURST_BLADE_RADIUS_SCALE = 128,
        SHELTER_B2_POD_BOTTOM_BURST_BLADE_HALF_ANGLE   = 32
    };

    EffectCentreScratch* scratch;
    POLY_G3*             triangle;
    s32                  upperAngle;
    s32                  lowerAngle;

    scratch                = SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    scratch->worldPoint.vx = coord->workm.t[0];
    scratch->worldPoint.vy = coord->workm.t[1];
    scratch->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->worldPoint);
    gte_rtps();
    gte_stsxy(&scratch->screenX);
    gte_stflg(&scratch->projectionFlags);
    if (scratch->projectionFlags >= 0) {
        triangle       = gGpuPrimCursor;
        gGpuPrimCursor = triangle + 1;
        setPolyG3(triangle);
        gte_stszotz(&scratch->depth);
        setRGB0(triangle, rgb[0], rgb[1], rgb[2]);
        setRGB1(triangle, 0, 0, 0);
        setRGB2(triangle, 0, 0, 0);
        scratch->screenExtent = ((s16)radiusScale * SHELTER_B2_POD_BOTTOM_BURST_BLADE_RADIUS_SCALE) / scratch->depth;
        upperAngle            = (s16)bladeAngle;
        lowerAngle            = upperAngle - SHELTER_B2_POD_BOTTOM_BURST_BLADE_HALF_ANGLE;
        triangle->x0          = scratch->screenX;
        triangle->y0          = scratch->screenY;
        triangle->x1          = scratch->screenX + ((scratch->screenExtent * rsin(lowerAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
        triangle->y1          = scratch->screenY + ((scratch->screenExtent * rcos(lowerAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
        upperAngle           += SHELTER_B2_POD_BOTTOM_BURST_BLADE_HALF_ANGLE;
        triangle->x2          = scratch->screenX + ((scratch->screenExtent * rsin(upperAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
        triangle->y2          = scratch->screenY + ((scratch->screenExtent * rcos(upperAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                triangle);
        gpuSetPrimitiveBlendMode(triangle, GPU_BLEND_ADD, scratch->depth);
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

void shelterB2PodBottomLightBeamTask(Task* task)
{
    enum {
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RADIUS_SCALE       = 256,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_LIFETIME           = 16,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_FADE_START         = 8,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_FULL_BRIGHTNESS    = 16,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_WHITE_NIBBLES      = 0xCCC,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RANDOM_NIBBLE_MASK = 0x777
    };

    EffectWork* work;
    GfxCoord*   coord;
    u32         random;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            _shelterB2PodBottomDrawLightBeam(coord, SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RADIUS_SCALE, (random >> 16) & SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RANDOM_NIBBLE_MASK, SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_FULL_BRIGHTNESS);
            return;
        }
        effectKillTask(work, task);
        return;
    }
    // Move the local transform; drawing still reads the existing composed cache.
    work->age++;
    coord->coord.t[1]  += task->spawnArg1.value;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (work->age < SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_FADE_START) {
        _shelterB2PodBottomDrawLightBeam(coord, SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RADIUS_SCALE, SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_WHITE_NIBBLES, SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_FULL_BRIGHTNESS);
        return;
    }
    _shelterB2PodBottomDrawLightBeam(coord, SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RADIUS_SCALE, SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_WHITE_NIBBLES, (u16)((SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_LIFETIME - work->age) * 2));
    if (work->age >= SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_LIFETIME) {
        effectKillTask(work, task);
    }
}

/// Draws a flickering capsule along the coordinate's negative local Y axis.
///
/// Borrows a composed `coord`; the second end is -16*`radiusScale` units from
/// its origin, rotated and narrowed to signed halfwords before translation.
/// Both projected depths SZ3/4 must be nonzero. Two layers have pixel radii
/// scale*64/depth and scale*128/depth. Each queues two cap pairs and two joining
/// quads: twelve additive G4s total, plus their blend commands. Caps sort at
/// their own end depth; sides sort at the second end's depth.
/// `packedColor` bits 8..11/4..7/0..3 are RGB nibbles multiplied by `brightness`;
/// odd display frames add 16 to each channel, wrapping to bytes. Either end's
/// negative GTE flag rejects the whole beam. Needs scratch and packet space.
static void _shelterB2PodBottomDrawLightBeam(const GfxCoord* coord, s16 radiusScale, u16 packedColor, u16 brightness)
{
    enum {
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_LENGTH_SHIFT      = 4,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RADIUS_SHIFT      = 6,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_LAYERS            = 2,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_FLICKER_SHIFT     = 4,
        SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_COLOR_NIBBLE_MASK = 0xF
    };

    RoomBeamScratch* scratch;
    POLY_G4*         quad;
    s32              radiusPass;
    u8               red;
    u8               green;
    u8               blue;
    s32              sweepLimit;
    s32              baseAngle;
    s32              scaledRadius;
    s32              sweepAngle;
    s32              nextSweepAngle;
    s32              sideAngle;
    s32              flicker;

    // Rotate the beam offset, wrap to halfwords, then project both ends.
    scratch            = SCRATCH_STACK_RESERVE_BLOCK(RoomBeamScratch);
    scratch->point1.vy = -(radiusScale << SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_LENGTH_SHIFT);
    scratch->point1.vx = 0;
    scratch->point1.vz = 0;
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(&scratch->point1);
    gte_rtv0();
    gte_stsv(&scratch->point1);
    scratch->point0.vx  = coord->workm.t[0];
    scratch->point0.vy  = coord->workm.t[1];
    scratch->point0.vz  = coord->workm.t[2];
    scratch->point1.vx += scratch->point0.vx;
    scratch->point1.vy += scratch->point0.vy;
    scratch->point1.vz += scratch->point0.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->point0);
    gte_rtps();
    gte_stsxy(&scratch->pair.sx0);
    gte_stflg(&scratch->pair.flag);
    if (scratch->pair.flag >= 0) {
        gte_stszotz(&scratch->pair.otz0);
        gte_ldv0(&scratch->point1);
        gte_rtps();
        gte_stsxy(&scratch->pair.sx1);
        gte_stflg(&scratch->pair.flag);
        gte_stszotz(&scratch->pair.otz1);
        if (scratch->pair.flag >= 0) {
            flicker = ((u8)gDisplayState.animFrame & 1) << SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_FLICKER_SHIFT;
            red     = brightness * ((packedColor >> 8) & SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_COLOR_NIBBLE_MASK) + flicker;
            green   = brightness * ((packedColor >> 4) & SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_COLOR_NIBBLE_MASK) + flicker;
            blue    = brightness * (packedColor & SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_COLOR_NIBBLE_MASK) + flicker;
            for (radiusPass = 1; radiusPass < SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_LAYERS + 1; radiusPass++) {
                scaledRadius          = radiusScale * (radiusPass << SHELTER_B2_POD_BOTTOM_LIGHT_BEAM_RADIUS_SHIFT);
                scratch->pair.radius0 = scaledRadius / scratch->pair.otz0;
                scratch->pair.radius1 = scaledRadius / scratch->pair.otz1;
                sweepAngle            = (s16)ratan2((s16)scratch->pair.sy1 - (s16)scratch->pair.sy0, (s16)scratch->pair.sx0 - (s16)scratch->pair.sx1);
                if (sweepAngle < sweepAngle + SHELTER_B2_POD_BOTTOM_HALF_TURN) {
                    baseAngle  = sweepAngle;
                    sweepLimit = sweepAngle + SHELTER_B2_POD_BOTTOM_HALF_TURN;
                    // Each half-turn sweep step queues two cap wedges and one side.
                    do {
                        quad           = gGpuPrimCursor;
                        gGpuPrimCursor = quad + 1;
                        _shelterB2PodBottomInitGlowWedge(quad, red, green, blue);
                        quad->x0 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + SHELTER_B2_POD_BOTTOM_HALF_TURN)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y0 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + SHELTER_B2_POD_BOTTOM_HALF_TURN)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + (SHELTER_B2_POD_BOTTOM_HALF_TURN + SHELTER_B2_POD_BOTTOM_EIGHTH_TURN))) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + (SHELTER_B2_POD_BOTTOM_HALF_TURN + SHELTER_B2_POD_BOTTOM_EIGHTH_TURN))) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->x2 = scratch->pair.sx1;
                        quad->y2 = scratch->pair.sy1;
                        quad->x3 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sweepAngle + (SHELTER_B2_POD_BOTTOM_HALF_TURN + SHELTER_B2_POD_BOTTOM_QUARTER_TURN))) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y3 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sweepAngle + (SHELTER_B2_POD_BOTTOM_HALF_TURN + SHELTER_B2_POD_BOTTOM_QUARTER_TURN))) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);

                        quad           = gGpuPrimCursor;
                        gGpuPrimCursor = quad + 1;
                        _shelterB2PodBottomInitGlowWedge(quad, red, green, blue);
                        quad->x0       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y0       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->x1       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sweepAngle + SHELTER_B2_POD_BOTTOM_EIGHTH_TURN)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y1       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sweepAngle + SHELTER_B2_POD_BOTTOM_EIGHTH_TURN)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        nextSweepAngle = sweepAngle + SHELTER_B2_POD_BOTTOM_QUARTER_TURN;
                        quad->x2       = scratch->pair.sx0;
                        quad->y2       = scratch->pair.sy0;
                        quad->x3       = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(nextSweepAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y3       = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(nextSweepAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz0);

                        quad           = gGpuPrimCursor;
                        sideAngle      = baseAngle + (sweepAngle - baseAngle) * 2;
                        gGpuPrimCursor = quad + 1;
                        setPolyG4(quad);
                        setRGB0(quad, 0, 0, 0);
                        setRGB1(quad, 0, 0, 0);
                        setRGB2(quad, red, green, blue);
                        setRGB3(quad, red, green, blue);
                        quad->x0 = scratch->pair.sx0 + ((scratch->pair.radius0 * rsin(sideAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y0 = scratch->pair.sy0 + ((scratch->pair.radius0 * rcos(sideAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->x1 = scratch->pair.sx1 + ((scratch->pair.radius1 * rsin(sideAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->y1 = scratch->pair.sy1 + ((scratch->pair.radius1 * rcos(sideAngle)) >> SHELTER_B2_POD_BOTTOM_TRIG_SHIFT);
                        quad->x2 = scratch->pair.sx0;
                        quad->y2 = scratch->pair.sy0;
                        quad->x3 = scratch->pair.sx1;
                        quad->y3 = scratch->pair.sy1;
                        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->pair.otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                                quad);
                        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, scratch->pair.otz1);
                        sweepAngle = nextSweepAngle;
                    } while (sweepAngle < sweepLimit);
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
        effectSpawn(EFFECT_FLASH_BURST, coord, 0x10300, 0);
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        if ((rnd >> 16) & 1) {
            effectSpawn(EFFECT_SPARK_FADE, coord, 0x10300, 0);
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
        effectSpawn(EFFECT_RISING_ENERGY_SPARK, coord, 0x8600, 0);
        rnd             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        gRandomLcgState = rnd;
        if (!((rnd >> 16) & 1)) {
            effectSpawn(EFFECT_RISING_ENERGY_SPARK, coord, 0x8600, 0);
        }
    }
}

void shelterB2PodBottomShockRingTask(Task* task)
{
    enum {
        SHELTER_B2_POD_BOTTOM_SHOCK_RING_INITIAL_RADIUS     = 384,
        SHELTER_B2_POD_BOTTOM_SHOCK_RING_INITIAL_BRIGHTNESS = 192,
        SHELTER_B2_POD_BOTTOM_SHOCK_RING_RADIUS_STEP        = 96,
        SHELTER_B2_POD_BOTTOM_SHOCK_RING_FADE_STEP          = 24
    };

    EffectWork* work;
    GfxCoord*   coord;
    s16         nextBrightness;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        effectKillTask(work, task);
        return;
    }
    work->age++;
    switch (task->state) {
        case SHELTER_B2_POD_BOTTOM_EFFECT_INIT:
            gfxRotMatrixX(&coord->coord, task->spawnArg1.value, GRAPHICS_ROTATION_COMPOSE);
            work->scale = SHELTER_B2_POD_BOTTOM_SHOCK_RING_INITIAL_BRIGHTNESS;
            work->angle = SHELTER_B2_POD_BOTTOM_SHOCK_RING_INITIAL_RADIUS;
            task->state = SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE;
        // Draw at the current radius/colour before advancing running ramps.
        case SHELTER_B2_POD_BOTTOM_EFFECT_ACTIVE:
            _shelterB2PodBottomDrawShockRing(coord, work->angle, work->scale);
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->angle   += SHELTER_B2_POD_BOTTOM_SHOCK_RING_RADIUS_STEP;
                nextBrightness = work->scale - SHELTER_B2_POD_BOTTOM_SHOCK_RING_FADE_STEP;
                work->scale    = nextBrightness;
                if (nextBrightness < SHELTER_B2_POD_BOTTOM_SHOCK_RING_FADE_STEP) {
                    effectKillTask(work, task);
                }
            } else {
                work->age--;
            }
            break;
    }
}
