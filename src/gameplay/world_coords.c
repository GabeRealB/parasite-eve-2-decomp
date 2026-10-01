#include "gameplay/world_coords.h"

#include <psyq/sys/types.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/enemy.h"
#include "geometry.h"
#include "item_menu.h"
#include "item_pickup.h"
#include "item_use.h"
#include "gameplay/light.h"
#include "gameplay/lighting_work.h"
#include "lighting_work.h"
#include "loading.h"
#include "gameplay/room.h"
#include "gameplay/scene.h"
#include "gameplay/weapon_data.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "gameplay/damage.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"

/// 12-byte ranked slot inserted by `Gp_InsertRankedSlot`. That helper walks a
/// 4-entry table (indices 0..3) from `arg4` toward 0 and keeps slots in
/// descending `field_4` order. A non-positive key is ignored. When the
/// new key is larger than slot `arg4`, that slot is copied to `arg4+1`
/// (if `arg4 < 3`) and the search recurses; otherwise the record is
/// stored at `arg4+1` when there is room.
typedef struct _GpRec12 {
    /* 0x0 */ s32   field_0; // payload from arg2
    /* 0x4 */ s32   field_4; // descending sort key (arg1)
    /* 0x8 */ void* field_8; // light selected by the kind in field_0
} GpRec12;
STATIC_ASSERT_SIZEOF(GpRec12, 0xC);

/// Three packed `SVECTOR3`s filled by `Gp_FillSVec3x3`. Each vector's
/// components are set to the same s16 argument.
typedef struct _GpSVec3x3 {
    /* 0x00 */ SVECTOR3 field_0;
    /* 0x06 */ SVECTOR3 field_6;
    /* 0x0C */ SVECTOR3 field_C;
} GpSVec3x3;
STATIC_ASSERT_SIZEOF(GpSVec3x3, 0x12);

/// Nearest room light selected by `func_800D78A4`. `kind` is -1 when no
/// light is selected, 1 for a point light, or 2 for a spot light.
/// `light` points to the selected light; `field_4` is cleared.
typedef struct _GpNearestLight {
    /* 0x00 */ s32              kind;
    /* 0x04 */ s32              field_4;
    /* 0x08 */ WorldCoordLight* light;
} GpNearestLight;
STATIC_ASSERT_SIZEOF(GpNearestLight, 0xC);

/// Room-light capture block owned by another overlay (imported at
/// `D_80760618`). `func_800D7A9C` fills the four `field_30` entries with the
/// per-channel light rows while `field_1` is set, and `func_800D78A4` writes
/// the nearest light selection into `field_24`. `Gp_DebugPanTask` raises
/// `field_1` around that pair so the capture happens, then clears it.
typedef struct _GpLightCapture {
    /* 0x00 */ byte           pad_0[0x1];
    /* 0x01 */ s8             field_1;
    /* 0x02 */ byte           pad_2[0x22];
    /* 0x24 */ GpNearestLight field_24;
    /* 0x30 */ GpRec12        field_30[4];
} GpLightCapture;
STATIC_ASSERT_SIZEOF(GpLightCapture, 0x60);

/// 0x20-byte scratch from the scratch stack used by `Gp_LightFalloff` /
/// `Gp_LightPoint` / `Gp_LightPointRoom`.
/// `vec` is the halved local position (`Gp_LightFalloff`) or the halved
/// world position less a world `VECTOR3` (`Gp_LightPoint` / `Gp_LightPointRoom`).
/// `distSq` is `vx²+vy²+vz²`. `outerSq` / `innerSq` are `(radius²) >> 2`
/// from `outer` / `inner` (`Gp_LightPointRoom` first stores
/// `outer / 2` in `outerSq` for the `|dx|` / `|dz|` test). `scale`
/// is 0, `0x1000`, or the 12-fractional-bit falloff copied to the light's attenuation.
typedef struct _GpAttnScratch {
    /* 0x00 */ VECTOR vec;
    /* 0x10 */ s32    distSq;
    /* 0x14 */ s32    outerSq;
    /* 0x18 */ s32    innerSq;
    /* 0x1C */ s32    scale;
} GpAttnScratch;
STATIC_ASSERT_SIZEOF(GpAttnScratch, 0x20);

/// 0x2C-byte scratch from the scratch stack used by `Gp_LightCone`.
/// `vec` is the halved `field_24.t -` world `VECTOR3`. `dir` is the
/// `Gfx_NormalizeLightDir` result at `head - 0x1C`. `distSq` / `outerSq`
/// / `innerSq` / `scale` match `GpAttnScratch`. `cosAng` is
/// `-(dir · matrix column 2) >> 12`, compared with `rcos` of half
/// `WorldCoordSpotLight.angle`.
typedef struct _GpSpotScratch {
    /* 0x00 */ VECTOR  vec;
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ u32     distSq;
    /* 0x1C */ u32     outerSq;
    /* 0x20 */ u32     innerSq;
    /* 0x24 */ u32     scale;
    /* 0x28 */ s32     cosAng;
} GpSpotScratch;
STATIC_ASSERT_SIZEOF(GpSpotScratch, 0x2C);

/// Scratch workspace and ranked-slot view used by `func_800D7A9C`.
typedef struct {
    /* 0x00 */ MATRIX  mtx;
    /* 0x20 */ s32     intensity;
    /* 0x24 */ VECTOR  pos;
    /* 0x34 */ SVECTOR local;
    /* 0x3C */ byte    pad_3C[0x10];
    /* 0x4C */ GpRec12 slots[4];
} GpLightSolveScratch;
STATIC_ASSERT_SIZEOF(GpLightSolveScratch, 0x7C);

typedef struct {
    /* 0x00 */ byte  pad[0x4C];
    /* 0x4C */ s32   field_0;
    /* 0x50 */ s32   field_4;
    /* 0x54 */ void* field_8;
} GpSolveSlotView;
STATIC_ASSERT_SIZEOF(GpSolveSlotView, 0x58);

/// 0x1C-byte scratch from the scratch stack used by `func_800D9794` /
/// `func_800D98C4` / `func_800D9A30`. `in` is the direction
/// `func_800D98C4` / `func_800D9A30` feed to `Gfx_NormalizeLightDir`.
/// `dir` is that output (then overwritten by the GPF-scaled color).
/// `scale` holds the light's attenuation loaded into IR0.
typedef struct _GpLightScratch {
    /* 0x00 */ VECTOR  in;
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ s32     scale;
} GpLightScratch;
STATIC_ASSERT_SIZEOF(GpLightScratch, 0x1C);

/// 0x3C-byte scratch from the scratch stack used by `func_800D759C`.
/// `in` is the light's negated local position fed to `Gfx_NormalizeLightDir`. `dir` is
/// that output, then the view-rotated copy, then the GPF-scaled color.
/// `mtx` is `Transpose(gGfxViewCoord.workm) * parent->workm` (rotation only).
/// `scale` holds the light's attenuation loaded into IR0.
typedef struct _GpViewLightScratch {
    /* 0x00 */ VECTOR  in;
    /* 0x10 */ SVECTOR dir;
    /* 0x18 */ MATRIX  mtx;
    /* 0x38 */ s32     scale;
} GpViewLightScratch;
STATIC_ASSERT_SIZEOF(GpViewLightScratch, 0x3C);

/// Lighting override copied as a vector, then consumed component by component.
/// The fourth halfword preserves the SVECTOR's unused final member.
typedef union GpLightScaleOverride {
    SVECTOR vector;
    u16     components[4];
} GpLightScaleOverride;
STATIC_ASSERT_SIZEOF(GpLightScaleOverride, 8);

/* Define BSS before API headers to preserve first-declaration order. */
GpCoord64 Gp_RoomCoords[8];

u8 Gp_OverrideVec2Flag;

GpLightScaleOverride Gp_OverrideVec2;

struct WorldTargetNode* D_80115260;

s32 D_80115264;

#include "world_coords.h"

/// Overlay import: pointer to the room-light capture block written by
/// `func_800D7A9C` / `func_800D78A4`.
extern GpLightCapture* D_80760618;

/// Re-evaluates each lit transient light slot against the view.
static inline void _gpUpdateRoomCoordSlots(void);

static s32 Gp_LightPointRoom(WorldCoordPointLight* light, VECTOR3* pos);

static s32 Gp_LightPoint(WorldCoordPointLight* light, VECTOR3* pos);

static s32 Gp_LightCone(WorldCoordSpotLight* spot, VECTOR3* pos);

static void func_800D759C(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

/// Selects the nearest point or cone light to world position `arg0`, using
/// squared distance after halving each coordinate difference. Initializes
/// `arg1` to no selection even when `Gp_GetRoomCoordSet` returns 0.
static void func_800D78A4(VECTOR* arg0, GpNearestLight* arg1);

static __inline__ void solve_func_800D9794(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static __inline__ void solve_func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static __inline__ void solve_func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static __inline__ s32 solve_luma(WorldCoordLight* arg0);

static __inline__ void solve_rank(GpRec12* slots, s32 val, s32 kind, void* obj, GpRec12* last);

static __inline__ void solve_rank0(GpRec12* slots, s32 val, s32 kind, void* obj, GpLightSolveScratch* block);

/// Fills a light colour matrix so all three lights share one colour: every
/// column of the red, green and blue rows gets `r`, `g` and `b`.
static inline void _gpSetColorMtx(MATRIX* mtx, s16 r, s16 g, s16 b);

static void Gp_DebugPanTask(Task* arg0);

/// Remaps a 3x3 color matrix (`MATRIX.m`) from lighting mode `arg2`
/// (`colorMode` bits 0-1, or bits 2-3 when blending). Weighted mode
/// collapses RGB as (7,6,3)/33 then *4/*2/*1. Black zeros the matrix. Tint
/// fills 0x180/0x100/0x100. Default remaps to *3/*1/*3 when
/// `reactionFlags` has damage over time set. `ENEMY_COLOR_HIT_FLASH` with
/// `spawnState == 0` applies a `rsin(gDisplayState.loopCount << 6)` flicker
/// and clears the bit.
static void Gp_RemapActorColor(Enemy* arg0, MATRIX* arg1, s32 arg2);

static void Gp_LightFalloff(WorldCoordPointLight* light);

static WorldCoordRoomAmbientEntry* Gp_GetRoomBound(GameLocationKey* arg0);

static s32 Gp_CountRoomCoords(void);

static GpRoomCoordSet* Gp_GetRoomCoordSet(GameLocationKey* arg0);

static s32 Gp_GetObjLuma(WorldCoordLight* arg0);

/// World X of the object's position.
static s32 Gp_GetObjTransX(GfxCoord* coord);

static void func_800D9794(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static void func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static void func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

void Gp_InsertRankedSlot(GpRec12* arg0, s32 arg1, s32 arg2, void* arg3, s32 arg4);

static void Gp_FillSVec3x3(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3);

static GpRoomCoordRec* Gp_GetRoomCoordRec(GameLocationKey* arg0);

static void Gp_CopyDefaultBound(WorldCoordRoomAmbientEntry* ambientEntry);

static void Gp_BindDefaultMtx(Task* arg0);

static __inline__ void project_slot(s32* sxy, GpSlot70* slot);

static __inline__ void Gp_ObjWorldPosInline(WorldCollisionBody* obj, VECTOR* pos);

/// The same lookup as `Gp_GetIdParam0`, returned at the tables' own width.
static inline u16 _gpIdParam0(s32 id);

/// Returns 1 if item `arg0` cannot be used, 0 if it can.
/// `arg1` supplies `field_2` (capacity) for ammo ids 0xA0–0xBF.

/// Re-evaluates each lit transient light slot against the view.
static inline void _gpUpdateRoomCoordSlots(void)
{
    GpCoord64* slot;
    s32        i;

    slot = Gp_RoomCoords;
    for (i = 0; i < 8; i++, slot++) {
        if (slot->framesLeft != 0) {
            Gp_UpdateCoordEx(&slot->light.head.transform.coord, &gGfxViewCoord);
        }
    }
}

/// First-run init plus per-frame update of the current room's `GpRoomCoordSet`
/// coordinate arrays (parented to `gGfxViewCoord`) and the `Gp_RoomCoords` slots.
/// Kills `arg0` when `Gp_GetRoomCoordSet` returns 0.
void Gp_UpdateRoomCoords(Task* task)
{
    GpRoomCoordSet*       set;
    SVECTOR*              vec;
    WorldCoordLight*      light;
    WorldCoordPointLight* point;
    WorldCoordSpotLight*  spot;
    GfxCoord*             coord;
    s32                   i;
    s32                   j;

    set = Gp_GetRoomCoordSet(&gGameSession->location.loc);
    if (set == NULL) {
        taskKill(task);
        return;
    }

    vec = SCRATCH_STACK_RESERVE_BYTES(0x1C);
    if (task->state == 0) {
        point = set->arr60;
        for (i = 0; i < set->n60; i++, point++) {
            coord               = &point->head.transform.coord;
            coord->parent       = &gGfxViewCoord;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }

        spot = set->arr6C;
        for (i = 0; i < set->n6C; i++, spot++) {
            coord         = &spot->head.transform.coord;
            coord->parent = &gGfxViewCoord;
            // Aim the local Z column along the cone axis. The translation stays.
            if (spot->axis.vy != 0 || spot->axis.vz != 0) {
                vec->vx = 0;
                vec->vy = -spot->axis.vz;
                vec->vz = spot->axis.vy;
            } else {
                vec->vx = spot->axis.vy;
                vec->vy = -spot->axis.vx;
                vec->vz = 0;
            }
            Gfx_OrthonormalBasis(&coord->coord, &spot->axis, vec);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }

        if (set->n58 > 0) {
            WorldCoordLight* dir;

            dir = set->arr58;
            for (i = 0; i < set->n58; i++, dir++) {
                coord               = &dir->transform.coord;
                coord->parent       = &gGfxViewCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }

        for (j = 0; j < 8; j++) {
            Gp_RoomCoords[j].framesLeft = 0;
            coord                       = &Gp_RoomCoords[j].light.head.transform.coord;
            coord->parent               = &gGfxViewCoord;
        }

        task->state++;
    }

    Gp_UpdateCoord(&gGfxViewCoord);

    _gpUpdateRoomCoordSlots();

    point = set->arr60;
    for (i = 0; i < set->n60; i++, point++) {
        coord = &point->head.transform.coord;
        Gp_UpdateCoordEx(coord, &gGfxViewCoord);
    }

    spot = set->arr6C;
    for (i = 0; i < set->n6C; i++, spot++) {
        coord = &spot->head.transform.coord;
        Gp_UpdateCoordEx(coord, &gGfxViewCoord);
    }

    if (set->n58 > 0) {
        light = set->arr58;
        for (i = 0; i < set->n58; i++, light++) {
            coord = &light->transform.coord;
            Gp_UpdateCoordEx(coord, &gGfxViewCoord);
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

static s32 Gp_LightPointRoom(WorldCoordPointLight* light, VECTOR3* pos)
{
    WorldCoordLight* base;
    GpAttnScratch*   block;
    s32              result;
    s32              tooFar;
    s16              viewId;

    base   = &light->head;
    viewId = base->transform.lighting.viewId;
    if (viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS && gGameSession->location.loc.view != viewId) {
        return 0;
    }
    block          = SCRATCH_STACK_RESERVE_BLOCK(GpAttnScratch);
    block->vec.vx  = (base->transform.lighting.composed.t[0] - pos->vx) >> 1;
    block->vec.vy  = (base->transform.lighting.composed.t[1] - pos->vy) >> 1;
    block->vec.vz  = (base->transform.lighting.composed.t[2] - pos->vz) >> 1;
    block->outerSq = light->outer >> 1;
    block->scale   = 0;
    if (block->vec.vx < 0) {
        block->vec.vx = -block->vec.vx;
    }
    if (block->vec.vz < 0) {
        block->vec.vz = -block->vec.vz;
    }
    // Rejects on the X and Z extents alone before paying for the squares.
    tooFar = (u32)block->vec.vx > (u32)block->outerSq;
    if (!tooFar) {
        tooFar = (u32)block->vec.vz > (u32)block->outerSq;
        if (!tooFar) {
            block->outerSq = (light->outer * light->outer) >> 2;
            block->distSq  = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
            tooFar         = (u32)block->outerSq < (u32)block->distSq;
        }
    }
    if (tooFar) {
        result = 0;
    } else {
        block->innerSq = (light->inner * light->inner) >> 2;
        result         = ((light->head.color.r * 8 + light->head.color.g * 6 + light->head.color.b * 2) >> 8) + 0xF00;
        block->scale   = ONE;
        if ((u32)block->distSq > (u32)block->innerSq) {
            block->outerSq -= block->innerSq;
            block->distSq  -= block->innerSq;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                result       = (u32)(block->scale * result) >> 12;
            }
        }
    }
    base->transform.lighting.attenuation = block->scale;
    SCRATCH_STACK_RELEASE_BLOCK(GpAttnScratch);
    return result;
}

static s32 Gp_LightPoint(WorldCoordPointLight* light, VECTOR3* pos)
{
    GpAttnScratch*   block;
    s32              result;
    WorldCoordLight* base;

    base           = &light->head;
    result         = 0;
    block          = SCRATCH_STACK_RESERVE_BLOCK(GpAttnScratch);
    block->vec.vx  = (base->transform.lighting.composed.t[0] - pos->vx) >> 1;
    block->vec.vy  = (base->transform.lighting.composed.t[1] - pos->vy) >> 1;
    block->vec.vz  = (base->transform.lighting.composed.t[2] - pos->vz) >> 1;
    block->distSq  = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->outerSq = (light->outer * light->outer) >> 2;
    block->scale   = 0;
    if ((u32)block->outerSq >= (u32)block->distSq) {
        block->innerSq = (light->inner * light->inner) >> 2;
        result         = ((light->head.color.r * 8 + light->head.color.g * 6 + light->head.color.b * 2) >> 8) + 0xF00;
        block->scale   = ONE;
        if ((u32)block->distSq > (u32)block->innerSq) {
            block->outerSq -= block->innerSq;
            block->distSq  -= block->innerSq;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                result       = (u32)(block->scale * result) >> 12;
            }
        }
    }
    base->transform.lighting.attenuation = block->scale;
    SCRATCH_STACK_RELEASE_BLOCK(GpAttnScratch);
    return result;
}

static s32 Gp_LightCone(WorldCoordSpotLight* spot, VECTOR3* pos)
{
    WorldCoordLight* light;
    GpSpotScratch*   block;
    s32              result;

    light  = &spot->head;
    result = 0;
    if (light->transform.lighting.viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS) {
        if (gGameSession->location.loc.view != light->transform.lighting.viewId) {
            return result;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(GpSpotScratch);
    block          = SCRATCH_STACK_CURSOR(GpSpotScratch);
    block->vec.vx  = (light->transform.lighting.composed.t[0] - pos->vx) >> 1;
    block->vec.vy  = (light->transform.lighting.composed.t[1] - pos->vy) >> 1;
    block->vec.vz  = (light->transform.lighting.composed.t[2] - pos->vz) >> 1;
    block->distSq  = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->outerSq = (spot->outer * spot->outer) >> 2;
    block->scale   = 0;
    if (block->outerSq < block->distSq) {
        result = 0;
    } else {
        block->innerSq = (spot->inner * spot->inner) >> 2;
        Gfx_NormalizeLightDir(&block->vec, &block->dir);
        block->cosAng = -(block->dir.vx * light->transform.lighting.composed.m[0][2] + block->dir.vy * light->transform.lighting.composed.m[1][2] + block->dir.vz * light->transform.lighting.composed.m[2][2]) >> 12;
        // Inside the cone when the sample is nearer the axis than half the opening.
        if (rcos(spot->angle >> 1) < block->cosAng) {
            result       = ((spot->head.color.r * 8 + spot->head.color.g * 6 + spot->head.color.b * 2) >> 8) + 0xF00;
            block->scale = ONE;
            if (block->distSq > block->innerSq) {
                block->outerSq -= block->innerSq;
                block->distSq  -= block->innerSq;
                while (block->outerSq > 0xFFFF) {
                    block->outerSq >>= 4;
                    block->distSq  >>= 4;
                }
                if (block->outerSq != 0) {
                    block->scale = ((block->outerSq - block->distSq) << 12) / block->outerSq;
                    result       = (block->scale * result) >> 12;
                }
            }
        }
    }
    light->transform.lighting.attenuation = block->scale;
    SCRATCH_STACK_RELEASE_BLOCK(GpSpotScratch);
    return result;
}

static void func_800D759C(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    GpViewLightScratch* block;
    MATRIX*             dirMtx;
    MATRIX*             colorMtx;

    block    = SCRATCH_STACK_RESERVE_BLOCK(GpViewLightScratch);
    dirMtx   = arg3->lightMtx;
    colorMtx = arg3->colorMtx;

    block->in.vx = -arg1->transform.lighting.local.t[0];
    block->in.vy = -arg1->transform.lighting.local.t[1];
    block->in.vz = -arg1->transform.lighting.local.t[2];
    Gfx_NormalizeLightDir(&block->in, &block->dir);

    Gp_UpdateCoord(arg1->transform.lighting.parent);
    TransposeMatrix(&gGfxViewCoord.workm, &block->mtx);
    gte_MulMatrix0(&block->mtx, &arg1->transform.lighting.parent->workm, &block->mtx);

    gfxRotateSv(&block->mtx, &block->dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    block->scale = arg1->transform.lighting.attenuation;
    gte_lddp(block->scale);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&block->dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_STACK_RELEASE_BLOCK(GpViewLightScratch);
}

/// Selects the nearest point or cone light to world position `arg0`, using
/// squared distance after halving each coordinate difference. Initializes
/// `arg1` to no selection even when `Gp_GetRoomCoordSet` returns 0.
static void func_800D78A4(VECTOR* arg0, GpNearestLight* arg1)
{
    GpRoomCoordSet*       set;
    WorldCoordPointLight* point;
    WorldCoordLight*      light;
    WorldCoordSpotLight*  cone;
    VECTOR*               delta;
    u32                   best;
    u32                   dist;
    s32                   i;

    set           = Gp_GetRoomCoordSet(&gGameSession->location.loc);
    best          = 0x7FFFFFFF;
    arg1->kind    = -1;
    arg1->field_4 = 0;
    arg1->light   = NULL;
    if (set != NULL) {
        SCRATCH_STACK_RESERVE_BYTES(0x10);
        delta = SCRATCH_STACK_CURSOR(VECTOR);
        if (set->n60 > 0) {
            point = set->arr60;
            for (i = 0; i < set->n60; i++, point++) {
                light     = &point->head;
                delta->vx = (light->transform.coord.workm.t[0] - arg0->vx) >> 1;
                delta->vy = (light->transform.coord.workm.t[1] - arg0->vy) >> 1;
                delta->vz = (light->transform.coord.workm.t[2] - arg0->vz) >> 1;
                dist      = delta->vx * delta->vx + delta->vy * delta->vy + delta->vz * delta->vz;
                if (dist < best) {
                    best        = dist;
                    arg1->kind  = 1;
                    arg1->light = light;
                }
            }
        }
        if (set->n6C > 0) {
            cone = set->arr6C;
            for (i = 0; i < set->n6C; i++, cone++) {
                light     = &cone->head;
                delta->vx = (light->transform.coord.workm.t[0] - arg0->vx) >> 1;
                delta->vy = (light->transform.coord.workm.t[1] - arg0->vy) >> 1;
                delta->vz = (light->transform.coord.workm.t[2] - arg0->vz) >> 1;
                dist      = delta->vx * delta->vx + delta->vy * delta->vy + delta->vz * delta->vz;
                if (dist < best) {
                    best        = dist;
                    arg1->kind  = 2;
                    arg1->light = light;
                }
            }
        }
        SCRATCH_STACK_RELEASE_BYTES(0x10);
    }
}

static __inline__ void solve_func_800D9794(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    GpLightScratch* block;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(GpLightScratch);
    block    = SCRATCH_STACK_CURSOR(GpLightScratch);
    dirMtx   = arg3->lightMtx;
    colorMtx = arg3->colorMtx;
    Gfx_NormalizeLightDir((VECTOR*)arg1->transform.coord.workm.t, &block->dir);

    dirMtx->m[arg0][0] = block->dir.vx;
    dirMtx->m[arg0][1] = block->dir.vy;
    dirMtx->m[arg0][2] = block->dir.vz;

    block->scale = arg1->transform.lighting.attenuation;
    gte_lddp(block->scale);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&block->dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_STACK_RELEASE_BLOCK(GpLightScratch);
}

static __inline__ void solve_func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    GpLightScratch* block;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(GpLightScratch);
    block        = SCRATCH_STACK_CURSOR(GpLightScratch);
    dirMtx       = arg3->lightMtx;
    colorMtx     = arg3->colorMtx;
    block->in.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    block->in.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    block->in.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, &block->dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    block->scale = arg1->transform.lighting.attenuation;
    gte_lddp(block->scale);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&block->dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_STACK_RELEASE_BLOCK(GpLightScratch);
}

static __inline__ void solve_func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    GpLightScratch* block;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(GpLightScratch);
    block        = SCRATCH_STACK_CURSOR(GpLightScratch);
    dirMtx       = arg3->lightMtx;
    colorMtx     = arg3->colorMtx;
    block->in.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    block->in.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    block->in.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, &block->dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    block->scale = arg1->transform.lighting.attenuation;
    gte_lddp(block->scale);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&block->dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_STACK_RELEASE_BLOCK(GpLightScratch);
}

static __inline__ s32 solve_luma(WorldCoordLight* arg0)
{
    s16 viewId;

    viewId = arg0->transform.lighting.viewId;
    if (viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS && gGameSession->location.loc.view != viewId) {
        return 0;
    }
    {
        s32 r, g, b, lum;
        r                                    = arg0->color.r;
        g                                    = arg0->color.g;
        b                                    = arg0->color.b;
        arg0->transform.lighting.attenuation = ONE;
        lum                                  = r * 8 + g * 6 + b * 2;
        USE_REG3(lum, lum, lum);
        return (lum >> 8) + 0xF00;
    }
}

static __inline__ void solve_rank(GpRec12* slots, s32 val, s32 kind, void* obj, GpRec12* last)
{
    if (val > 0 && last->field_4 < val) {
        Gp_InsertRankedSlot(slots, val, kind, obj, 2);
    }
}
static __inline__ void solve_rank0(GpRec12* slots, s32 val, s32 kind, void* obj, GpLightSolveScratch* block)
{
    if (val > 0 && block->slots[3].field_4 < val) {
        Gp_InsertRankedSlot(slots, val, kind, obj, 2);
    }
}
void func_800D7A9C(TmdObject* extra, VECTOR* pos, s32 start, s32 count)
{

    register s32                  startr;
    register GpRoomCoordSet*      set;
    register MATRIX*              colorMtx;
    register GpLightSolveScratch* block;

    s32                   n;
    s32                   nOcc;
    s32                   idx;
    s32                   i;
    s32                   sum;
    s32                   val;
    void**                cutoffPtr;
    WorldCoordPointLight* light;

    startr   = start;
    set      = Gp_GetRoomCoordSet(&gGameSession->location.loc);
    colorMtx = extra->colorMtx;
    nOcc     = 0;
    if (set == NULL) {
        return;
    }

    n = set->n58 + set->n60 + set->n6C;
    for (idx = 0; idx < 8; idx++) {
        if (Gp_RoomCoords[idx].framesLeft != 0) {
            nOcc++;
        }
    }

    sum = startr + count;
    n  += nOcc;
    if ((u32)sum >= 4U) {
        return;
    }
    if (count == 0) {
        return;
    }

    Gp_FillSVec3x3((GpSVec3x3*)colorMtx, 0, 0, 0);

    if ((u32)(sum - 1) >= (u32)n) {
        func_800D7A9C(extra, pos, startr, count - 1);
        return;
    }

    {
        u8* head;

        head                     = SCRATCH_STACK_CURSOR(u8);
        head                    -= 0x7C;
        SCRATCH_STACK_CURSOR(u8) = head;
        block                    = SCRATCH_STACK_CURSOR(GpLightSolveScratch);
    }

    {
        s32 j;

        j = startr;
        for (; (u32)j < (u32)count;) {
            colorMtx->m[0][j] = 0;
            colorMtx->m[1][j] = 0;
            colorMtx->m[2][j] = 0;
            j++;
        }
    }

    {

        i = 0;
        do {
            block->slots[i].field_4 = -1;
            block->slots[i].field_8 = 0;
            i++;
        } while (i < 4);
    }

    block->pos.vx   = pos->vx;
    block->pos.vy   = pos->vy;
    block->pos.vz   = pos->vz;
    block->local.vx = pos->vx - gGfxViewCoord.workm.t[0];
    block->local.vy = pos->vy - gGfxViewCoord.workm.t[1];
    block->local.vz = pos->vz - gGfxViewCoord.workm.t[2];
    gte_TransposeMatrix(&gGfxViewCoord.workm, &block->mtx);

    gfxLoadRotSv(&block->mtx, &block->local);
    gte_rtv0();
    gte_stsv(&block->local);

    {
        s32                 pointIndex;
        register GpCoord64* p;

        register GpRec12* last;

        p          = Gp_RoomCoords;
        pointIndex = 0;
        last       = &block->slots[3];

        block->pos.vx = block->local.vx;
        block->pos.vy = block->local.vy;
        block->pos.vz = block->local.vz;
        do {
            if (p->framesLeft != 0) {
                light            = &p->light;
                val              = Gp_LightPoint(light, (VECTOR3*)&block->pos);
                block->intensity = val;
                solve_rank(block->slots, val, 3, light, last);
            }
            pointIndex++;
            p++;
        } while (pointIndex < 8);
    }

    if (set->n60 > 0) {
        light = set->arr60;
        for (i = 0; i < set->n60; i++, light++) {
            val              = Gp_LightPointRoom(light, (VECTOR3*)&block->pos);
            block->intensity = val;
            solve_rank(block->slots, val, 1, light, &block->slots[3]);
        }
    }

    if (set->n6C > 0) {
        register WorldCoordSpotLight* spot;
        s32                           coneRank;

        spot = set->arr6C;
        i    = 0;

        for (; i < set->n6C;) {
            val              = Gp_LightCone(spot, (VECTOR3*)&block->pos);
            coneRank         = 2;
            block->intensity = val;
            solve_rank(block->slots, val, coneRank, spot, &block->slots[3]);
            i++;
            spot++;
        }
    }

    if (set->n58 > 0) {
        register WorldCoordLight* obj58;

        obj58 = set->arr58;
        i     = 0;
        for (; i < set->n58;) {
            val              = solve_luma(obj58);
            block->intensity = val;
            solve_rank0(block->slots, val, 0, obj58, block);
            i++;
            obj58++;
        }
    }

    colorMtx->t[2] = 0;
    colorMtx->t[1] = 0;
    colorMtx->t[0] = 0;

    {

        s32              end;
        GpSolveSlotView* slotArg;

        WorldCoordLight* light;
        WorldCoordLight* extraLight;

        s32 delta;
        s32 amb;

        i = startr;
        if ((u32)i < (u32)count) {
            end = i + count;
            /* The slot is addressed as the block advanced by whole records, so its
             * fields load at the slot array's own displacement; indexing
             * block->slots folds that displacement into the pointer instead. */
            slotArg = (GpSolveSlotView*)((GpRec12*)block + end);

            do {
                light = block->slots[i].field_8;
                if (light != NULL) {
                    if (i == end - 1) {
                        cutoffPtr = &((GpSolveSlotView*)((GpRec12*)block + count))->field_8;
                        if (*cutoffPtr != 0) {
                            WorldCoordLight* cutoffLight;
                            s32              attenuation;
                            s32              diff;
                            s32              cutoffScale;
                            cutoffLight = slotArg->field_8;
                            delta       = 0;
                            if (block->slots[count].field_0 != 0) {
                                attenuation = light->transform.lighting.attenuation;
                                cutoffScale = cutoffLight->transform.lighting.attenuation;
                                diff        = attenuation - cutoffScale;
                                if (diff < 0) {
                                    diff = 0;
                                }
                                if (diff < 0x200) {
                                    diff                                  = (diff * attenuation) >> 9;
                                    delta                                 = attenuation - diff;
                                    light->transform.lighting.attenuation = diff;
                                }
                            }
                            extraLight     = slotArg->field_8;
                            delta        >>= 2;
                            amb            = (slotArg->field_4 >> 2) + delta;
                            colorMtx->t[2] = amb;
                            colorMtx->t[1] = amb;
                            colorMtx->t[0] = amb;
                            colorMtx->t[0] = amb + (extraLight->color.r >> 6);
                            colorMtx->t[1] = colorMtx->t[1] + (extraLight->color.g >> 6);
                            colorMtx->t[2] = colorMtx->t[2] + (extraLight->color.b >> 6);
                        }
                    }

                    switch (block->slots[i].field_0) {
                        case 1:
                        case 3:
                            solve_func_800D98C4(i, block->slots[i].field_8, &block->pos, extra);
                            break;
                        case 2:
                            solve_func_800D9A30(i, block->slots[i].field_8, &block->pos, extra);
                            break;
                        default:
                            solve_func_800D9794(i, block->slots[i].field_8, &block->pos, extra);
                            break;
                    }
                }

                i++;

            } while ((u32)i < (u32)count);
        }
    }

    // Apply the ambient override or the view's minimum RGB levels.
    if ((s8)Gp_OverrideVecFlag == 1) {
        colorMtx->t[0] = Gp_OverrideVec.vx;
        colorMtx->t[1] = Gp_OverrideVec.vy;
        colorMtx->t[2] = Gp_OverrideVec.vz;
    } else {
        WorldCoordRoomAmbientEntry* ambientEntry;

        ambientEntry = Gp_GetRoomBound(&gGameSession->location.loc);
        if (colorMtx->t[0] < ambientEntry->color.r) {
            colorMtx->t[0] = ambientEntry->color.r;
        }
        if (colorMtx->t[1] < ambientEntry->color.g) {
            colorMtx->t[1] = ambientEntry->color.g;
        }
        if (colorMtx->t[2] < ambientEntry->color.b) {
            colorMtx->t[2] = ambientEntry->color.b;
        }
    }

    if ((s8)Gp_OverrideVec2Flag == 1) {
        u16*      ov;
        SVECTOR3* row;
        s32       j;

        ov  = Gp_OverrideVec2.components;
        j   = 0;
        row = (SVECTOR3*)extra->colorMtx;
        do {
            block->local.vx = row[j].vx;
            block->local.vy = row[j].vy;
            block->local.vz = row[j].vz;
            gte_lddp(*ov);
            gte_ldsv(&block->local);
            gte_gpf12();
            gte_stsv(&block->local);
            row[j].vx = block->local.vx;
            row[j].vy = block->local.vy;
            row[j].vz = block->local.vz;
            j++;
            ov++;
        } while (j < 3);
    }

    if (Pad_RemapState->field_1 == 0x13 && D_80760618->field_1 == 1) {
        i = 0;
        do {
            D_80760618->field_30[i] = block->slots[i];
            i++;
        } while (i < 4);
    }

    SCRATCH_STACK_RELEASE_BYTES(0x7C);
}

/// Fills a light colour matrix so all three lights share one colour: every
/// column of the red, green and blue rows gets `r`, `g` and `b`.
static inline void _gpSetColorMtx(MATRIX* mtx, s16 r, s16 g, s16 b)
{
    mtx->m[0][0] = mtx->m[0][1] = mtx->m[0][2] = r;
    mtx->m[1][0] = mtx->m[1][1] = mtx->m[1][2] = g;
    mtx->m[2][0] = mtx->m[2][1] = mtx->m[2][2] = b;
}

static void Gp_DebugPanTask(Task* arg0)
{
    Task*                        slot;
    Task*                        work;
    PlayerStatus*                cfg;
    TmdObject*                   extra;
    GfxCoord*                    coord;
    GameActor*                   actor;
    GameActor*                   actor2;
    MATRIX*                      mtx;
    WorldCoordProjectionScratch* projection;
    SVECTOR*                     inputPoint;
    VECTOR                       vec;
    TextDrawReq                  req;
    s32                          i;
    s32                          val;

    slot = gameGetPtrSlot(3);
    cfg  = &gPlayerStatus;
    if (slot == NULL) {
        return;
    }

    extra = slot->extra.tmd;
    coord = &extra->coords[1];
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x64;
    vec.vz = coord->workm.t[2];

    if (Pad_RemapState->field_1 == 0x13) {
        SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
        projection          = SCRATCH_STACK_CURSOR(WorldCoordProjectionScratch);
        D_80760618->field_1 = 1;
        func_800D7A9C(extra, &vec, 0, 3);
        func_800D78A4(&vec, &D_80760618->field_24);
        inputPoint          = &projection->point;
        D_80760618->field_1 = 0;
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_SetTransMatrix(&GsWSMATRIX);
        // Project the sampled position to place the debug light readout.
        projection->point.vx = vec.vx;
        projection->point.vy = vec.vy;
        projection->point.vz = vec.vz;
        gte_ldv0(inputPoint);
        gte_rtps();
        gte_stsxy(&projection->screen);
        gte_stdp(&projection->depthCue);
        gte_stflg(&projection->projectionFlags);
        gte_stszotz(&projection->orderingDepth);
        if (projection->projectionFlags >= 0) {
            req.x          = projection->screen.vx;
            req.y          = projection->screen.vy;
            req.otIndex    = 4;
            req.colorRgb   = 0x37A78;
            req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            req.alignment  = TEXT_ALIGNMENT_CENTER;
            req.drawMode   = TEXT_DRAW_QUEUED;
            Text_DrawString(&req, (u8*)D_8009745C);
        }
        SCRATCH_STACK_RELEASE_BLOCK(WorldCoordProjectionScratch);
    } else {
        func_800D7A9C(extra, &vec, 0, 3);
        if (D_80114F28 != 0) {
            mtx = extra->colorMtx;
            val = rsin(gDisplayState.loopCount << 6) + 0x1800;
            if ((gDisplayState.loopCount & 1) == 0) {
                val >>= 1;
            }
            _gpSetColorMtx(mtx, 0x200, val, 0x200);
            D_80114F28 = 0;
        } else if ((gDisplayState.animFrame % 3) == 0 && cfg->hp > 0 && gGameSession->eventState == 0) {
            if (Gp_StateC08.field_14 > 0 || (Gp_StateC08.field_16 != 0 && Gp_StateC08.field_17 != 0)) {
                _gpSetColorMtx(extra->colorMtx, 0x400, 0x2000, 0x2000);
            } else if (Gp_StateC08.field_16 != 0) {
                _gpSetColorMtx(extra->colorMtx, 0x400, 0x400, 0x2000);
            } else if (Gp_StateC08.field_17 != 0) {
                _gpSetColorMtx(extra->colorMtx, 0x2000, 0x2000, 0x400);
            }
            if (cfg->statusFlags & PLAYER_STATUS_BERSERKER) {
                _gpSetColorMtx(extra->colorMtx, 0x2000, 0x400, 0x400);
            }
        }
    }

    actor = slot->work;
    {
        Task* task;

        for (i = 0; i < 2; i++) {
            task = actor->attachmentTasks[i];
            if (task != NULL) {
                extra           = task->extra.tmd;
                extra->lightMtx = &Gp_DefaultMtx;
                extra->colorMtx = &Gp_DefaultMtx2;
            }
        }
        for (i = 0; i < 2; i++) {
            task = actor->equipmentTasks[i];
            if (task != NULL) {
                extra           = task->extra.tmd;
                extra->lightMtx = &Gp_DefaultMtx;
                extra->colorMtx = &Gp_DefaultMtx2;
            }
        }
    }

    work = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION];
    if (work != NULL) {
        TmdObject* model;

        model           = work->extra.tmd;
        actor2          = work->work;
        coord           = &model->coords[1];
        extra           = model;
        extra->colorMtx = &D_80114EF8;
        extra->lightMtx = &D_80114ED8;
        Gp_UpdateCoord(coord);
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1] - 0x64;
        vec.vz = coord->workm.t[2];
        func_800D7A9C(extra, &vec, 0, 3);
        {
            Task* task;

            for (i = 0; i < 2; i++) {
                task = actor2->attachmentTasks[i];
                if (task != NULL) {
                    extra           = task->extra.tmd;
                    extra->lightMtx = &D_80114ED8;
                    extra->colorMtx = &D_80114EF8;
                }
            }
            for (i = 0; i < 2; i++) {
                task = actor2->equipmentTasks[i];
                if (task != NULL) {
                    extra           = task->extra.tmd;
                    extra->lightMtx = &D_80114ED8;
                    extra->colorMtx = &D_80114EF8;
                }
            }
        }
    }
}

/// Remaps a 3x3 color matrix (`MATRIX.m`) from lighting mode `arg2`
/// (`colorMode` bits 0-1, or bits 2-3 when blending). Weighted mode
/// collapses RGB as (7,6,3)/33 then *4/*2/*1. Black zeros the matrix. Tint
/// fills 0x180/0x100/0x100. Default remaps to *3/*1/*3 when
/// `reactionFlags` has damage over time set. `ENEMY_COLOR_HIT_FLASH` with
/// `spawnState == 0` applies a `rsin(gDisplayState.loopCount << 6)` flicker
/// and clears the bit.
static void Gp_RemapActorColor(Enemy* arg0, MATRIX* arg1, s32 arg2)
{
    s32 i;
    s32 val;

    if (arg2 == ENEMY_COLOR_WEIGHTED) {
        goto case1;
    } else if (arg2 < ENEMY_COLOR_BLACK) {
        goto def;
    } else if (arg2 == ENEMY_COLOR_BLACK) {
        goto case2;
    } else if (arg2 == ENEMY_COLOR_TINT) {
        goto case3;
    } else {
        goto def;
    }

case1: {
    s32 t;
    for (i = 0; i < 3; i++) {
        t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
        arg1->m[0][i] = t * 4;
        arg1->m[1][i] = t * 2;
        arg1->m[2][i] = t;
    }
}
    return;

case3:
    if ((arg0->colorMode & ENEMY_COLOR_HIT_FLASH) && (arg0->spawnState == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x180;
    arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = 0x100;
    arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x100;
    return;

case2:
    if ((arg0->colorMode & ENEMY_COLOR_HIT_FLASH) && (arg0->spawnState == 0)) {
        goto flicker;
    }
    arg1->m[0][0] = 0;
    arg1->m[0][1] = 0;
    arg1->m[0][2] = 0;
    arg1->m[1][0] = 0;
    arg1->m[1][1] = 0;
    arg1->m[1][2] = 0;
    arg1->m[2][0] = 0;
    arg1->m[2][1] = 0;
    arg1->m[2][2] = 0;
    return;

def:
    if ((arg0->colorMode & ENEMY_COLOR_HIT_FLASH) && (arg0->spawnState == 0)) {
    flicker:
        val = rsin(gDisplayState.loopCount << 6) + 0x1800;
        if ((gDisplayState.loopCount & 1) == 0) {
            val >>= 1;
        }
        arg1->m[0][0] = arg1->m[0][1] = arg1->m[0][2] = 0x200;
        arg1->m[1][0] = arg1->m[1][1] = arg1->m[1][2] = val;
        arg1->m[2][0] = arg1->m[2][1] = arg1->m[2][2] = 0x200;
        arg0->colorMode                              &= ENEMY_COLOR_HIT_FLASH_CLEAR;
    } else if (arg0->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        s32 t;
        for (i = 0; i < 3; i++) {
            t             = (arg1->m[0][i] * 7 + arg1->m[1][i] * 6 + arg1->m[2][i] * 3) / 33;
            arg1->m[0][i] = t * 3;
            arg1->m[1][i] = t;
            arg1->m[2][i] = t * 3;
        }
    }
}

void Gp_UpdateActorColor(Enemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3)
{
    TmdObject*      extra;
    MATRIX*         colorMtx;
    s32             mode;
    GpColorScratch* block;
    s32             i;
    s32             w0;
    s32             w1;

    extra    = arg0->task->extra.tmd;
    colorMtx = extra->colorMtx;
    mode     = arg0->colorMode & ENEMY_COLOR_MODE_MASK;
    if ((!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (extra->buffer != NULL)) || (gGameSession->sceneUpdatesPaused != 1)) {
        block = SCRATCH_STACK_RESERVE_BLOCK(GpColorScratch);
        func_800D7A9C(extra, arg1, 0, 3);
        if ((s8)arg0->colorBlend <= 0) {
            Gp_RemapActorColor(arg0, colorMtx, mode);
        } else {
            block->mtx.m[0][0] = colorMtx->m[0][0];
            block->mtx.m[0][1] = colorMtx->m[0][1];
            block->mtx.m[0][2] = colorMtx->m[0][2];
            block->mtx.m[1][0] = colorMtx->m[1][0];
            block->mtx.m[1][1] = colorMtx->m[1][1];
            block->mtx.m[1][2] = colorMtx->m[1][2];
            block->mtx.m[2][0] = colorMtx->m[2][0];
            block->mtx.m[2][1] = colorMtx->m[2][1];
            block->mtx.m[2][2] = colorMtx->m[2][2];
            Gp_RemapActorColor(arg0, colorMtx, mode);
            Gp_RemapActorColor(arg0, &block->mtx, (arg0->colorMode >> ENEMY_COLOR_PREVIOUS_SHIFT) & ENEMY_COLOR_MODE_MASK);
            w0 = (s8)arg0->colorBlend << 8;
            w1 = 0x1000 - w0;
            for (i = 0; i < 3; i++) {
                block->col0.vx = colorMtx->m[0][i];
                block->col0.vy = colorMtx->m[1][i];
                block->col0.vz = colorMtx->m[2][i];
                block->col1.vx = block->mtx.m[0][i];
                block->col1.vy = block->mtx.m[1][i];
                block->col1.vz = block->mtx.m[2][i];
                gte_lddp(w1);
                gte_ldsv(&block->col0);
                gte_gpf12();
                gte_lddp(w0);
                gte_ldsv(&block->col1);
                gte_gpl12();
                gte_stsv(&block->col0);
                colorMtx->m[0][i] = block->col0.vx;
                colorMtx->m[1][i] = block->col0.vy;
                colorMtx->m[2][i] = block->col0.vz;
            }
            if (Gp_StateF0.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                arg0->colorBlend--;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(GpColorScratch);
    }
}

static void Gp_LightFalloff(WorldCoordPointLight* light)
{
    GpAttnScratch*   block;
    s32              result;
    WorldCoordLight* base;

    base           = &light->head;
    result         = 0;
    block          = SCRATCH_STACK_RESERVE_BLOCK(GpAttnScratch);
    block->vec.vx  = base->transform.lighting.local.t[0] >> 1;
    block->vec.vy  = base->transform.lighting.local.t[1] >> 1;
    block->vec.vz  = base->transform.lighting.local.t[2] >> 1;
    block->distSq  = block->vec.vx * block->vec.vx + block->vec.vy * block->vec.vy + block->vec.vz * block->vec.vz;
    block->outerSq = (light->outer * light->outer) >> 2;
    block->scale   = 0;
    if ((u32)block->outerSq >= (u32)block->distSq) {
        block->innerSq = (light->inner * light->inner) >> 2;
        result         = ((light->head.color.r * 8 + light->head.color.g * 6 + light->head.color.b * 2) >> 8) + 0xF00;
        block->scale   = ONE;
        if ((u32)block->distSq > (u32)block->innerSq) {
            block->outerSq -= block->innerSq;
            block->distSq  -= block->innerSq;
            while ((u32)block->outerSq > 0xFFFF) {
                block->outerSq = (u32)block->outerSq >> 4;
                block->distSq  = (u32)block->distSq >> 4;
            }
            if (block->outerSq != 0) {
                block->scale = ((u32)(block->outerSq - block->distSq) << 12) / (u32)block->outerSq;
                result       = (u32)(block->scale * result) >> 12;
            }
        }
    }
    base->transform.lighting.attenuation   = block->scale;
    base->transform.lighting.composed.t[0] = result;
    SCRATCH_STACK_RELEASE_BLOCK(GpAttnScratch);
}

void Gp_SetLightMode(Enemy* arg0, s32 arg1)
{
    u8 val;

    val   = arg0->colorMode;
    arg1 &= ENEMY_COLOR_MODE_MASK;
    if ((val & ENEMY_COLOR_MODE_MASK) != arg1) {
        arg0->colorMode  = (val & ENEMY_COLOR_KEPT_BITS) | ((val & ENEMY_COLOR_MODE_MASK) << ENEMY_COLOR_PREVIOUS_SHIFT) | arg1;
        arg0->colorBlend = ENEMY_COLOR_BLEND_STEPS;
    }
}

s32 gpGetObjDepth(GfxCoord* coord)
{
    s32 val;

    val = coord->workm.t[2] - gDisplayState.screenDistance;
    if (val >= 0x7FFF) {
        val = 0x7FFF;
    }
    if (val < -0x7FFF) {
        val = -0x7FFF;
    }
    return val >> 8;
}

/// Projects a local origin into the reserved record immediately below `scratchEnd`.
///
/// `scratchEnd` is the cursor before reservation. Keeps the GTE projection
/// settings, replaces its matrices/results and leaves the scratch cursor alone.
static inline void _worldCoordProjectOrigin(const GfxCoord* coord, WorldCoordProjectionScratch* scratchEnd)
{
    WorldCoordProjectionScratch* projection;
    SVECTOR*                     inputPoint;

    projection = scratchEnd - 1;
    inputPoint = &projection->point;
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    projection->point.vz = 0;
    projection->point.vy = 0;
    projection->point.vx = 0;
    gte_RotTransPers(inputPoint, &scratchEnd[-1].screen, &scratchEnd[-1].depthCue,
                     &scratchEnd[-1].projectionFlags, &scratchEnd[-1].orderingDepth);
}

s32 worldCoordGetOriginAudioPan(const GfxCoord* coord)
{
    // Screen-pixel limits and pixels per sound-event pan-offset unit.
    enum {
        WORLD_COORDINATE_AUDIO_PAN_MIN_X           = -160,
        WORLD_COORDINATE_AUDIO_PAN_MAX_X           = 159,
        WORLD_COORDINATE_AUDIO_PAN_PIXELS_PER_UNIT = 10,
        WORLD_COORDINATE_AUDIO_PAN_CENTER          = 0
    };
    WorldCoordProjectionScratch* scratchEnd;
    WorldCoordProjectionScratch* projection;
    s32                          negativePan;

    projection = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
    scratchEnd = projection + 1;
    // Project the coordinate's local origin through its composed view matrix.
    _worldCoordProjectOrigin(coord, scratchEnd);
    if (projection->projectionFlags >= 0) {
        if (projection->screen.vx > WORLD_COORDINATE_AUDIO_PAN_MAX_X) {
            projection->screen.vx = WORLD_COORDINATE_AUDIO_PAN_MAX_X;
        }
        if (projection->screen.vx <= WORLD_COORDINATE_AUDIO_PAN_MIN_X) {
            projection->screen.vx = WORLD_COORDINATE_AUDIO_PAN_MIN_X;
        }
        negativePan = -projection->screen.vx / WORLD_COORDINATE_AUDIO_PAN_PIXELS_PER_UNIT;
    } else {
        negativePan = WORLD_COORDINATE_AUDIO_PAN_CENTER;
    }
    SCRATCH_STACK_RELEASE_BLOCK(WorldCoordProjectionScratch);
    return -negativePan;
}

void Gp_SetOverrideVec(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        Gp_OverrideVecFlag = 0;
        return;
    }
    Gp_OverrideVecFlag = 1;
    Gp_OverrideVec     = *arg0;
}

void Gp_SetOverrideVec2(SVECTOR* arg0)
{
    if (arg0 == NULL) {
        Gp_OverrideVec2Flag = 0;
        return;
    }
    Gp_OverrideVec2Flag    = 1;
    Gp_OverrideVec2.vector = *arg0;
}

void Gp_SetObjTrans(TmdObject* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    MATRIX* m;

    m       = arg0->colorMtx;
    m->t[0] = arg1;
    m->t[1] = arg2;
    m->t[2] = arg3;
}

static WorldCoordRoomAmbientEntry* Gp_GetRoomBound(GameLocationKey* arg0)
{
    GpRoomCoordRec**            mid;
    GpRoomCoordRec*             rec;
    WorldCoordRoomAmbientEntry* ambientEntry;
    WorldCoordRoomAmbientEntry* ambientTable;

    mid = Gp_RoomCoordTables[arg0->stage - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->area - 1];
        if (rec != NULL) {
            rec = &rec[arg0->room - 1];
        }
    }
    ambientEntry = &Gp_RoomBoundDefault;
    if (rec != NULL) {
        ambientTable = rec->field_4;
        if (ambientTable != NULL) {
            if (ambientTable->viewCount >= arg0->view) {
                ambientEntry = &ambientTable[arg0->view];
            }
        }
    }
    return ambientEntry;
}

static s32 Gp_CountRoomCoords(void)
{
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 8; i++) {
        if (Gp_RoomCoords[i].framesLeft != 0) {
            count++;
        }
    }
    return count;
}

static GpRoomCoordSet* Gp_GetRoomCoordSet(GameLocationKey* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;
    GpRoomCoordSet*  result;

    result = NULL;
    mid    = Gp_RoomCoordTables[arg0->stage - 1];
    rec    = NULL;
    if (mid != NULL) {
        rec = mid[arg0->area - 1];
        if (rec != NULL) {
            rec = &rec[arg0->room - 1];
        }
    }
    if (rec != NULL) {
        result = rec->field_0;
    }
    return result;
}

void func_800D96C8(Task* arg0)
{
    TaskFunc funcs[2] = { Gp_BindDefaultMtx, Gp_DebugPanTask };

    funcs[arg0->state](arg0);
}

static s32 Gp_GetObjLuma(WorldCoordLight* arg0)
{
    s16 viewId;

    viewId = arg0->transform.lighting.viewId;
    if (viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS && gGameSession->location.loc.view != viewId) {
        return 0;
    }
    arg0->transform.lighting.attenuation = ONE;
    return ((arg0->color.r * 8 + arg0->color.g * 6 + arg0->color.b * 2) >> 8) + 0xF00;
}

/// World X of the object's position.
static s32 Gp_GetObjTransX(GfxCoord* coord)
{
    return coord->workm.t[0];
}

static void func_800D9794(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    GpLightScratch* block;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(GpLightScratch);
    block    = SCRATCH_STACK_CURSOR(GpLightScratch);
    dirMtx   = arg3->lightMtx;
    colorMtx = arg3->colorMtx;
    Gfx_NormalizeLightDir((VECTOR*)arg1->transform.coord.workm.t, &block->dir);

    dirMtx->m[arg0][0] = block->dir.vx;
    dirMtx->m[arg0][1] = block->dir.vy;
    dirMtx->m[arg0][2] = block->dir.vz;

    block->scale = arg1->transform.lighting.attenuation;
    gte_lddp(block->scale);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&block->dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_STACK_RELEASE_BLOCK(GpLightScratch);
}

static void func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    GpLightScratch* block;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(GpLightScratch);
    block        = SCRATCH_STACK_CURSOR(GpLightScratch);
    dirMtx       = arg3->lightMtx;
    colorMtx     = arg3->colorMtx;
    block->in.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    block->in.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    block->in.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, &block->dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    block->scale = arg1->transform.lighting.attenuation;
    gte_lddp(block->scale);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&block->dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_STACK_RELEASE_BLOCK(GpLightScratch);
}

static void func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    GpLightScratch* block;
    MATRIX*         dirMtx;
    MATRIX*         colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(GpLightScratch);
    block        = SCRATCH_STACK_CURSOR(GpLightScratch);
    dirMtx       = arg3->lightMtx;
    colorMtx     = arg3->colorMtx;
    block->in.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    block->in.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    block->in.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    Gfx_NormalizeLightDir(&block->in, &block->dir);

    dirMtx->m[arg0][0] = -block->dir.vx;
    dirMtx->m[arg0][1] = -block->dir.vy;
    dirMtx->m[arg0][2] = -block->dir.vz;

    block->scale = arg1->transform.lighting.attenuation;
    gte_lddp(block->scale);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&block->dir);

    colorMtx->m[0][arg0] = block->dir.vx;
    colorMtx->m[1][arg0] = block->dir.vy;
    colorMtx->m[2][arg0] = block->dir.vz;

    SCRATCH_STACK_RELEASE_BLOCK(GpLightScratch);
}

void Gp_InsertRankedSlot(GpRec12* arg0, s32 arg1, s32 arg2, void* arg3, s32 arg4)
{
    GpRec12* rec;
    GpRec12* next;

    if (arg1 <= 0) {
        return;
    }

    rec = (GpRec12*)(arg4 * sizeof(*arg0) + (s32)arg0);
    if (rec->field_4 < arg1) {
        if (arg4 < 3) {
            rec[1] = *rec;
        }
        if (arg4 > 0) {
            Gp_InsertRankedSlot(arg0, arg1, arg2, arg3, arg4 - 1);
        } else {
            arg0->field_4 = arg1;
            arg0->field_0 = arg2;
            arg0->field_8 = arg3;
        }
    } else if (arg4 < 3) {
        next           = rec + 1;
        next->field_4  = arg1;
        rec[1].field_0 = arg2;
        next->field_8  = arg3;
    }
}

static void Gp_FillSVec3x3(GpSVec3x3* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    arg0->field_0.vx = arg0->field_0.vy = arg0->field_0.vz = arg1;
    arg0->field_6.vx = arg0->field_6.vy = arg0->field_6.vz = arg2;
    arg0->field_C.vx = arg0->field_C.vy = arg0->field_C.vz = arg3;
}

static GpRoomCoordRec* Gp_GetRoomCoordRec(GameLocationKey* arg0)
{
    GpRoomCoordRec** mid;
    GpRoomCoordRec*  rec;

    mid = Gp_RoomCoordTables[arg0->stage - 1];
    rec = NULL;
    if (mid != NULL) {
        rec = mid[arg0->area - 1];
        if (rec != NULL) {
            rec = &rec[arg0->room - 1];
        }
    }
    return rec;
}

void func_800D9CC8(Task* arg0)
{
    Task_CallExit(arg0);
}

static void Gp_CopyDefaultBound(WorldCoordRoomAmbientEntry* ambientEntry)
{
    *ambientEntry = Gp_RoomBoundDefault;
}

static void Gp_BindDefaultMtx(Task* arg0)
{
    Task*           slot;
    TmdObject*      extra;
    GameActor*      actor;
    GpRoomCoordSet* result;
    s32             i;

    slot  = gameGetPtrSlot(3);
    extra = slot->extra.tmd;
    if (slot != NULL) {
        result = Gp_GetRoomCoordSet(&gGameSession->location.loc);
        i      = 0;
        if (result == 0) {
            taskKill(arg0);
            return;
        }
        arg0->spawnArg2.pointer = result;
        extra->lightMtx         = &Gp_DefaultMtx;
        extra->colorMtx         = &Gp_DefaultMtx2;
        actor                   = slot->work;
        Gp_OverrideVecFlag      = 0;
        Gp_OverrideVec2Flag     = 0;
        D_80114F28              = 0;
        do {
            extra           = actor->attachmentTasks[i]->extra.tmd;
            extra->lightMtx = &Gp_DefaultMtx;
            extra->colorMtx = &Gp_DefaultMtx2;
            i++;
        } while (i < 2);
        arg0->state++;
        Gp_DebugPanTask(arg0);
    }
}

static __inline__ void project_slot(s32* sxy, GpSlot70* slot)
{
    WorldTargetNode* src;
    GpPerspScratch*  block;

    src = slot->field_0;
    SCRATCH_STACK_RESERVE_BLOCK(GpPerspScratch);
    block         = SCRATCH_STACK_CURSOR(GpPerspScratch);
    block->vec.vx = GP_NODE_ENEMY(src)->bodyPos.vx;
    block->vec.vy = GP_NODE_ENEMY(src)->bodyPos.vy;
    block->vec.vz = GP_NODE_ENEMY(src)->bodyPos.vz;
    gte_SetRotMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stdp(&block->p);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    SCRATCH_STACK_RELEASE_BLOCK(GpPerspScratch);
}

static __inline__ void Gp_ObjWorldPosInline(WorldCollisionBody* obj, VECTOR* pos)
{
    u8*     h;
    VECTOR* vec;
    h                          = SCRATCH_STACK_CURSOR(u8);
    vec                        = (VECTOR*)(h - 0x30);
    SCRATCH_STACK_CURSOR(void) = vec;
    gte_SetRotMatrix(&obj->coord->workm);
    gte_ldv0(&obj->pos);
    gte_rtv0();
    gte_stlvnl(vec);
    pos->vx = (obj->coord)->workm.t[0] + ((VECTOR*)(h - 0x30))->vx;
    pos->vy = (obj->coord)->workm.t[1] + vec->vy;
    pos->vz = (obj->coord)->workm.t[2] + vec->vz;
    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

/// The same lookup as `Gp_GetIdParam0`, returned at the tables' own width.
static inline u16 _gpIdParam0(s32 id)
{
    if ((id & 0x8000) == 0) {
        return Gp_IdParamLo[id & 0x7F].params[2];
    }
    return Gp_IdParamHi.rows[id & 0x7F].field[5];
}
