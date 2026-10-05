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
#include "item_use.h"
#include "gameplay/light.h"
#include "gameplay/lighting_work.h"
#include "loading.h"
#include "gameplay/room.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_targets.h"
#include "weapon_data.h"

#include "gameplay/damage.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"

/// Source kinds stored in the ranked-light table.
enum {
    WORLD_COORDINATE_RANKED_LIGHT_DIRECTIONAL     = 0,
    WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT      = 1,
    WORLD_COORDINATE_RANKED_LIGHT_CONE            = 2,
    WORLD_COORDINATE_RANKED_LIGHT_TRANSIENT_POINT = 3
};

/// Three model-light contributions plus one cutoff contribution for ambient blending.
enum { WORLD_COORDINATE_RANKED_LIGHT_COUNT = 4 };

/// Rank of an unused entry; only positive contributions enter the table.
enum { WORLD_COORDINATE_RANKED_LIGHT_EMPTY_RANK = -1 };

/// A light contribution ranked at the sampled model position.
///
/// Entries are ordered by decreasing contribution score, derived from weighted
/// RGB intensity and distance falloff after view/cone eligibility tests. One
/// extra entry supplies the cutoff for ambient blending. An empty entry has a
/// null light and an unused rank; its kind is unspecified. Source storage must
/// remain live while the entry is consumed. Selection may adjust the borrowed
/// header's scratch attenuation.
typedef struct {
    s32              kind;  // Source kind (0 directional, 1 room point, 2 cone, 3 transient point); valid with a light
    s32              rank;  // Positive contribution score, descending; -1 when unused
    WorldCoordLight* light; // Borrowed common header of the source light, or NULL for an empty entry
} _WorldCoordRankedLight;
STATIC_ASSERT_SIZEOF(_WorldCoordRankedLight, 0xC);

/// Source-kind sentinel for a nearest-room-light result without a selection.
enum { WORLD_COORDINATE_NEAREST_LIGHT_NONE = -1 };

/// The diagnostic probe's nearest authored room point or cone light.
///
/// Selection compares squared distances after arithmetically halving each
/// signed coordinate difference. Only unsigned sums below 0x7FFFFFFF select a
/// source. Equal distances retain the earlier entry, with points before cones.
/// View filters, intensity, falloff radii and cone angles do not affect selection.
/// The source header is borrowed and must not outlive its loaded room overlay.
typedef struct {
    s32              kind;    // Source kind (-1 none, 1 room point, 2 cone)
    s32              field_4; // Role unproven; the nearest-light query always clears this word
    WorldCoordLight* light;   // Borrowed common source header, or NULL when kind is -1
} _WorldCoordNearestRoomLight;
STATIC_ASSERT_SIZEOF(_WorldCoordNearestRoomLight, 0xC);

/// Values written to the light-probe's ranked snapshot gate.
enum {
    WORLD_COORDINATE_LIGHT_CAPTURE_DISABLED = 0,
    WORLD_COORDINATE_LIGHT_CAPTURE_ENABLED  = 1
};

/// Writable light-probe capture prefix used by world-coordinate diagnostics.
///
/// The player probe opens `captureEnabled` while sampling lighting. The nearest
/// selection is refreshed independently; an early solver return retains the
/// ranked snapshot. Captured light headers are borrowed and require their source
/// storage to remain live. Only the accessed 0x60-byte prefix is described;
/// allocation ownership and the original object's full extent are unproven.
typedef struct {
    byte                        unknown_0[0x1];                                    // Contents unproven
    s8                          captureEnabled;                                    // Ranked snapshot gate (0 closed, 1 open; other values closed)
    byte                        unknown_2[0x22];                                   // Contents and field boundaries unproven
    _WorldCoordNearestRoomLight nearestRoomLight;                                  // Nearest authored room point/cone selection, independent of contribution rank
    _WorldCoordRankedLight      rankedLights[WORLD_COORDINATE_RANKED_LIGHT_COUNT]; // Three model-light contributions plus the ambient cutoff
} _WorldCoordLightProbeCapture;
STATIC_ASSERT_SIZEOF(_WorldCoordLightProbeCapture, 0x60);

/// Temporary squared-distance falloff workspace for a point light.
///
/// Borrowed from the scratch stack for one query and released after its Q12
/// attenuation is copied to the light. Offset components are arithmetically
/// halved in the sample's coordinate frame; origin queries use local translation.
/// The SDK vector's final word is unused and left uninitialized.
///
/// In the fade interval, `distanceSquared` and `outerLimit` become differences
/// from the inner squared radius. Both are logically shifted in four-bit steps
/// until the outer span fits 16 bits, bounding the numerator before its Q12 shift.
typedef struct {
    VECTOR halfOffset;         // Halved light-minus-sample offset; room rejection makes X and Z nonnegative
    u32    distanceSquared;    // Sum of squared offset components, then inner-relative and reduced with outerLimit
    u32    outerLimit;         // Half radius for room rejection, then (outer radius squared >> 2), then reduced fade span
    u32    innerRadiusSquared; // Inner radius squared >> 2; full strength at or below this threshold
    u32    attenuation;        // Contribution scale with 12 fractional bits (0 dark, ONE full strength)
} _WorldCoordPointLightFalloffScratch;
STATIC_ASSERT_SIZEOF(_WorldCoordPointLightFalloffScratch, 0x20);

/// Q12 falloff arithmetic and the bound applied to its squared-distance span.
enum {
    WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS   = 12,
    WORLD_COORDINATE_LIGHT_FALLOFF_MAX_SPAN        = 0xFFFF,
    WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT = 4
};

/// Temporary workspace for one cone-light contribution query.
///
/// Borrowed from the scratch stack for one sample and released after its Q12
/// attenuation is copied to the light, including a zero contribution. Offset
/// components are arithmetically halved in the sample's coordinate frame.
/// The SDK vector's final word is unused and left uninitialized.
/// Normalization yields the direction from the sample toward the light, and
/// the cone test compares it with the composed matrix's Z column, the cone
/// axis.
///
/// Inside the cone, distance falloff matches a point light: full strength
/// within the inner radius and none once the distance reaches the outer
/// radius. Equal radii are full strength out to that distance. In the fade
/// interval, `distanceSquared` and `outerLimit` become differences from the
/// inner squared radius. Both are logically shifted in four-bit steps until
/// the outer span fits 16 bits, bounding the numerator before its Q12 shift.
typedef struct {
    VECTOR  halfOffset;         // Halved light-minus-sample offset; final word unused and uninitialized
    SVECTOR direction;          // Normalized sample-to-light direction, length about ONE
    u32     distanceSquared;    // Sum of squared offset components, then inner-relative and reduced with outerLimit
    u32     outerLimit;         // (outer radius squared >> 2), then the reduced fade span
    u32     innerRadiusSquared; // Inner radius squared >> 2; full strength at or below this threshold
    u32     attenuation;        // Contribution scale with 12 fractional bits (0 dark, ONE full strength)
    s32     axisCosine;         // Q12 cosine of the angle from the cone axis to the sample
} _WorldCoordConeLightScratch;
STATIC_ASSERT_SIZEOF(_WorldCoordConeLightScratch, 0x2C);

/// Scratch workspace for ranking lights at one sample position.
///
/// Borrowed from the scratch stack while a model's light and colour columns
/// are filled. The caller's world position is stored, then replaced by the
/// sample in view space; falloff and direction calculations read that
/// view-space position. The ranked table keeps three model-light contributions
/// and the cutoff blended into ambient. A diagnostic probe may copy the table
/// before the block is released.
typedef struct {
    MATRIX                 viewRotation;                                      // Transposed view rotation; places the sample in view space. Translation unused
    s32                    contribution;                                      // Contribution score of the light just evaluated
    VECTOR                 viewPosition;                                      // View-space sample. The world position is stored first and replaced before any read; SDK pad unused
    SVECTOR                viewOffset;                                        // View-relative offset, then the rotated sample; later reused to scale a colour row
    byte                   unknown_3C[0x10];                                  // Contents and field boundaries unproven; the query does not access these bytes
    _WorldCoordRankedLight rankedLights[WORLD_COORDINATE_RANKED_LIGHT_COUNT]; // Three model-light contributions plus the ambient cutoff
} _WorldCoordLightQueryScratch;
STATIC_ASSERT_SIZEOF(_WorldCoordLightQueryScratch, 0x7C);

/// Temporary workspace for one model light-direction row and RGB column.
///
/// Borrowed from the scratch stack until that pair is written. Direction and
/// colour share output storage and are consumed in that order. Normalization
/// needs another 24 scratch bytes below this block. The SDK vectors' final
/// components are unused and left uninitialized.
typedef struct {
    VECTOR lightToObject;  // Object minus light position in world units; unused for directional lights
    union {
        SVECTOR direction; // Normalized input direction, length approximately ONE
        struct {
            s16 r;         // Attenuated red intensity, 12 fractional bits
            s16 g;         // Attenuated green intensity, 12 fractional bits
            s16 b;         // Attenuated blue intensity, 12 fractional bits
        } color;           // Replaces the direction after its matrix row has been written
    } result;              // Shared direction and colour output storage
    s32 attenuation;       // Sign-extended light contribution scale (0 dark, ONE full strength), 12 fractional bits
} _WorldCoordLightMatrixScratch;
STATIC_ASSERT_SIZEOF(_WorldCoordLightMatrixScratch, 0x1C);

/// Temporary workspace for one light-direction row and RGB column of a light
/// placed in its parent's frame.
///
/// Borrowed from the scratch stack until that pair is written. The direction
/// comes from the light's local translation rather than its composed placement:
/// it is normalized in the parent's frame, then rotated by the parent's composed
/// rotation with the view coordinate's rotation undone. Direction and colour
/// share output storage and are consumed in that order. Normalization needs
/// another 24 scratch bytes below this block. The SDK vectors' final components
/// and the matrix translation are unused and left uninitialized.
typedef struct {
    VECTOR lightToParentOrigin; // Negated local translation of the light, in the parent's units
    union {
        SVECTOR direction;      // Normalized direction, length approximately ONE; first in the parent's frame, then rotated
        struct {
            s16 r;              // Attenuated red intensity, 12 fractional bits
            s16 g;              // Attenuated green intensity, 12 fractional bits
            s16 b;              // Attenuated blue intensity, 12 fractional bits
        } color;                // Replaces the direction after its matrix row has been written
    } result;                   // Shared direction and colour output storage
    MATRIX parentRotation;      // Transposed view rotation, then its product with the parent's composed rotation
    s32    attenuation;         // Sign-extended light contribution scale (0 dark, ONE full strength), 12 fractional bits
} _WorldCoordParentFrameLightMatrixScratch;
STATIC_ASSERT_SIZEOF(_WorldCoordParentFrameLightMatrixScratch, 0x3C);

/// Per-channel multipliers applied to the model light-colour matrix.
///
/// Scales have 12 fractional bits: zero suppresses a channel and `ONE` is unity.
/// Each scale multiplies one RGB row of the 3x3 matrix; the ambient translation
/// is preserved. The input is copied as an `SVECTOR`, then its scale halfwords
/// are read unsigned for GTE GPF12. The copied final halfword is unused.
typedef union {
    SVECTOR inputVector;      // Copied RGB scales in vx/vy/vz, including the unused final halfword
    u16     channelScales[3]; // Unsigned Q12 multipliers in red, green, blue order
} _WorldCoordLightColorScaleOverride;
STATIC_ASSERT_SIZEOF(_WorldCoordLightColorScaleOverride, 8);

/* Define BSS before API headers to preserve first-declaration order. */
WorldCoordTransientPointLight gWorldCoordTransientPointLights[WORLD_COORDINATE_TRANSIENT_LIGHT_COUNT];

u8 Gp_OverrideVec2Flag;

_WorldCoordLightColorScaleOverride Gp_OverrideVec2;

struct WorldTargetNode* D_80115260;

s32 D_80115264;

#include "world_coords.h"

/// Absolute import: pointer to the light-probe capture block.
extern _WorldCoordLightProbeCapture* D_80760618;

/// Re-evaluates each lit transient light slot against the view.
static inline void _gpUpdateRoomCoordSlots(void);

static s32 Gp_LightPointRoom(WorldCoordPointLight* light, VECTOR3* pos);

static s32 Gp_LightPoint(WorldCoordPointLight* light, VECTOR3* pos);

static s32 Gp_LightCone(WorldCoordSpotLight* spot, VECTOR3* pos);

static void func_800D759C(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

/// Selects the nearest point or cone light to world position `arg0`, using
/// squared distance after halving each coordinate difference. Initializes
/// `nearestLight` to no selection even when `Gp_GetRoomCoordSet` returns 0.
static void func_800D78A4(VECTOR* arg0, _WorldCoordNearestRoomLight* nearestLight);

static __inline__ void solve_func_800D9794(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static __inline__ void solve_func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static __inline__ void solve_func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static __inline__ s32 solve_luma(WorldCoordLight* arg0);

static __inline__ void solve_rank(_WorldCoordRankedLight* slots, s32 val, s32 kind, WorldCoordLight* obj, _WorldCoordRankedLight* last);

static __inline__ void solve_rank0(_WorldCoordRankedLight* slots, s32 val, s32 kind, WorldCoordLight* obj, _WorldCoordLightQueryScratch* block);

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

static const WorldCoordRoomAmbientEntry* Gp_GetRoomBound(GameLocationKey* arg0);

static s32 Gp_CountRoomCoords(void);

static WorldCoordRoomLights* Gp_GetRoomCoordSet(GameLocationKey* arg0);

static s32 Gp_GetObjLuma(WorldCoordLight* arg0);

/// World X of the object's position.
static s32 Gp_GetObjTransX(GfxCoord* coord);

static void func_800D9794(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static void func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

static void func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3);

void Gp_InsertRankedSlot(_WorldCoordRankedLight* arg0, s32 arg1, s32 arg2, WorldCoordLight* arg3, s32 arg4);

static void _worldCoordFillLightColorMatrixOutOfLine(MATRIX* colorMtx, s16 r, s16 g, s16 b);

static WorldCoordRoomLighting* Gp_GetRoomCoordRec(GameLocationKey* arg0);

static void Gp_CopyDefaultBound(WorldCoordRoomAmbientEntry* ambientEntry);

static void Gp_BindDefaultMtx(Task* arg0);

static __inline__ void Gp_ObjWorldPosInline(WorldCollisionBody* obj, VECTOR* pos);

/// The same lookup as `Gp_GetIdParam0`, returned at the tables' own width.
static inline u16 _gpIdParam0(s32 id);

/// Returns 1 if item `arg0` cannot be used, 0 if it can.
/// `arg1` supplies `field_2` (capacity) for ammo ids 0xA0–0xBF.

/// Re-evaluates each lit transient light slot against the view.
static inline void _gpUpdateRoomCoordSlots(void)
{
    WorldCoordTransientPointLight* slot;
    s32                            i;

    slot = gWorldCoordTransientPointLights;
    for (i = 0; i < ARRAY_SIZE(gWorldCoordTransientPointLights); i++, slot++) {
        if (slot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            actorRenderComposeCoordRelative(&slot->light.head.transform.coord, &gGfxViewCoord);
        }
    }
}

/// First-run init plus per-frame update of the current room's `WorldCoordRoomLights`
/// coordinate arrays (parented to `gGfxViewCoord`) and the `gWorldCoordTransientPointLights` slots.
/// Kills `arg0` when `Gp_GetRoomCoordSet` returns 0.
void Gp_UpdateRoomCoords(Task* task)
{
    WorldCoordRoomLights* roomLights;
    SVECTOR*              vec;
    WorldCoordLight*      light;
    WorldCoordPointLight* point;
    WorldCoordSpotLight*  spot;
    GfxCoord*             coord;
    s32                   i;
    s32                   j;

    roomLights = Gp_GetRoomCoordSet(&gGameSession->location.loc);
    if (roomLights == NULL) {
        taskKill(task);
        return;
    }

    vec = SCRATCH_STACK_RESERVE_BYTES(0x1C);
    if (task->state == 0) {
        point = roomLights->pointLights;
        for (i = 0; i < roomLights->pointLightCount; i++, point++) {
            coord               = &point->head.transform.coord;
            coord->parent       = &gGfxViewCoord;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }

        spot = roomLights->coneLights;
        for (i = 0; i < roomLights->coneLightCount; i++, spot++) {
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

        if (roomLights->directionalLightCount > 0) {
            WorldCoordLight* dir;

            dir = roomLights->directionalLights;
            for (i = 0; i < roomLights->directionalLightCount; i++, dir++) {
                coord               = &dir->transform.coord;
                coord->parent       = &gGfxViewCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }

        // Drop previous transient contributions before composing the room's lighting.
        for (j = 0; j < ARRAY_SIZE(gWorldCoordTransientPointLights); j++) {
            gWorldCoordTransientPointLights[j].framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
            coord                                         = &gWorldCoordTransientPointLights[j].light.head.transform.coord;
            coord->parent                                 = &gGfxViewCoord;
        }

        task->state++;
    }

    actorRenderComposeCoord(&gGfxViewCoord);

    _gpUpdateRoomCoordSlots();

    point = roomLights->pointLights;
    for (i = 0; i < roomLights->pointLightCount; i++, point++) {
        coord = &point->head.transform.coord;
        actorRenderComposeCoordRelative(coord, &gGfxViewCoord);
    }

    spot = roomLights->coneLights;
    for (i = 0; i < roomLights->coneLightCount; i++, spot++) {
        coord = &spot->head.transform.coord;
        actorRenderComposeCoordRelative(coord, &gGfxViewCoord);
    }

    if (roomLights->directionalLightCount > 0) {
        light = roomLights->directionalLights;
        for (i = 0; i < roomLights->directionalLightCount; i++, light++) {
            coord = &light->transform.coord;
            actorRenderComposeCoordRelative(coord, &gGfxViewCoord);
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}

static s32 Gp_LightPointRoom(WorldCoordPointLight* light, VECTOR3* pos)
{
    WorldCoordLight*                     base;
    _WorldCoordPointLightFalloffScratch* falloff;
    s32                                  result;
    s32                                  tooFar;
    s16                                  viewId;

    base   = &light->head;
    viewId = base->transform.lighting.viewId;
    if (viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS && gGameSession->location.loc.view != viewId) {
        return 0;
    }
    falloff                = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordPointLightFalloffScratch);
    falloff->halfOffset.vx = (base->transform.lighting.composed.t[0] - pos->vx) >> 1;
    falloff->halfOffset.vy = (base->transform.lighting.composed.t[1] - pos->vy) >> 1;
    falloff->halfOffset.vz = (base->transform.lighting.composed.t[2] - pos->vz) >> 1;
    falloff->outerLimit    = light->outer >> 1;
    falloff->attenuation   = 0;
    if (falloff->halfOffset.vx < 0) {
        falloff->halfOffset.vx = -falloff->halfOffset.vx;
    }
    if (falloff->halfOffset.vz < 0) {
        falloff->halfOffset.vz = -falloff->halfOffset.vz;
    }
    // Rejects on the X and Z extents alone before paying for the squares.
    tooFar = (u32)falloff->halfOffset.vx > falloff->outerLimit;
    if (!tooFar) {
        tooFar = (u32)falloff->halfOffset.vz > falloff->outerLimit;
        if (!tooFar) {
            falloff->outerLimit      = (light->outer * light->outer) >> 2;
            falloff->distanceSquared = falloff->halfOffset.vx * falloff->halfOffset.vx + falloff->halfOffset.vy * falloff->halfOffset.vy + falloff->halfOffset.vz * falloff->halfOffset.vz;
            tooFar                   = falloff->outerLimit < falloff->distanceSquared;
        }
    }
    if (tooFar) {
        result = 0;
    } else {
        falloff->innerRadiusSquared = (light->inner * light->inner) >> 2;
        result                      = ((light->head.color.r * 8 + light->head.color.g * 6 + light->head.color.b * 2) >> 8) + 0xF00;
        falloff->attenuation        = ONE;
        // Measure the fade interval from its inner edge and bound the Q12 numerator.
        if (falloff->distanceSquared > falloff->innerRadiusSquared) {
            falloff->outerLimit      -= falloff->innerRadiusSquared;
            falloff->distanceSquared -= falloff->innerRadiusSquared;
            while (falloff->outerLimit > WORLD_COORDINATE_LIGHT_FALLOFF_MAX_SPAN) {
                falloff->outerLimit      >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
                falloff->distanceSquared >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
            }
            if (falloff->outerLimit != 0) {
                falloff->attenuation = ((falloff->outerLimit - falloff->distanceSquared) << WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS) / falloff->outerLimit;
                result               = (falloff->attenuation * result) >> WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS;
            }
        }
    }
    base->transform.lighting.attenuation = falloff->attenuation;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordPointLightFalloffScratch);
    return result;
}

static s32 Gp_LightPoint(WorldCoordPointLight* light, VECTOR3* pos)
{
    _WorldCoordPointLightFalloffScratch* falloff;
    s32                                  result;
    WorldCoordLight*                     base;

    base                     = &light->head;
    result                   = 0;
    falloff                  = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordPointLightFalloffScratch);
    falloff->halfOffset.vx   = (base->transform.lighting.composed.t[0] - pos->vx) >> 1;
    falloff->halfOffset.vy   = (base->transform.lighting.composed.t[1] - pos->vy) >> 1;
    falloff->halfOffset.vz   = (base->transform.lighting.composed.t[2] - pos->vz) >> 1;
    falloff->distanceSquared = falloff->halfOffset.vx * falloff->halfOffset.vx + falloff->halfOffset.vy * falloff->halfOffset.vy + falloff->halfOffset.vz * falloff->halfOffset.vz;
    falloff->outerLimit      = (light->outer * light->outer) >> 2;
    falloff->attenuation     = 0;
    if (falloff->outerLimit >= falloff->distanceSquared) {
        falloff->innerRadiusSquared = (light->inner * light->inner) >> 2;
        result                      = ((light->head.color.r * 8 + light->head.color.g * 6 + light->head.color.b * 2) >> 8) + 0xF00;
        falloff->attenuation        = ONE;
        // Measure the fade interval from its inner edge and bound the Q12 numerator.
        if (falloff->distanceSquared > falloff->innerRadiusSquared) {
            falloff->outerLimit      -= falloff->innerRadiusSquared;
            falloff->distanceSquared -= falloff->innerRadiusSquared;
            while (falloff->outerLimit > WORLD_COORDINATE_LIGHT_FALLOFF_MAX_SPAN) {
                falloff->outerLimit      >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
                falloff->distanceSquared >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
            }
            if (falloff->outerLimit != 0) {
                falloff->attenuation = ((falloff->outerLimit - falloff->distanceSquared) << WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS) / falloff->outerLimit;
                result               = (falloff->attenuation * result) >> WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS;
            }
        }
    }
    base->transform.lighting.attenuation = falloff->attenuation;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordPointLightFalloffScratch);
    return result;
}

static s32 Gp_LightCone(WorldCoordSpotLight* spot, VECTOR3* pos)
{
    WorldCoordLight*             light;
    _WorldCoordConeLightScratch* scratch;
    s32                          result;

    light  = &spot->head;
    result = 0;
    if (light->transform.lighting.viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS) {
        if (gGameSession->location.loc.view != light->transform.lighting.viewId) {
            return result;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordConeLightScratch);
    scratch                  = SCRATCH_STACK_CURSOR(_WorldCoordConeLightScratch);
    scratch->halfOffset.vx   = (light->transform.lighting.composed.t[0] - pos->vx) >> 1;
    scratch->halfOffset.vy   = (light->transform.lighting.composed.t[1] - pos->vy) >> 1;
    scratch->halfOffset.vz   = (light->transform.lighting.composed.t[2] - pos->vz) >> 1;
    scratch->distanceSquared = scratch->halfOffset.vx * scratch->halfOffset.vx + scratch->halfOffset.vy * scratch->halfOffset.vy + scratch->halfOffset.vz * scratch->halfOffset.vz;
    scratch->outerLimit      = (spot->outer * spot->outer) >> 2;
    scratch->attenuation     = 0;
    if (scratch->outerLimit < scratch->distanceSquared) {
        result = 0;
    } else {
        scratch->innerRadiusSquared = (spot->inner * spot->inner) >> 2;
        gfxNormalizeLightDirection(&scratch->halfOffset, &scratch->direction);
        // The normalized offset points toward the light, so negate the axis dot product.
        scratch->axisCosine = -(scratch->direction.vx * light->transform.lighting.composed.m[0][2] + scratch->direction.vy * light->transform.lighting.composed.m[1][2] + scratch->direction.vz * light->transform.lighting.composed.m[2][2]) >> 12;
        // Inside the cone when the sample is nearer the axis than half the opening.
        if (rcos(spot->angle >> 1) < scratch->axisCosine) {
            result               = ((spot->head.color.r * 8 + spot->head.color.g * 6 + spot->head.color.b * 2) >> 8) + 0xF00;
            scratch->attenuation = ONE;
            // Measure the fade interval from its inner edge and bound the Q12 numerator.
            if (scratch->distanceSquared > scratch->innerRadiusSquared) {
                scratch->outerLimit      -= scratch->innerRadiusSquared;
                scratch->distanceSquared -= scratch->innerRadiusSquared;
                while (scratch->outerLimit > WORLD_COORDINATE_LIGHT_FALLOFF_MAX_SPAN) {
                    scratch->outerLimit      >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
                    scratch->distanceSquared >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
                }
                if (scratch->outerLimit != 0) {
                    scratch->attenuation = ((scratch->outerLimit - scratch->distanceSquared) << WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS) / scratch->outerLimit;
                    result               = (scratch->attenuation * result) >> WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS;
                }
            }
        }
    }
    light->transform.lighting.attenuation = scratch->attenuation;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordConeLightScratch);
    return result;
}

static void func_800D759C(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    _WorldCoordParentFrameLightMatrixScratch* lightScratch;
    MATRIX*                                   dirMtx;
    MATRIX*                                   colorMtx;

    lightScratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordParentFrameLightMatrixScratch);
    dirMtx       = arg3->lightMtx;
    colorMtx     = arg3->colorMtx;

    lightScratch->lightToParentOrigin.vx = -arg1->transform.lighting.local.t[0];
    lightScratch->lightToParentOrigin.vy = -arg1->transform.lighting.local.t[1];
    lightScratch->lightToParentOrigin.vz = -arg1->transform.lighting.local.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToParentOrigin, &lightScratch->result.direction);

    // Undo the view rotation in the parent's composed rotation, then turn the direction by it.
    actorRenderComposeCoord(arg1->transform.lighting.parent);
    TransposeMatrix(&gGfxViewCoord.workm, &lightScratch->parentRotation);
    gte_MulMatrix0(&lightScratch->parentRotation, &arg1->transform.lighting.parent->workm, &lightScratch->parentRotation);

    _gfxRotateSv(&lightScratch->parentRotation, &lightScratch->result.direction);

    dirMtx->m[arg0][0] = -lightScratch->result.direction.vx;
    dirMtx->m[arg0][1] = -lightScratch->result.direction.vy;
    dirMtx->m[arg0][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    lightScratch->attenuation = arg1->transform.lighting.attenuation;
    gte_lddp(lightScratch->attenuation);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&lightScratch->result.color);

    colorMtx->m[0][arg0] = lightScratch->result.color.r;
    colorMtx->m[1][arg0] = lightScratch->result.color.g;
    colorMtx->m[2][arg0] = lightScratch->result.color.b;

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordParentFrameLightMatrixScratch);
}

/// Selects the nearest point or cone light to world position `arg0`, using
/// squared distance after halving each coordinate difference. Initializes
/// `nearestLight` to no selection even when `Gp_GetRoomCoordSet` returns 0.
static void func_800D78A4(VECTOR* arg0, _WorldCoordNearestRoomLight* nearestLight)
{
    WorldCoordRoomLights* roomLights;
    WorldCoordPointLight* point;
    WorldCoordLight*      light;
    WorldCoordSpotLight*  cone;
    VECTOR*               delta;
    u32                   best;
    u32                   dist;
    s32                   i;

    roomLights            = Gp_GetRoomCoordSet(&gGameSession->location.loc);
    best                  = 0x7FFFFFFF;
    nearestLight->kind    = WORLD_COORDINATE_NEAREST_LIGHT_NONE;
    nearestLight->field_4 = 0;
    nearestLight->light   = NULL;
    if (roomLights != NULL) {
        SCRATCH_STACK_RESERVE_BYTES(0x10);
        delta = SCRATCH_STACK_CURSOR(VECTOR);
        if (roomLights->pointLightCount > 0) {
            point = roomLights->pointLights;
            for (i = 0; i < roomLights->pointLightCount; i++, point++) {
                light     = &point->head;
                delta->vx = (light->transform.coord.workm.t[0] - arg0->vx) >> 1;
                delta->vy = (light->transform.coord.workm.t[1] - arg0->vy) >> 1;
                delta->vz = (light->transform.coord.workm.t[2] - arg0->vz) >> 1;
                dist      = delta->vx * delta->vx + delta->vy * delta->vy + delta->vz * delta->vz;
                if (dist < best) {
                    best                = dist;
                    nearestLight->kind  = WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT;
                    nearestLight->light = light;
                }
            }
        }
        if (roomLights->coneLightCount > 0) {
            cone = roomLights->coneLights;
            for (i = 0; i < roomLights->coneLightCount; i++, cone++) {
                light     = &cone->head;
                delta->vx = (light->transform.coord.workm.t[0] - arg0->vx) >> 1;
                delta->vy = (light->transform.coord.workm.t[1] - arg0->vy) >> 1;
                delta->vz = (light->transform.coord.workm.t[2] - arg0->vz) >> 1;
                dist      = delta->vx * delta->vx + delta->vy * delta->vy + delta->vz * delta->vz;
                if (dist < best) {
                    best                = dist;
                    nearestLight->kind  = WORLD_COORDINATE_RANKED_LIGHT_CONE;
                    nearestLight->light = light;
                }
            }
        }
        SCRATCH_STACK_RELEASE_BYTES(0x10);
    }
}

static __inline__ void solve_func_800D9794(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx       = arg3->lightMtx;
    colorMtx     = arg3->colorMtx;
    gfxNormalizeLightDirection(arg1->transform.coord.workm.t, &lightScratch->result.direction);

    dirMtx->m[arg0][0] = lightScratch->result.direction.vx;
    dirMtx->m[arg0][1] = lightScratch->result.direction.vy;
    dirMtx->m[arg0][2] = lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    lightScratch->attenuation = arg1->transform.lighting.attenuation;
    gte_lddp(lightScratch->attenuation);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&lightScratch->result.color);

    colorMtx->m[0][arg0] = lightScratch->result.color.r;
    colorMtx->m[1][arg0] = lightScratch->result.color.g;
    colorMtx->m[2][arg0] = lightScratch->result.color.b;

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

static __inline__ void solve_func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch                   = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx                         = arg3->lightMtx;
    colorMtx                       = arg3->colorMtx;
    lightScratch->lightToObject.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    lightScratch->lightToObject.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    lightScratch->lightToObject.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToObject, &lightScratch->result.direction);

    dirMtx->m[arg0][0] = -lightScratch->result.direction.vx;
    dirMtx->m[arg0][1] = -lightScratch->result.direction.vy;
    dirMtx->m[arg0][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    lightScratch->attenuation = arg1->transform.lighting.attenuation;
    gte_lddp(lightScratch->attenuation);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&lightScratch->result.color);

    colorMtx->m[0][arg0] = lightScratch->result.color.r;
    colorMtx->m[1][arg0] = lightScratch->result.color.g;
    colorMtx->m[2][arg0] = lightScratch->result.color.b;

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

static __inline__ void solve_func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch                   = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx                         = arg3->lightMtx;
    colorMtx                       = arg3->colorMtx;
    lightScratch->lightToObject.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    lightScratch->lightToObject.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    lightScratch->lightToObject.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToObject, &lightScratch->result.direction);

    dirMtx->m[arg0][0] = -lightScratch->result.direction.vx;
    dirMtx->m[arg0][1] = -lightScratch->result.direction.vy;
    dirMtx->m[arg0][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    lightScratch->attenuation = arg1->transform.lighting.attenuation;
    gte_lddp(lightScratch->attenuation);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&lightScratch->result.color);

    colorMtx->m[0][arg0] = lightScratch->result.color.r;
    colorMtx->m[1][arg0] = lightScratch->result.color.g;
    colorMtx->m[2][arg0] = lightScratch->result.color.b;

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
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

static __inline__ void solve_rank(_WorldCoordRankedLight* slots, s32 val, s32 kind, WorldCoordLight* obj, _WorldCoordRankedLight* last)
{
    if (val > 0 && last->rank < val) {
        Gp_InsertRankedSlot(slots, val, kind, obj, WORLD_COORDINATE_RANKED_LIGHT_COUNT - 2);
    }
}
static __inline__ void solve_rank0(_WorldCoordRankedLight* slots, s32 val, s32 kind, WorldCoordLight* obj, _WorldCoordLightQueryScratch* block)
{
    if (val > 0 && block->rankedLights[ARRAY_SIZE(block->rankedLights) - 1].rank < val) {
        Gp_InsertRankedSlot(slots, val, kind, obj, WORLD_COORDINATE_RANKED_LIGHT_COUNT - 2);
    }
}
void func_800D7A9C(TmdObject* extra, VECTOR* pos, s32 start, s32 count)
{

    register s32                           startr;
    register WorldCoordRoomLights*         roomLights;
    register MATRIX*                       colorMtx;
    register _WorldCoordLightQueryScratch* block;

    s32                   n;
    s32                   nOcc;
    s32                   idx;
    s32                   i;
    s32                   sum;
    s32                   val;
    WorldCoordPointLight* light;

    startr     = start;
    roomLights = Gp_GetRoomCoordSet(&gGameSession->location.loc);
    colorMtx   = extra->colorMtx;
    nOcc       = 0;
    if (roomLights == NULL) {
        return;
    }

    n = roomLights->directionalLightCount + roomLights->pointLightCount + roomLights->coneLightCount;
    for (idx = 0; idx < ARRAY_SIZE(gWorldCoordTransientPointLights); idx++) {
        if (gWorldCoordTransientPointLights[idx].framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            nOcc++;
        }
    }

    sum = startr + count;
    n  += nOcc;
    if ((u32)sum >= ARRAY_SIZE(block->rankedLights)) {
        return;
    }
    if (count == 0) {
        return;
    }

    _worldCoordFillLightColorMatrixOutOfLine(colorMtx, 0, 0, 0);

    if ((u32)(sum - 1) >= (u32)n) {
        func_800D7A9C(extra, pos, startr, count - 1);
        return;
    }

    {
        u8* head;

        head                     = SCRATCH_STACK_CURSOR(u8);
        head                    -= sizeof(_WorldCoordLightQueryScratch);
        SCRATCH_STACK_CURSOR(u8) = head;
        block                    = SCRATCH_STACK_CURSOR(_WorldCoordLightQueryScratch);
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
            block->rankedLights[i].rank  = WORLD_COORDINATE_RANKED_LIGHT_EMPTY_RANK;
            block->rankedLights[i].light = NULL;
            i++;
        } while (i < (s32)ARRAY_SIZE(block->rankedLights));
    }

    // Keep the caller's world position, then rotate the view-relative offset into view space.
    block->viewPosition.vx = pos->vx;
    block->viewPosition.vy = pos->vy;
    block->viewPosition.vz = pos->vz;
    block->viewOffset.vx   = pos->vx - gGfxViewCoord.workm.t[0];
    block->viewOffset.vy   = pos->vy - gGfxViewCoord.workm.t[1];
    block->viewOffset.vz   = pos->vz - gGfxViewCoord.workm.t[2];
    gte_TransposeMatrix(&gGfxViewCoord.workm, &block->viewRotation);

    _gfxLoadRotSv(&block->viewRotation, &block->viewOffset);
    gte_rtv0();
    gte_stsv(&block->viewOffset);

    {
        s32                                     pointIndex;
        register WorldCoordTransientPointLight* lightSlot;

        register _WorldCoordRankedLight* last;

        // Rank active transient points alongside the room's authored lights.
        lightSlot  = gWorldCoordTransientPointLights;
        pointIndex = 0;
        last       = &block->rankedLights[ARRAY_SIZE(block->rankedLights) - 1];

        // Falloff and light directions read this rotated sample.
        block->viewPosition.vx = block->viewOffset.vx;
        block->viewPosition.vy = block->viewOffset.vy;
        block->viewPosition.vz = block->viewOffset.vz;
        do {
            if (lightSlot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
                light               = &lightSlot->light;
                val                 = Gp_LightPoint(light, (VECTOR3*)&block->viewPosition);
                block->contribution = val;
                solve_rank(block->rankedLights, val, WORLD_COORDINATE_RANKED_LIGHT_TRANSIENT_POINT, &light->head, last);
            }
            pointIndex++;
            lightSlot++;
        } while (pointIndex < ARRAY_SIZE(gWorldCoordTransientPointLights));
    }

    if (roomLights->pointLightCount > 0) {
        light = roomLights->pointLights;
        for (i = 0; i < roomLights->pointLightCount; i++, light++) {
            val                 = Gp_LightPointRoom(light, (VECTOR3*)&block->viewPosition);
            block->contribution = val;
            solve_rank(block->rankedLights, val, WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT, &light->head, &block->rankedLights[ARRAY_SIZE(block->rankedLights) - 1]);
        }
    }

    if (roomLights->coneLightCount > 0) {
        register WorldCoordSpotLight* spot;
        s32                           coneKind;

        spot = roomLights->coneLights;
        i    = 0;

        for (; i < roomLights->coneLightCount;) {
            val                 = Gp_LightCone(spot, (VECTOR3*)&block->viewPosition);
            coneKind            = WORLD_COORDINATE_RANKED_LIGHT_CONE;
            block->contribution = val;
            solve_rank(block->rankedLights, val, coneKind, &spot->head, &block->rankedLights[ARRAY_SIZE(block->rankedLights) - 1]);
            i++;
            spot++;
        }
    }

    if (roomLights->directionalLightCount > 0) {
        register WorldCoordLight* directionalLight;

        directionalLight = roomLights->directionalLights;
        i                = 0;
        for (; i < roomLights->directionalLightCount;) {
            val                 = solve_luma(directionalLight);
            block->contribution = val;
            solve_rank0(block->rankedLights, val, WORLD_COORDINATE_RANKED_LIGHT_DIRECTIONAL, directionalLight, block);
            i++;
            directionalLight++;
        }
    }

    colorMtx->t[2] = 0;
    colorMtx->t[1] = 0;
    colorMtx->t[0] = 0;

    {

        s32 end;

        WorldCoordLight* light;
        WorldCoordLight* extraLight;

        s32 delta;
        s32 amb;

        i = startr;
        if ((u32)i < (u32)count) {
            end = i + count;
            do {
                light = block->rankedLights[i].light;
                if (light != NULL) {
                    if (i == end - 1) {
                        if (block->rankedLights[count].light != NULL) {
                            WorldCoordLight* cutoffLight;
                            s32              attenuation;
                            s32              diff;
                            s32              cutoffScale;
                            cutoffLight = block->rankedLights[end].light;
                            delta       = 0;
                            if (block->rankedLights[count].kind != WORLD_COORDINATE_RANKED_LIGHT_DIRECTIONAL) {
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
                            extraLight     = block->rankedLights[end].light;
                            delta        >>= 2;
                            amb            = (block->rankedLights[end].rank >> 2) + delta;
                            colorMtx->t[2] = amb;
                            colorMtx->t[1] = amb;
                            colorMtx->t[0] = amb;
                            colorMtx->t[0] = amb + (extraLight->color.r >> 6);
                            colorMtx->t[1] = colorMtx->t[1] + (extraLight->color.g >> 6);
                            colorMtx->t[2] = colorMtx->t[2] + (extraLight->color.b >> 6);
                        }
                    }

                    switch (block->rankedLights[i].kind) {
                        case WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT:
                        case WORLD_COORDINATE_RANKED_LIGHT_TRANSIENT_POINT:
                            solve_func_800D98C4(i, block->rankedLights[i].light, &block->viewPosition, extra);
                            break;
                        case WORLD_COORDINATE_RANKED_LIGHT_CONE:
                            solve_func_800D9A30(i, block->rankedLights[i].light, &block->viewPosition, extra);
                            break;
                        default:
                            solve_func_800D9794(i, block->rankedLights[i].light, &block->viewPosition, extra);
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
        const WorldCoordRoomAmbientEntry* ambientEntry;

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

    // Scale the three light-colour rows without changing the ambient term.
    if ((s8)Gp_OverrideVec2Flag == 1) {
        const u16* channelScale;
        SVECTOR3*  row;
        s32        channelIndex;

        channelScale = Gp_OverrideVec2.channelScales;
        channelIndex = 0;
        row          = (SVECTOR3*)extra->colorMtx;
        do {
            block->viewOffset.vx = row[channelIndex].vx;
            block->viewOffset.vy = row[channelIndex].vy;
            block->viewOffset.vz = row[channelIndex].vz;
            gte_lddp(*channelScale);
            gte_ldsv(&block->viewOffset);
            gte_gpf12();
            gte_stsv(&block->viewOffset);
            row[channelIndex].vx = block->viewOffset.vx;
            row[channelIndex].vy = block->viewOffset.vy;
            row[channelIndex].vz = block->viewOffset.vz;
            channelIndex++;
            channelScale++;
        } while (channelIndex < (s32)ARRAY_SIZE(Gp_OverrideVec2.channelScales));
    }

    // Retain the ranked snapshot before releasing the query's scratch storage.
    if (Pad_RemapState->diagnosticMode == GAME_DEBUG_DIAGNOSTIC_LIGHT_PROBE && D_80760618->captureEnabled == WORLD_COORDINATE_LIGHT_CAPTURE_ENABLED) {
        i = 0;
        do {
            D_80760618->rankedLights[i] = block->rankedLights[i];
            i++;
        } while (i < (s32)ARRAY_SIZE(D_80760618->rankedLights));
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(_WorldCoordLightQueryScratch));
}

/// Writes one colour into every column of a light-colour matrix.
///
/// `r`, `g` and `b` are signed channel intensities with 12 fractional bits
/// (`ONE` is full strength). Each value is stored in all three columns of its
/// row, so the three lights contribute the same colour. The translation, which
/// holds the ambient colour, is left unchanged.
static inline void _worldCoordFillLightColorMatrix(MATRIX* colorMtx, s16 r, s16 g, s16 b)
{
    colorMtx->m[0][0] = colorMtx->m[0][1] = colorMtx->m[0][2] = r;
    colorMtx->m[1][0] = colorMtx->m[1][1] = colorMtx->m[1][2] = g;
    colorMtx->m[2][0] = colorMtx->m[2][1] = colorMtx->m[2][2] = b;
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

    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    cfg  = &gPlayerStatus;
    if (slot == NULL) {
        return;
    }

    extra = slot->extra.tmd;
    coord = &extra->coords[1];
    actorRenderComposeCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x64;
    vec.vz = coord->workm.t[2];

    if (Pad_RemapState->diagnosticMode == GAME_DEBUG_DIAGNOSTIC_LIGHT_PROBE) {
        SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
        projection = SCRATCH_STACK_CURSOR(WorldCoordProjectionScratch);
        // Open the ranked snapshot gate only for the player's diagnostic sample.
        D_80760618->captureEnabled = WORLD_COORDINATE_LIGHT_CAPTURE_ENABLED;
        func_800D7A9C(extra, &vec, 0, 3);
        func_800D78A4(&vec, &D_80760618->nearestRoomLight);
        inputPoint                 = &projection->point;
        D_80760618->captureEnabled = WORLD_COORDINATE_LIGHT_CAPTURE_DISABLED;
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
            req.drawMode   = TEXT_DRAW_FILL_ONLY;
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
            _worldCoordFillLightColorMatrix(mtx, 0x200, val, 0x200);
            D_80114F28 = 0;
        } else if ((gDisplayState.animFrame % 3) == 0 && cfg->hp > 0 && gGameSession->eventState == 0) {
            if (Gp_StateC08.metabolismTicks > 0 || (Gp_StateC08.mindWard != 0 && Gp_StateC08.bodyWard != 0)) {
                _worldCoordFillLightColorMatrix(extra->colorMtx, 0x400, 0x2000, 0x2000);
            } else if (Gp_StateC08.mindWard != 0) {
                _worldCoordFillLightColorMatrix(extra->colorMtx, 0x400, 0x400, 0x2000);
            } else if (Gp_StateC08.bodyWard != 0) {
                _worldCoordFillLightColorMatrix(extra->colorMtx, 0x2000, 0x2000, 0x400);
            }
            if (cfg->statusFlags & PLAYER_STATUS_BERSERKER) {
                _worldCoordFillLightColorMatrix(extra->colorMtx, 0x2000, 0x400, 0x400);
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
        actorRenderComposeCoord(coord);
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
    TmdObject*                   extra;
    MATRIX*                      colorMtx;
    s32                          mode;
    WorldCoordActorColorScratch* block;
    s32                          i;
    s32                          w0;
    s32                          w1;

    extra    = arg0->task->extra.tmd;
    colorMtx = extra->colorMtx;
    mode     = arg0->colorMode & ENEMY_COLOR_MODE_MASK;
    if ((!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (extra->buffer != NULL)) || (gGameSession->sceneUpdatesPaused != 1)) {
        block = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordActorColorScratch);
        func_800D7A9C(extra, arg1, 0, 3);
        if ((s8)arg0->colorBlend <= 0) {
            Gp_RemapActorColor(arg0, colorMtx, mode);
        } else {
            block->previousColor.m[0][0] = colorMtx->m[0][0];
            block->previousColor.m[0][1] = colorMtx->m[0][1];
            block->previousColor.m[0][2] = colorMtx->m[0][2];
            block->previousColor.m[1][0] = colorMtx->m[1][0];
            block->previousColor.m[1][1] = colorMtx->m[1][1];
            block->previousColor.m[1][2] = colorMtx->m[1][2];
            block->previousColor.m[2][0] = colorMtx->m[2][0];
            block->previousColor.m[2][1] = colorMtx->m[2][1];
            block->previousColor.m[2][2] = colorMtx->m[2][2];
            Gp_RemapActorColor(arg0, colorMtx, mode);
            Gp_RemapActorColor(arg0, &block->previousColor, (arg0->colorMode >> ENEMY_COLOR_PREVIOUS_SHIFT) & ENEMY_COLOR_MODE_MASK);
            w0 = (s8)arg0->colorBlend << 8;
            w1 = 0x1000 - w0;
            for (i = 0; i < 3; i++) {
                block->currentColumn.vx  = colorMtx->m[0][i];
                block->currentColumn.vy  = colorMtx->m[1][i];
                block->currentColumn.vz  = colorMtx->m[2][i];
                block->previousColumn.vx = block->previousColor.m[0][i];
                block->previousColumn.vy = block->previousColor.m[1][i];
                block->previousColumn.vz = block->previousColor.m[2][i];
                gte_lddp(w1);
                gte_ldsv(&block->currentColumn);
                gte_gpf12();
                gte_lddp(w0);
                gte_ldsv(&block->previousColumn);
                gte_gpl12();
                gte_stsv(&block->currentColumn);
                colorMtx->m[0][i] = block->currentColumn.vx;
                colorMtx->m[1][i] = block->currentColumn.vy;
                colorMtx->m[2][i] = block->currentColumn.vz;
            }
            if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                arg0->colorBlend--;
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(WorldCoordActorColorScratch);
    }
}

static void Gp_LightFalloff(WorldCoordPointLight* light)
{
    _WorldCoordPointLightFalloffScratch* falloff;
    s32                                  result;
    WorldCoordLight*                     base;

    base                     = &light->head;
    result                   = 0;
    falloff                  = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordPointLightFalloffScratch);
    falloff->halfOffset.vx   = base->transform.lighting.local.t[0] >> 1;
    falloff->halfOffset.vy   = base->transform.lighting.local.t[1] >> 1;
    falloff->halfOffset.vz   = base->transform.lighting.local.t[2] >> 1;
    falloff->distanceSquared = falloff->halfOffset.vx * falloff->halfOffset.vx + falloff->halfOffset.vy * falloff->halfOffset.vy + falloff->halfOffset.vz * falloff->halfOffset.vz;
    falloff->outerLimit      = (light->outer * light->outer) >> 2;
    falloff->attenuation     = 0;
    if (falloff->outerLimit >= falloff->distanceSquared) {
        falloff->innerRadiusSquared = (light->inner * light->inner) >> 2;
        result                      = ((light->head.color.r * 8 + light->head.color.g * 6 + light->head.color.b * 2) >> 8) + 0xF00;
        falloff->attenuation        = ONE;
        // Measure the fade interval from its inner edge and bound the Q12 numerator.
        if (falloff->distanceSquared > falloff->innerRadiusSquared) {
            falloff->outerLimit      -= falloff->innerRadiusSquared;
            falloff->distanceSquared -= falloff->innerRadiusSquared;
            while (falloff->outerLimit > WORLD_COORDINATE_LIGHT_FALLOFF_MAX_SPAN) {
                falloff->outerLimit      >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
                falloff->distanceSquared >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;
            }
            if (falloff->outerLimit != 0) {
                falloff->attenuation = ((falloff->outerLimit - falloff->distanceSquared) << WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS) / falloff->outerLimit;
                result               = (falloff->attenuation * result) >> WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS;
            }
        }
    }
    base->transform.lighting.attenuation   = falloff->attenuation;
    base->transform.lighting.composed.t[0] = result;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordPointLightFalloffScratch);
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

s32 worldCoordGetOriginAudioDepth(const GfxCoord* coord)
{
    // Signed game-coordinate limits and the shift for 256 coordinates per depth unit.
    enum {
        /// Inclusive pre-shift audio-depth floor in game-coordinate units.
        ///
        /// Saturates the projection-plane Z difference at -32767 before the
        /// eight-bit arithmetic shift, preserving the signed-byte endpoint -128
        /// without wrapping. At that endpoint, sound starts scale the volume-table
        /// index by 1/127; pan/attenuation updates target the unattenuated level.
        WORLD_COORDINATE_AUDIO_DEPTH_MIN = -0x7FFF,

        /// Inclusive pre-shift audio-depth ceiling in game-coordinate units.
        ///
        /// The eight-bit shift maps this ceiling to +127, the silent attenuation
        /// endpoint. Saturating the projection-plane Z difference at 32767 keeps
        /// farther origins from wrapping when stored in a sound event's signed byte.
        WORLD_COORDINATE_AUDIO_DEPTH_MAX = 0x7FFF,

        /// Right-shift count converting projection-plane-relative Z to spatial sound depth.
        ///
        /// After clamping to [-32767, 32767] game-coordinate units, the arithmetic
        /// shift yields [-128, 127], fitting the sound event's signed attenuation
        /// byte. One depth unit spans 256 game-coordinate units; negative values
        /// round down, so division by 256 would change their quantization.
        WORLD_COORDINATE_AUDIO_DEPTH_SHIFT = 8
    };
    s32 depthFromPlane;

    depthFromPlane = coord->workm.t[2] - gDisplayState.screenDistance;
    if (depthFromPlane >= WORLD_COORDINATE_AUDIO_DEPTH_MAX) {
        depthFromPlane = WORLD_COORDINATE_AUDIO_DEPTH_MAX;
    }
    if (depthFromPlane < WORLD_COORDINATE_AUDIO_DEPTH_MIN) {
        depthFromPlane = WORLD_COORDINATE_AUDIO_DEPTH_MIN;
    }
    // Arithmetic shifting rounds negative depths down and retains the -128 endpoint.
    return depthFromPlane >> WORLD_COORDINATE_AUDIO_DEPTH_SHIFT;
}

/// Projects a coordinate node's local origin into a caller-owned projection record.
///
/// `coord->workm` must already map local coordinates into view space. `result`
/// must address a live, word-aligned `WorldCoordProjectionScratch`; its point
/// becomes (0, 0, 0), and its screen pixels, depth cue, flags and quarter-depth
/// are written even when projection reports an error. The unused point halfword
/// is left unchanged. Keeps the current GTE projection settings, replaces its
/// rotation/translation matrices and results, and performs no scratch reservation
/// or release.
static inline void _worldCoordProjectOrigin(const GfxCoord* coord, WorldCoordProjectionScratch* result)
{
    SVECTOR* inputPoint;

    inputPoint = &result->point;
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    result->point.vz = 0;
    result->point.vy = 0;
    result->point.vx = 0;
    gte_RotTransPers(inputPoint, &result->screen, &result->depthCue,
                     &result->projectionFlags, &result->orderingDepth);
}

s32 worldCoordGetOriginAudioPan(const GfxCoord* coord)
{
    // Screen-pixel limits and pixels per sound-event pan-offset unit.
    enum {
        /// Inclusive screen-X floor in pixels before spatial audio pan scaling.
        ///
        /// With ten pixels per pan-offset unit, -160 caps the leftward offset at -16.
        WORLD_COORDINATE_AUDIO_PAN_MIN_X = -160,

        /// Inclusive screen-X ceiling in pixels before spatial audio pan scaling.
        ///
        /// With ten pixels per pan-offset unit, +159 caps the rightward offset at +15.
        WORLD_COORDINATE_AUDIO_PAN_MAX_X = 159,

        /// Projected screen pixels per signed spatial sound pan-offset unit.
        ///
        /// Division truncates toward zero, mapping clamped X in [-160, 159]
        /// to offsets in [-16, 15]; X in [-9, 9] leaves the base pan unchanged.
        /// Playback multiplies the offset by three before adding it to the base pan.
        WORLD_COORDINATE_AUDIO_PAN_PIXELS_PER_UNIT = 10,

        /// Preserves the sound's base pan when projection reports a GTE summary error.
        ///
        /// Zero is a signed pan offset; the sound's base pan may be off-centre.
        WORLD_COORDINATE_AUDIO_PAN_NO_OFFSET = 0
    };
    WorldCoordProjectionScratch* projection;
    s32                          negativePan;

    projection = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
    // Project the coordinate's local origin through its composed view matrix.
    _worldCoordProjectOrigin(coord, projection);
    if (projection->projectionFlags >= 0) {
        if (projection->screen.vx > WORLD_COORDINATE_AUDIO_PAN_MAX_X) {
            projection->screen.vx = WORLD_COORDINATE_AUDIO_PAN_MAX_X;
        }
        if (projection->screen.vx <= WORLD_COORDINATE_AUDIO_PAN_MIN_X) {
            projection->screen.vx = WORLD_COORDINATE_AUDIO_PAN_MIN_X;
        }
        negativePan = -projection->screen.vx / WORLD_COORDINATE_AUDIO_PAN_PIXELS_PER_UNIT;
    } else {
        negativePan = WORLD_COORDINATE_AUDIO_PAN_NO_OFFSET;
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
    Gp_OverrideVec2Flag         = 1;
    Gp_OverrideVec2.inputVector = *arg0;
}

void Gp_SetObjTrans(TmdObject* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    MATRIX* m;

    m       = arg0->colorMtx;
    m->t[0] = arg1;
    m->t[1] = arg2;
    m->t[2] = arg3;
}

static const WorldCoordRoomAmbientEntry* Gp_GetRoomBound(GameLocationKey* arg0)
{
    WorldCoordRoomLighting**          areaLightingTables;
    WorldCoordRoomLighting*           roomLighting;
    const WorldCoordRoomAmbientEntry* ambientEntry;
    const WorldCoordRoomAmbientEntry* ambientTable;

    areaLightingTables = Gp_RoomCoordTables[arg0->stage - 1];
    roomLighting       = NULL;
    if (areaLightingTables != NULL) {
        roomLighting = areaLightingTables[arg0->area - 1];
        if (roomLighting != NULL) {
            roomLighting = &roomLighting[arg0->room - 1];
        }
    }
    ambientEntry = &Gp_RoomBoundDefault;
    if (roomLighting != NULL) {
        ambientTable = roomLighting->ambientTable;
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
    for (i = 0; i < ARRAY_SIZE(gWorldCoordTransientPointLights); i++) {
        if (gWorldCoordTransientPointLights[i].framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            count++;
        }
    }
    return count;
}

static WorldCoordRoomLights* Gp_GetRoomCoordSet(GameLocationKey* arg0)
{
    WorldCoordRoomLighting** areaLightingTables;
    WorldCoordRoomLighting*  roomLighting;
    WorldCoordRoomLights*    roomLights;

    roomLights         = NULL;
    areaLightingTables = Gp_RoomCoordTables[arg0->stage - 1];
    roomLighting       = NULL;
    if (areaLightingTables != NULL) {
        roomLighting = areaLightingTables[arg0->area - 1];
        if (roomLighting != NULL) {
            roomLighting = &roomLighting[arg0->room - 1];
        }
    }
    if (roomLighting != NULL) {
        roomLights = roomLighting->lights;
    }
    return roomLights;
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
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx       = arg3->lightMtx;
    colorMtx     = arg3->colorMtx;
    gfxNormalizeLightDirection(arg1->transform.coord.workm.t, &lightScratch->result.direction);

    dirMtx->m[arg0][0] = lightScratch->result.direction.vx;
    dirMtx->m[arg0][1] = lightScratch->result.direction.vy;
    dirMtx->m[arg0][2] = lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    lightScratch->attenuation = arg1->transform.lighting.attenuation;
    gte_lddp(lightScratch->attenuation);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&lightScratch->result.color);

    colorMtx->m[0][arg0] = lightScratch->result.color.r;
    colorMtx->m[1][arg0] = lightScratch->result.color.g;
    colorMtx->m[2][arg0] = lightScratch->result.color.b;

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

static void func_800D98C4(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch                   = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx                         = arg3->lightMtx;
    colorMtx                       = arg3->colorMtx;
    lightScratch->lightToObject.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    lightScratch->lightToObject.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    lightScratch->lightToObject.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToObject, &lightScratch->result.direction);

    dirMtx->m[arg0][0] = -lightScratch->result.direction.vx;
    dirMtx->m[arg0][1] = -lightScratch->result.direction.vy;
    dirMtx->m[arg0][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    lightScratch->attenuation = arg1->transform.lighting.attenuation;
    gte_lddp(lightScratch->attenuation);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&lightScratch->result.color);

    colorMtx->m[0][arg0] = lightScratch->result.color.r;
    colorMtx->m[1][arg0] = lightScratch->result.color.g;
    colorMtx->m[2][arg0] = lightScratch->result.color.b;

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

static void func_800D9A30(s32 arg0, WorldCoordLight* arg1, VECTOR* arg2, TmdObject* arg3)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch                   = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx                         = arg3->lightMtx;
    colorMtx                       = arg3->colorMtx;
    lightScratch->lightToObject.vx = arg2->vx - arg1->transform.coord.workm.t[0];
    lightScratch->lightToObject.vy = arg2->vy - arg1->transform.coord.workm.t[1];
    lightScratch->lightToObject.vz = arg2->vz - arg1->transform.coord.workm.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToObject, &lightScratch->result.direction);

    dirMtx->m[arg0][0] = -lightScratch->result.direction.vx;
    dirMtx->m[arg0][1] = -lightScratch->result.direction.vy;
    dirMtx->m[arg0][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    lightScratch->attenuation = arg1->transform.lighting.attenuation;
    gte_lddp(lightScratch->attenuation);
    gte_ldsv(&arg1->color);
    gte_gpf12();
    gte_stsv(&lightScratch->result.color);

    colorMtx->m[0][arg0] = lightScratch->result.color.r;
    colorMtx->m[1][arg0] = lightScratch->result.color.g;
    colorMtx->m[2][arg0] = lightScratch->result.color.b;

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

void Gp_InsertRankedSlot(_WorldCoordRankedLight* arg0, s32 arg1, s32 arg2, WorldCoordLight* arg3, s32 arg4)
{
    _WorldCoordRankedLight* slot;
    _WorldCoordRankedLight* nextSlot;

    if (arg1 <= 0) {
        return;
    }

    slot = (_WorldCoordRankedLight*)(arg4 * sizeof(*arg0) + (s32)arg0);
    if (slot->rank < arg1) {
        if (arg4 < WORLD_COORDINATE_RANKED_LIGHT_COUNT - 1) {
            slot[1] = *slot;
        }
        if (arg4 > 0) {
            Gp_InsertRankedSlot(arg0, arg1, arg2, arg3, arg4 - 1);
        } else {
            arg0->rank  = arg1;
            arg0->kind  = arg2;
            arg0->light = arg3;
        }
    } else if (arg4 < WORLD_COORDINATE_RANKED_LIGHT_COUNT - 1) {
        nextSlot        = slot + 1;
        nextSlot->rank  = arg1;
        slot[1].kind    = arg2;
        nextSlot->light = arg3;
    }
}

/// Writes one colour into every column of a light-colour matrix.
///
/// `r`, `g` and `b` are signed channel intensities with 12 fractional bits
/// (`ONE` is full strength). Each value is stored in all three columns of its
/// row, so the three lights contribute the same colour. The translation, which
/// holds the ambient colour, is left unchanged.
///
/// The room-light query calls this function so the nine coefficient stores
/// stay behind a jump.
static void _worldCoordFillLightColorMatrixOutOfLine(MATRIX* colorMtx, s16 r, s16 g, s16 b)
{
    _worldCoordFillLightColorMatrix(colorMtx, r, g, b);
}

static WorldCoordRoomLighting* Gp_GetRoomCoordRec(GameLocationKey* arg0)
{
    WorldCoordRoomLighting** areaLightingTables;
    WorldCoordRoomLighting*  roomLighting;

    areaLightingTables = Gp_RoomCoordTables[arg0->stage - 1];
    roomLighting       = NULL;
    if (areaLightingTables != NULL) {
        roomLighting = areaLightingTables[arg0->area - 1];
        if (roomLighting != NULL) {
            roomLighting = &roomLighting[arg0->room - 1];
        }
    }
    return roomLighting;
}

void func_800D9CC8(Task* arg0)
{
    taskCallExit(arg0);
}

static void Gp_CopyDefaultBound(WorldCoordRoomAmbientEntry* ambientEntry)
{
    *ambientEntry = Gp_RoomBoundDefault;
}

static void Gp_BindDefaultMtx(Task* arg0)
{
    Task*                 slot;
    TmdObject*            extra;
    GameActor*            actor;
    WorldCoordRoomLights* roomLights;
    s32                   i;

    slot  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    extra = slot->extra.tmd;
    if (slot != NULL) {
        roomLights = Gp_GetRoomCoordSet(&gGameSession->location.loc);
        i          = 0;
        if (roomLights == 0) {
            taskKill(arg0);
            return;
        }
        arg0->spawnArg2.pointer = roomLights;
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
        return Gp_IdParamLo[id & 0x7F].hitReaction;
    }
    return Gp_IdParamHi.rows[id & 0x7F].column.outcome.hitReaction;
}
