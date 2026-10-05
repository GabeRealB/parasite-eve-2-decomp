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

/// RGB weights and base score used before distance attenuation of a light contribution.
///
/// With all three channels at ONE, the unattenuated score is ONE. The RGB
/// sum is shifted arithmetically by eight before the base score is added.
enum {
    WORLD_COORDINATE_LIGHT_SCORE_RED_WEIGHT   = 8,
    WORLD_COORDINATE_LIGHT_SCORE_GREEN_WEIGHT = 6,
    WORLD_COORDINATE_LIGHT_SCORE_BLUE_WEIGHT  = 2,
    WORLD_COORDINATE_LIGHT_SCORE_RGB_SHIFT    = 8,
    WORLD_COORDINATE_LIGHT_SCORE_BASE         = 0xF00
};

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

static inline void _worldCoordComposeTransientPointLights(void);

static s32 _worldCoordScoreRoomPointLight(WorldCoordPointLight* light, const VECTOR* samplePosition);

static s32 _worldCoordScoreTransientPointLight(WorldCoordPointLight* light, const VECTOR* samplePosition);

static s32 _worldCoordScoreConeLight(WorldCoordSpotLight* coneLight, const VECTOR* samplePosition);

/// Selects the nearest point or cone light to world position `arg0`, using
/// squared distance after halving each coordinate difference. Initializes
/// `nearestLight` to no selection even when `_worldCoordGetRoomLights` returns 0.
static void func_800D78A4(VECTOR* arg0, _WorldCoordNearestRoomLight* nearestLight);

static __inline__ void _worldCoordWriteDirectionalLightMatrix(s32 lightIndex, const WorldCoordLight* light, const VECTOR* unusedObjectPosition, const TmdObject* model);

static __inline__ void _worldCoordWritePositionalLightMatrix(s32 lightIndex, const WorldCoordLight* light, const VECTOR* objectPosition, const TmdObject* model);

static __inline__ s32 _worldCoordScoreDirectionalLight(WorldCoordLight* light);

static __inline__ void _worldCoordAdmitRankedLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, _WorldCoordRankedLight* cutoffLight);

static __inline__ void _worldCoordAdmitDirectionalLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, _WorldCoordLightQueryScratch* lightQuery);

static void Gp_DebugPanTask(Task* arg0);

/// Remaps a 3x3 color matrix (`MATRIX.m`) from lighting mode `arg2`
/// (`colorMode` bits 0-1, or bits 2-3 when blending). Weighted mode
/// collapses RGB as (7,6,3)/33 then *4/*2/*1. Black zeros the matrix. Tint
/// fills 0x180/0x100/0x100. Default remaps to *3/*1/*3 when
/// `reactionFlags` has damage over time set. `ENEMY_COLOR_HIT_FLASH` with
/// `spawnState == 0` applies a `rsin(gDisplayState.loopCount << 6)` flicker
/// and clears the bit.
static void Gp_RemapActorColor(Enemy* arg0, MATRIX* arg1, s32 arg2);

static const WorldCoordRoomAmbientEntry* _worldCoordGetRoomAmbientEntry(const GameLocationKey* location);

static s32 Gp_CountRoomCoords(void);

static WorldCoordRoomLights* _worldCoordGetRoomLights(const GameLocationKey* location);

static void _worldCoordInsertRankedLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, s32 slotIndex);

static void _worldCoordFillLightColorMatrixOutOfLine(MATRIX* colorMtx, s16 r, s16 g, s16 b);

static void Gp_BindDefaultMtx(Task* arg0);

/// Writes a model RGB column from a light's signed Q12 colour and attenuation.
///
/// All arguments must be side-effect-free expressions. lightIndex is 0..2;
/// colorMtx is writable. lightScratch points to either light-matrix scratch
/// layout with attenuation and result.color members, overwritten here. Source,
/// output and scratch must be disjoint. Changes GTE arithmetic state. Expands
/// to a compound statement; use only as a standalone statement inside braces.
#define WORLD_COORDINATE_WRITE_ATTENUATED_LIGHT_COLOR_COLUMN(lightIndex, light, lightScratch, colorMtx) \
    {                                                                                                   \
        (lightScratch)->attenuation = (light)->transform.lighting.attenuation;                          \
        gte_lddp((lightScratch)->attenuation);                                                          \
        gte_ldsv(&(light)->color);                                                                      \
        gte_gpf12();                                                                                    \
        gte_stsv(&(lightScratch)->result.color);                                                        \
        (colorMtx)->m[0][(lightIndex)] = (lightScratch)->result.color.r;                                \
        (colorMtx)->m[1][(lightIndex)] = (lightScratch)->result.color.g;                                \
        (colorMtx)->m[2][(lightIndex)] = (lightScratch)->result.color.b;                                \
    }

/// Applies the squared-distance fade shared by point and cone light scoring.
///
/// falloff is a side-effect-free pointer to either query scratch layout, with
/// distanceSquared, outerLimit, innerRadiusSquared and attenuation initialized;
/// the sample is inside outerLimit and attenuation is ONE. contributionScore
/// is a separate s32 lvalue, overwritten here. Mutates the unsigned spans in
/// four-bit steps, divides in Q12 and preserves the unsigned score multiply
/// and logical shift. Arguments are evaluated repeatedly. Expands to a compound
/// statement; use only as a standalone statement inside braces.
#define WORLD_COORDINATE_APPLY_RADIAL_LIGHT_FADE(falloff, contributionScore)                                                                                             \
    {                                                                                                                                                                    \
        if ((falloff)->distanceSquared > (falloff)->innerRadiusSquared) {                                                                                                \
            (falloff)->outerLimit      -= (falloff)->innerRadiusSquared;                                                                                                 \
            (falloff)->distanceSquared -= (falloff)->innerRadiusSquared;                                                                                                 \
            while ((falloff)->outerLimit > WORLD_COORDINATE_LIGHT_FALLOFF_MAX_SPAN) {                                                                                    \
                (falloff)->outerLimit      >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;                                                                           \
                (falloff)->distanceSquared >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;                                                                           \
            }                                                                                                                                                            \
            if ((falloff)->outerLimit != 0) {                                                                                                                            \
                (falloff)->attenuation = (((falloff)->outerLimit - (falloff)->distanceSquared) << WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS) / (falloff)->outerLimit; \
                (contributionScore)    = ((falloff)->attenuation * (contributionScore)) >> WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS;                                 \
            }                                                                                                                                                            \
        }                                                                                                                                                                \
    }

/// The same lookup as `Gp_GetIdParam0`, returned at the tables' own width.
static inline u16 _gpIdParam0(s32 id);

/// Borrows a room-lighting descriptor through nullable stage and area tables.
///
/// The key must contain valid 1-based stage/area/room indices. The returned
/// descriptor belongs to the loaded table's overlay and may be NULL.
static __inline__ WorldCoordRoomLighting* _worldCoordLookupRoomLighting(const GameLocationKey* location)
{
    WorldCoordRoomLighting** areaLightingTables;
    WorldCoordRoomLighting*  roomLighting;

    areaLightingTables = Gp_RoomCoordTables[location->stage - 1];
    roomLighting       = NULL;
    if (areaLightingTables != NULL) {
        roomLighting = areaLightingTables[location->area - 1];
        if (roomLighting != NULL) {
            roomLighting = &roomLighting[location->room - 1];
        }
    }
    return roomLighting;
}

/// Composes active transient point lights relative to the current view.
///
/// Every slot with a nonzero lifetime is composed; this neither ages nor
/// clears a slot. Its parent chain and the persistent view coordinate must be
/// live. Called after the room lights have been composed; changes GTE state.
static inline void _worldCoordComposeTransientPointLights(void)
{
    WorldCoordTransientPointLight* slot;
    s32                            slotIndex;

    slot = gWorldCoordTransientPointLights;
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(gWorldCoordTransientPointLights); slotIndex++, slot++) {
        if (slot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            actorRenderComposeCoordRelative(&slot->light.head.transform.coord, &gGfxViewCoord);
        }
    }
}

/// First-run init plus per-frame update of the current room's `WorldCoordRoomLights`
/// coordinate arrays (parented to `gGfxViewCoord`) and the `gWorldCoordTransientPointLights` slots.
/// Kills `arg0` when `_worldCoordGetRoomLights` returns 0.
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

    roomLights = _worldCoordGetRoomLights(&gGameSession->location.loc);
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

    _worldCoordComposeTransientPointLights();

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

/// Scores a room point light at a sample in the composed lighting frame.
///
/// `samplePosition` xyz and the light's composed translation use the same
/// game-coordinate frame; only xyz are read. A view mismatch returns zero and
/// preserves attenuation. Eligible lights store Q12 attenuation: ONE inside
/// the inner radius, zero outside the outer radius, and a remaining squared-
/// distance-span ratio between them. Returns the weighted signed RGB score
/// scaled by attenuation. Equal radii retain full strength at their boundary.
/// Requires 32 scratch bytes, released before return; changes no GTE state.
static s32 _worldCoordScoreRoomPointLight(WorldCoordPointLight* light, const VECTOR* samplePosition)
{
    WorldCoordLight*                     header;
    _WorldCoordPointLightFalloffScratch* falloff;
    s32                                  contributionScore;
    s32                                  outsideRadius;
    s16                                  viewId;

    header = &light->head;
    viewId = header->transform.lighting.viewId;
    if (viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS && gGameSession->location.loc.view != viewId) {
        return 0;
    }
    falloff                = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordPointLightFalloffScratch);
    falloff->halfOffset.vx = (header->transform.lighting.composed.t[0] - samplePosition->vx) >> 1;
    falloff->halfOffset.vy = (header->transform.lighting.composed.t[1] - samplePosition->vy) >> 1;
    falloff->halfOffset.vz = (header->transform.lighting.composed.t[2] - samplePosition->vz) >> 1;
    falloff->outerLimit    = light->outer >> 1;
    falloff->attenuation   = 0;
    if (falloff->halfOffset.vx < 0) {
        falloff->halfOffset.vx = -falloff->halfOffset.vx;
    }
    if (falloff->halfOffset.vz < 0) {
        falloff->halfOffset.vz = -falloff->halfOffset.vz;
    }
    // Rejects on the X and Z extents alone before paying for the squares.
    outsideRadius = (u32)falloff->halfOffset.vx > falloff->outerLimit;
    if (!outsideRadius) {
        outsideRadius = (u32)falloff->halfOffset.vz > falloff->outerLimit;
        if (!outsideRadius) {
            falloff->outerLimit      = (light->outer * light->outer) >> 2;
            falloff->distanceSquared = falloff->halfOffset.vx * falloff->halfOffset.vx + falloff->halfOffset.vy * falloff->halfOffset.vy + falloff->halfOffset.vz * falloff->halfOffset.vz;
            outsideRadius            = falloff->outerLimit < falloff->distanceSquared;
        }
    }
    if (outsideRadius) {
        contributionScore = 0;
    } else {
        falloff->innerRadiusSquared = (light->inner * light->inner) >> 2;
        contributionScore           = ((light->head.color.r * WORLD_COORDINATE_LIGHT_SCORE_RED_WEIGHT + light->head.color.g * WORLD_COORDINATE_LIGHT_SCORE_GREEN_WEIGHT + light->head.color.b * WORLD_COORDINATE_LIGHT_SCORE_BLUE_WEIGHT) >> WORLD_COORDINATE_LIGHT_SCORE_RGB_SHIFT) + WORLD_COORDINATE_LIGHT_SCORE_BASE;
        falloff->attenuation        = ONE;
        // Measure the fade interval from its inner edge and bound the Q12 numerator.
        WORLD_COORDINATE_APPLY_RADIAL_LIGHT_FADE(falloff, contributionScore);
    }
    header->transform.lighting.attenuation = falloff->attenuation;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordPointLightFalloffScratch);
    return contributionScore;
}

/// Scores a transient point light without applying an authored view filter.
///
/// `samplePosition` xyz and the composed light translation use the same
/// game-coordinate frame. Writes Q12 attenuation (ONE inside the inner radius,
/// zero outside the outer radius, a remaining squared-distance-span ratio
/// between them) and returns the attenuated weighted signed RGB score. Equal
/// radii retain full strength at their boundary. Only sample xyz are read.
/// Requires 32 scratch bytes, released before return; changes no GTE state.
static s32 _worldCoordScoreTransientPointLight(WorldCoordPointLight* light, const VECTOR* samplePosition)
{
    _WorldCoordPointLightFalloffScratch* falloff;
    s32                                  contributionScore;
    WorldCoordLight*                     header;

    header                   = &light->head;
    contributionScore        = 0;
    falloff                  = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordPointLightFalloffScratch);
    falloff->halfOffset.vx   = (header->transform.lighting.composed.t[0] - samplePosition->vx) >> 1;
    falloff->halfOffset.vy   = (header->transform.lighting.composed.t[1] - samplePosition->vy) >> 1;
    falloff->halfOffset.vz   = (header->transform.lighting.composed.t[2] - samplePosition->vz) >> 1;
    falloff->distanceSquared = falloff->halfOffset.vx * falloff->halfOffset.vx + falloff->halfOffset.vy * falloff->halfOffset.vy + falloff->halfOffset.vz * falloff->halfOffset.vz;
    falloff->outerLimit      = (light->outer * light->outer) >> 2;
    falloff->attenuation     = 0;
    if (falloff->outerLimit >= falloff->distanceSquared) {
        falloff->innerRadiusSquared = (light->inner * light->inner) >> 2;
        contributionScore           = ((light->head.color.r * WORLD_COORDINATE_LIGHT_SCORE_RED_WEIGHT + light->head.color.g * WORLD_COORDINATE_LIGHT_SCORE_GREEN_WEIGHT + light->head.color.b * WORLD_COORDINATE_LIGHT_SCORE_BLUE_WEIGHT) >> WORLD_COORDINATE_LIGHT_SCORE_RGB_SHIFT) + WORLD_COORDINATE_LIGHT_SCORE_BASE;
        falloff->attenuation        = ONE;
        // Measure the fade interval from its inner edge and bound the Q12 numerator.
        WORLD_COORDINATE_APPLY_RADIAL_LIGHT_FADE(falloff, contributionScore);
    }
    header->transform.lighting.attenuation = falloff->attenuation;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordPointLightFalloffScratch);
    return contributionScore;
}

/// Scores a view-eligible cone light at a sample in its composed frame.
///
/// Sample xyz and composed light translation use the same game-coordinate
/// frame. A view mismatch returns zero and preserves attenuation; other
/// rejection stores zero attenuation. The normalized sample-to-light vector
/// is tested against the composed Z axis; the strict half-opening cosine test
/// excludes the cone boundary. The opening angle uses 4096 units per turn.
/// Accepted samples use point-light squared-distance falloff, including full
/// strength at an equal-radius boundary. Returns the attenuated weighted
/// signed RGB score. Requires 44 scratch bytes plus 24 for normalization,
/// released before return; changes GTE state and reads only sample xyz.
static s32 _worldCoordScoreConeLight(WorldCoordSpotLight* coneLight, const VECTOR* samplePosition)
{
    enum { WORLD_COORDINATE_CONE_AXIS_COSINE_FRACTION_BITS = 12 };

    WorldCoordLight*             header;
    _WorldCoordConeLightScratch* falloff;
    s32                          contributionScore;

    header            = &coneLight->head;
    contributionScore = 0;
    if (header->transform.lighting.viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS) {
        if (gGameSession->location.loc.view != header->transform.lighting.viewId) {
            return contributionScore;
        }
    }
    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordConeLightScratch);
    falloff                  = SCRATCH_STACK_CURSOR(_WorldCoordConeLightScratch);
    falloff->halfOffset.vx   = (header->transform.lighting.composed.t[0] - samplePosition->vx) >> 1;
    falloff->halfOffset.vy   = (header->transform.lighting.composed.t[1] - samplePosition->vy) >> 1;
    falloff->halfOffset.vz   = (header->transform.lighting.composed.t[2] - samplePosition->vz) >> 1;
    falloff->distanceSquared = falloff->halfOffset.vx * falloff->halfOffset.vx + falloff->halfOffset.vy * falloff->halfOffset.vy + falloff->halfOffset.vz * falloff->halfOffset.vz;
    falloff->outerLimit      = (coneLight->outer * coneLight->outer) >> 2;
    falloff->attenuation     = 0;
    if (falloff->outerLimit < falloff->distanceSquared) {
        contributionScore = 0;
    } else {
        falloff->innerRadiusSquared = (coneLight->inner * coneLight->inner) >> 2;
        gfxNormalizeLightDirection(&falloff->halfOffset, &falloff->direction);
        // The normalized offset points toward the header, so negate the axis dot product.
        falloff->axisCosine = -(falloff->direction.vx * header->transform.lighting.composed.m[0][2] + falloff->direction.vy * header->transform.lighting.composed.m[1][2] + falloff->direction.vz * header->transform.lighting.composed.m[2][2]) >> WORLD_COORDINATE_CONE_AXIS_COSINE_FRACTION_BITS;
        // Inside the cone when the sample is nearer the axis than half the opening.
        if (rcos(coneLight->angle >> 1) < falloff->axisCosine) {
            contributionScore    = ((coneLight->head.color.r * WORLD_COORDINATE_LIGHT_SCORE_RED_WEIGHT + coneLight->head.color.g * WORLD_COORDINATE_LIGHT_SCORE_GREEN_WEIGHT + coneLight->head.color.b * WORLD_COORDINATE_LIGHT_SCORE_BLUE_WEIGHT) >> WORLD_COORDINATE_LIGHT_SCORE_RGB_SHIFT) + WORLD_COORDINATE_LIGHT_SCORE_BASE;
            falloff->attenuation = ONE;
            // Measure the fade interval from its inner edge and bound the Q12 numerator.
            WORLD_COORDINATE_APPLY_RADIAL_LIGHT_FADE(falloff, contributionScore);
        }
    }
    header->transform.lighting.attenuation = falloff->attenuation;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordConeLightScratch);
    return contributionScore;
}

#undef WORLD_COORDINATE_APPLY_RADIAL_LIGHT_FADE

/// Writes one model light row and RGB column from a source in its parent's frame.
///
/// `lightIndex` is 0..2. `model` borrows writable lightMtx and colorMtx matrices;
/// their other rows/columns and translations are preserved. The light's parent
/// must be live. The negated local translation is normalized, rotated by the
/// parent's composed rotation with the view rotation undone, then negated into
/// the light row. The colour column uses signed RGB intensities scaled by the
/// source's previously computed Q12 attenuation. `unusedObjectPosition` is
/// ignored and may be NULL. Requires 60 scratch bytes plus 24 for normalization,
/// releases them before return, composes the parent and changes GTE state.
///
/// Retained out-of-line entry with no current call sites.
static void _worldCoordWriteParentFrameLightMatrix(s32 lightIndex, const WorldCoordLight* light, const VECTOR* unusedObjectPosition, const TmdObject* model)
{
    _WorldCoordParentFrameLightMatrixScratch* lightScratch;
    MATRIX*                                   dirMtx;
    MATRIX*                                   colorMtx;

    lightScratch = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordParentFrameLightMatrixScratch);
    dirMtx       = model->lightMtx;
    colorMtx     = model->colorMtx;

    lightScratch->lightToParentOrigin.vx = -light->transform.lighting.local.t[0];
    lightScratch->lightToParentOrigin.vy = -light->transform.lighting.local.t[1];
    lightScratch->lightToParentOrigin.vz = -light->transform.lighting.local.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToParentOrigin, &lightScratch->result.direction);

    // Undo the view rotation in the parent's composed rotation, then turn the direction by it.
    actorRenderComposeCoord(light->transform.lighting.parent);
    TransposeMatrix(&gGfxViewCoord.workm, &lightScratch->parentRotation);
    gte_MulMatrix0(&lightScratch->parentRotation, &light->transform.lighting.parent->workm, &lightScratch->parentRotation);

    _gfxRotateSv(&lightScratch->parentRotation, &lightScratch->result.direction);

    dirMtx->m[lightIndex][0] = -lightScratch->result.direction.vx;
    dirMtx->m[lightIndex][1] = -lightScratch->result.direction.vy;
    dirMtx->m[lightIndex][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    WORLD_COORDINATE_WRITE_ATTENUATED_LIGHT_COLOR_COLUMN(lightIndex, light, lightScratch, colorMtx);

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordParentFrameLightMatrixScratch);
}

/// Selects the nearest point or cone light to world position `arg0`, using
/// squared distance after halving each coordinate difference. Initializes
/// `nearestLight` to no selection even when `_worldCoordGetRoomLights` returns 0.
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

    roomLights            = _worldCoordGetRoomLights(&gGameSession->location.loc);
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

/// Writes one model light-direction row and RGB column from a composed directional source.
///
/// `lightIndex` is 0..2. `model` supplies writable lightMtx and colorMtx;
/// their other rows/columns and translations are preserved. The RGB column
/// scales signed Q12 colour by the source's previously computed attenuation.
/// Normalizes the composed translation as a direction; its following word
/// must be readable for the normalizer. `unusedObjectPosition` is ignored
/// and may be NULL.
/// Requires 28 scratch bytes plus 24 for normalization, released before
/// return; changes GTE state. Inlined into the model-light selection query.
static __inline__ void _worldCoordWriteDirectionalLightMatrix(s32 lightIndex, const WorldCoordLight* light, const VECTOR* unusedObjectPosition, const TmdObject* model)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        lightMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    lightMtx     = model->lightMtx;
    colorMtx     = model->colorMtx;
    gfxNormalizeLightDirection(light->transform.coord.workm.t, &lightScratch->result.direction);

    lightMtx->m[lightIndex][0] = lightScratch->result.direction.vx;
    lightMtx->m[lightIndex][1] = lightScratch->result.direction.vy;
    lightMtx->m[lightIndex][2] = lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    WORLD_COORDINATE_WRITE_ATTENUATED_LIGHT_COLOR_COLUMN(lightIndex, light, lightScratch, colorMtx);

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

/// Writes one model light-direction row and RGB column from a composed positional source.
///
/// `lightIndex` is 0..2. `model` supplies writable lightMtx and colorMtx;
/// their other rows/columns and translations are preserved. The RGB column
/// scales signed Q12 colour by the source's previously computed attenuation.
/// Sample xyz and source translation use the same game-coordinate frame.
/// Normalizes object minus light, then negates it into the direction row.
/// Point or cone eligibility and attenuation must already be evaluated;
/// only the common light header is used. Requires 28 scratch bytes plus 24
/// for normalization, released before return; changes GTE state. Inlined
/// into the model-light selection query.
static __inline__ void _worldCoordWritePositionalLightMatrix(s32 lightIndex, const WorldCoordLight* light, const VECTOR* objectPosition, const TmdObject* model)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        lightMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch                   = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    lightMtx                       = model->lightMtx;
    colorMtx                       = model->colorMtx;
    lightScratch->lightToObject.vx = objectPosition->vx - light->transform.coord.workm.t[0];
    lightScratch->lightToObject.vy = objectPosition->vy - light->transform.coord.workm.t[1];
    lightScratch->lightToObject.vz = objectPosition->vz - light->transform.coord.workm.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToObject, &lightScratch->result.direction);

    lightMtx->m[lightIndex][0] = -lightScratch->result.direction.vx;
    lightMtx->m[lightIndex][1] = -lightScratch->result.direction.vy;
    lightMtx->m[lightIndex][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    WORLD_COORDINATE_WRITE_ATTENUATED_LIGHT_COLOR_COLUMN(lightIndex, light, lightScratch, colorMtx);

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

/// Scores a directional light admitted by the current room view.
///
/// An all-view or matching source gets attenuation ONE and its weighted
/// signed Q12 RGB score plus the base score. A view mismatch returns zero and
/// retains attenuation. Uses no coordinates, scratch storage or GTE state.
/// Inlined into the model-light selection query.
static __inline__ s32 _worldCoordScoreDirectionalLight(WorldCoordLight* light)
{
    s16 viewId;

    viewId = light->transform.lighting.viewId;
    if (viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS && gGameSession->location.loc.view != viewId) {
        return 0;
    }
    {
        s32 r, g, b, weightedRgb;
        r                                     = light->color.r;
        g                                     = light->color.g;
        b                                     = light->color.b;
        light->transform.lighting.attenuation = ONE;
        weightedRgb                           = r * WORLD_COORDINATE_LIGHT_SCORE_RED_WEIGHT + g * WORLD_COORDINATE_LIGHT_SCORE_GREEN_WEIGHT + b * WORLD_COORDINATE_LIGHT_SCORE_BLUE_WEIGHT;
        // Keep the RGB sum in a register before the inlined admission test.
        USE_REG3(weightedRgb, weightedRgb, weightedRgb);
        return (weightedRgb >> WORLD_COORDINATE_LIGHT_SCORE_RGB_SHIFT) + WORLD_COORDINATE_LIGHT_SCORE_BASE;
    }
}

/// Admits a positive contribution only when it outranks the ambient cutoff.
///
/// `rankedLights` addresses four descending entries and `cutoffLight` points
/// to its last entry. `sourceKind` is one of the ranked-light source kinds;
/// `light` is its borrowed common header, live through table consumption.
/// Ties retain the earlier source. Insertion starts at entry 2.
static __inline__ void _worldCoordAdmitRankedLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, _WorldCoordRankedLight* cutoffLight)
{
    if (contributionScore > 0 && cutoffLight->rank < contributionScore) {
        _worldCoordInsertRankedLight(rankedLights, contributionScore, sourceKind, light, WORLD_COORDINATE_RANKED_LIGHT_COUNT - 2);
    }
}
/// Admits a directional contribution against the query's ambient cutoff.
///
/// `rankedLights` must be `lightQuery->rankedLights` (four descending entries),
/// `sourceKind` must be WORLD_COORDINATE_RANKED_LIGHT_DIRECTIONAL and `light`
/// is a borrowed common header, live through consumption. Nonpositive scores
/// and cutoff ties are ignored. Insertion starts at entry 2.
static __inline__ void _worldCoordAdmitDirectionalLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, _WorldCoordLightQueryScratch* lightQuery)
{
    if (contributionScore > 0 && lightQuery->rankedLights[ARRAY_SIZE(lightQuery->rankedLights) - 1].rank < contributionScore) {
        _worldCoordInsertRankedLight(rankedLights, contributionScore, sourceKind, light, WORLD_COORDINATE_RANKED_LIGHT_COUNT - 2);
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
    roomLights = _worldCoordGetRoomLights(&gGameSession->location.loc);
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
                val                 = _worldCoordScoreTransientPointLight(light, &block->viewPosition);
                block->contribution = val;
                _worldCoordAdmitRankedLight(block->rankedLights, val, WORLD_COORDINATE_RANKED_LIGHT_TRANSIENT_POINT, &light->head, last);
            }
            pointIndex++;
            lightSlot++;
        } while (pointIndex < ARRAY_SIZE(gWorldCoordTransientPointLights));
    }

    if (roomLights->pointLightCount > 0) {
        light = roomLights->pointLights;
        for (i = 0; i < roomLights->pointLightCount; i++, light++) {
            val                 = _worldCoordScoreRoomPointLight(light, &block->viewPosition);
            block->contribution = val;
            _worldCoordAdmitRankedLight(block->rankedLights, val, WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT, &light->head, &block->rankedLights[ARRAY_SIZE(block->rankedLights) - 1]);
        }
    }

    if (roomLights->coneLightCount > 0) {
        register WorldCoordSpotLight* spot;
        s32                           coneKind;

        spot = roomLights->coneLights;
        i    = 0;

        for (; i < roomLights->coneLightCount;) {
            val                 = _worldCoordScoreConeLight(spot, &block->viewPosition);
            coneKind            = WORLD_COORDINATE_RANKED_LIGHT_CONE;
            block->contribution = val;
            _worldCoordAdmitRankedLight(block->rankedLights, val, coneKind, &spot->head, &block->rankedLights[ARRAY_SIZE(block->rankedLights) - 1]);
            i++;
            spot++;
        }
    }

    if (roomLights->directionalLightCount > 0) {
        register WorldCoordLight* directionalLight;

        directionalLight = roomLights->directionalLights;
        i                = 0;
        for (; i < roomLights->directionalLightCount;) {
            val                 = _worldCoordScoreDirectionalLight(directionalLight);
            block->contribution = val;
            _worldCoordAdmitDirectionalLight(block->rankedLights, val, WORLD_COORDINATE_RANKED_LIGHT_DIRECTIONAL, directionalLight, block);
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
                            _worldCoordWritePositionalLightMatrix(i, block->rankedLights[i].light, &block->viewPosition, extra);
                            break;
                        case WORLD_COORDINATE_RANKED_LIGHT_CONE:
                            _worldCoordWritePositionalLightMatrix(i, block->rankedLights[i].light, &block->viewPosition, extra);
                            break;
                        default:
                            _worldCoordWriteDirectionalLightMatrix(i, block->rankedLights[i].light, &block->viewPosition, extra);
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

        ambientEntry = _worldCoordGetRoomAmbientEntry(&gGameSession->location.loc);
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

/// Stores a point light's attenuation and contribution score at its parent's origin.
///
/// Samples the light's local translation in game-coordinate units, without a
/// view-filter test or coordinate composition. Halved signed offsets supply
/// squared distance; inner/outer radii use the same units. Attenuation is ONE
/// within the inner radius, zero outside the outer radius and a Q12 remaining
/// squared-distance-span ratio between them. The signed weighted RGB score is
/// scaled by that attenuation and stored in composed.t[0], replacing cached X.
/// Requires 32 free scratch-stack bytes, released before return. Equal radii
/// retain full strength at their boundary; the reduction and truncation order
/// are part of the retained calculation.
static void _worldCoordEvaluatePointLightAtParentOrigin(WorldCoordPointLight* light)
{
/// Applies the inner-relative squared-distance fade to a point-light score.
///
/// falloff is a side-effect-free scratch pointer expression with all four
/// scalars initialized, the sample inside outerLimit and attenuation ONE.
/// contributionScore is a distinct s32 lvalue, overwritten with the result.
/// Expands to a compound statement; use only as a standalone statement inside
/// braces. Updates the scratch scalars using unsigned Q12 span arithmetic.
#define WORLD_COORDINATE_APPLY_POINT_LIGHT_FADE(falloff, contributionScore)                                                                                              \
    {                                                                                                                                                                    \
        if ((falloff)->distanceSquared > (falloff)->innerRadiusSquared) {                                                                                                \
            (falloff)->outerLimit      -= (falloff)->innerRadiusSquared;                                                                                                 \
            (falloff)->distanceSquared -= (falloff)->innerRadiusSquared;                                                                                                 \
            while ((falloff)->outerLimit > WORLD_COORDINATE_LIGHT_FALLOFF_MAX_SPAN) {                                                                                    \
                (falloff)->outerLimit      >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;                                                                           \
                (falloff)->distanceSquared >>= WORLD_COORDINATE_LIGHT_FALLOFF_REDUCTION_SHIFT;                                                                           \
            }                                                                                                                                                            \
            if ((falloff)->outerLimit != 0) {                                                                                                                            \
                (falloff)->attenuation = (((falloff)->outerLimit - (falloff)->distanceSquared) << WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS) / (falloff)->outerLimit; \
                (contributionScore)    = ((falloff)->attenuation * (contributionScore)) >> WORLD_COORDINATE_LIGHT_FALLOFF_FRACTION_BITS;                                 \
            }                                                                                                                                                            \
        }                                                                                                                                                                \
    }

    _WorldCoordPointLightFalloffScratch* falloff;
    s32                                  contributionScore;
    WorldCoordLight*                     header;

    header                   = &light->head;
    contributionScore        = 0;
    falloff                  = SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordPointLightFalloffScratch);
    falloff->halfOffset.vx   = header->transform.lighting.local.t[0] >> 1;
    falloff->halfOffset.vy   = header->transform.lighting.local.t[1] >> 1;
    falloff->halfOffset.vz   = header->transform.lighting.local.t[2] >> 1;
    falloff->distanceSquared = falloff->halfOffset.vx * falloff->halfOffset.vx + falloff->halfOffset.vy * falloff->halfOffset.vy + falloff->halfOffset.vz * falloff->halfOffset.vz;
    falloff->outerLimit      = (light->outer * light->outer) >> 2;
    falloff->attenuation     = 0;
    if (falloff->outerLimit >= falloff->distanceSquared) {
        falloff->innerRadiusSquared = (light->inner * light->inner) >> 2;
        contributionScore           = ((light->head.color.r * WORLD_COORDINATE_LIGHT_SCORE_RED_WEIGHT + light->head.color.g * WORLD_COORDINATE_LIGHT_SCORE_GREEN_WEIGHT + light->head.color.b * WORLD_COORDINATE_LIGHT_SCORE_BLUE_WEIGHT) >> WORLD_COORDINATE_LIGHT_SCORE_RGB_SHIFT) + WORLD_COORDINATE_LIGHT_SCORE_BASE;
        falloff->attenuation        = ONE;
        WORLD_COORDINATE_APPLY_POINT_LIGHT_FADE(falloff, contributionScore);
#undef WORLD_COORDINATE_APPLY_POINT_LIGHT_FADE
    }
    // Publish the attenuation and repurpose cached X as the contribution score.
    header->transform.lighting.attenuation   = falloff->attenuation;
    header->transform.lighting.composed.t[0] = contributionScore;
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

void worldCoordSetAmbientColorOverride(const SVECTOR* ambientColor)
{
    enum { WORLD_COORDINATE_AMBIENT_OVERRIDE_DISABLED = 0,
           WORLD_COORDINATE_AMBIENT_OVERRIDE_ENABLED  = 1 };

    if (ambientColor == NULL) {
        Gp_OverrideVecFlag = WORLD_COORDINATE_AMBIENT_OVERRIDE_DISABLED;
        return;
    }
    Gp_OverrideVecFlag = WORLD_COORDINATE_AMBIENT_OVERRIDE_ENABLED;
    Gp_OverrideVec     = *ambientColor;
}

void worldCoordSetLightColorScaleOverride(const SVECTOR* colorScales)
{
    enum { WORLD_COORDINATE_LIGHT_COLOR_OVERRIDE_DISABLED = 0,
           WORLD_COORDINATE_LIGHT_COLOR_OVERRIDE_ENABLED  = 1 };

    if (colorScales == NULL) {
        Gp_OverrideVec2Flag = WORLD_COORDINATE_LIGHT_COLOR_OVERRIDE_DISABLED;
        return;
    }
    Gp_OverrideVec2Flag         = WORLD_COORDINATE_LIGHT_COLOR_OVERRIDE_ENABLED;
    Gp_OverrideVec2.inputVector = *colorScales;
}

void worldCoordSetModelAmbientColor(const TmdObject* model, s16 r, s16 g, s16 b)
{
    MATRIX* colorMtx;

    colorMtx       = model->colorMtx;
    colorMtx->t[0] = r;
    colorMtx->t[1] = g;
    colorMtx->t[2] = b;
}

/// Borrows the minimum ambient-light entry for a room view, or the default entry.
///
/// `location` supplies valid 1-based stage, area and room indices within their
/// loaded tables, and normally a 1-based view. A missing stage/area room table,
/// missing ambient table or view above its signed count returns the default.
/// The retained zero-view case selects the table header when its count admits
/// zero. Room entries must not outlive the loaded room overlay; the default
/// entry lives with gameplay. No bounds are checked beyond the view count.
static const WorldCoordRoomAmbientEntry* _worldCoordGetRoomAmbientEntry(const GameLocationKey* location)
{
    WorldCoordRoomLighting*           roomLighting;
    const WorldCoordRoomAmbientEntry* ambientEntry;
    const WorldCoordRoomAmbientEntry* ambientTable;

    roomLighting = _worldCoordLookupRoomLighting(location);
    ambientEntry = &Gp_RoomBoundDefault;
    if (roomLighting != NULL) {
        ambientTable = roomLighting->ambientTable;
        if (ambientTable != NULL) {
            if (ambientTable->viewCount >= location->view) {
                ambientEntry = &ambientTable[location->view];
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

/// Borrows the authored light collection selected by stage, area and room.
///
/// `location` contains valid 1-based indices within the loaded tables. A null
/// stage/area table or room light collection returns NULL. The collection and
/// its source arrays must not be retained after their room overlay unloads.
/// Does not compose lights, check view eligibility or allocate storage.
static WorldCoordRoomLights* _worldCoordGetRoomLights(const GameLocationKey* location)
{
    WorldCoordRoomLighting* roomLighting;
    WorldCoordRoomLights*   roomLights;

    roomLights   = NULL;
    roomLighting = _worldCoordLookupRoomLighting(location);
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

/// Scores a directional light admitted by the current room view.
///
/// A matching view or WORLD_COORDINATE_LIGHT_ALL_VIEWS sets the light's Q12
/// attenuation to ONE and returns its weighted RGB score plus the base score.
/// A rejected view returns zero and retains the previous attenuation. Source
/// RGB values are signed Q12 intensities; no coordinate or scratch state is used.
static s32 _worldCoordScoreDirectionalLightOutOfLine(WorldCoordLight* light)
{
    s16 viewId;

    viewId = light->transform.lighting.viewId;
    if (viewId != WORLD_COORDINATE_LIGHT_ALL_VIEWS && gGameSession->location.loc.view != viewId) {
        return 0;
    }
    light->transform.lighting.attenuation = ONE;
    return ((light->color.r * WORLD_COORDINATE_LIGHT_SCORE_RED_WEIGHT + light->color.g * WORLD_COORDINATE_LIGHT_SCORE_GREEN_WEIGHT + light->color.b * WORLD_COORDINATE_LIGHT_SCORE_BLUE_WEIGHT) >> WORLD_COORDINATE_LIGHT_SCORE_RGB_SHIFT) + WORLD_COORDINATE_LIGHT_SCORE_BASE;
}

/// Returns a coordinate's cached local-origin X in its composition-root frame.
///
/// `coord->workm` must already be composed. The result is in game-coordinate
/// units; the root may be the view or an explicitly excluded ancestor. This
/// reads the cache without composing the coordinate or changing GTE state.
static s32 _worldCoordGetOriginComposedX(const GfxCoord* coord)
{
    return coord->workm.t[0];
}

/// Writes one model light row and RGB column from a composed directional source.
///
/// `lightIndex` is 0..2; model lightMtx and colorMtx must be writable 3x3
/// matrices. The source's composed translation is the direction in the model
/// lighting frame and must have its following word readable for normalization.
/// The colour column scales signed RGB intensities by the stored Q12
/// attenuation. `unusedObjectPosition` is ignored and may be NULL. Other
/// matrix rows/columns and translations are preserved. Requires 28 scratch
/// bytes plus 24 for normalization, released before return; changes GTE state.
///
/// Retained out-of-line entry with no current call sites.
static void _worldCoordWriteDirectionalLightMatrixOutOfLine(s32 lightIndex, const WorldCoordLight* light, const VECTOR* unusedObjectPosition, const TmdObject* model)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx       = model->lightMtx;
    colorMtx     = model->colorMtx;
    gfxNormalizeLightDirection(light->transform.coord.workm.t, &lightScratch->result.direction);

    dirMtx->m[lightIndex][0] = lightScratch->result.direction.vx;
    dirMtx->m[lightIndex][1] = lightScratch->result.direction.vy;
    dirMtx->m[lightIndex][2] = lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    WORLD_COORDINATE_WRITE_ATTENUATED_LIGHT_COLOR_COLUMN(lightIndex, light, lightScratch, colorMtx);

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

/// Writes one model light row and RGB column from a composed point source.
///
/// `lightIndex` is 0..2; model lightMtx and colorMtx must be writable 3x3
/// matrices. `objectPosition` xyz and the light's composed translation use
/// game-coordinate units in the same lighting frame. Normalizes object minus
/// light, then negates it into the row. The RGB column uses the source's stored
/// Q12 attenuation, computed before this call. Other rows/columns and matrix
/// translations are preserved. Requires 28 scratch bytes plus 24 for
/// normalization, released before return; changes GTE state.
///
/// Retained out-of-line entry with no current call sites.
static void _worldCoordWritePointLightMatrixOutOfLine(s32 lightIndex, const WorldCoordLight* light, const VECTOR* objectPosition, const TmdObject* model)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch                   = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx                         = model->lightMtx;
    colorMtx                       = model->colorMtx;
    lightScratch->lightToObject.vx = objectPosition->vx - light->transform.coord.workm.t[0];
    lightScratch->lightToObject.vy = objectPosition->vy - light->transform.coord.workm.t[1];
    lightScratch->lightToObject.vz = objectPosition->vz - light->transform.coord.workm.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToObject, &lightScratch->result.direction);

    dirMtx->m[lightIndex][0] = -lightScratch->result.direction.vx;
    dirMtx->m[lightIndex][1] = -lightScratch->result.direction.vy;
    dirMtx->m[lightIndex][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    WORLD_COORDINATE_WRITE_ATTENUATED_LIGHT_COLOR_COLUMN(lightIndex, light, lightScratch, colorMtx);

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

/// Writes one model light row and RGB column from a composed cone source.
///
/// `lightIndex` is 0..2; model lightMtx and colorMtx must be writable 3x3
/// matrices. `objectPosition` xyz and the light's composed translation use
/// game-coordinate units in the same lighting frame. Normalizes object minus
/// light, then negates it into the row. The cone's eligibility and Q12
/// attenuation must already be evaluated; this call uses only its common
/// header. Other rows/columns and matrix translations are preserved. Requires
/// 28 scratch bytes plus 24 for normalization, released before return; changes
/// GTE state.
///
/// Retained out-of-line entry with no current call sites.
static void _worldCoordWriteConeLightMatrixOutOfLine(s32 lightIndex, const WorldCoordLight* light, const VECTOR* objectPosition, const TmdObject* model)
{
    _WorldCoordLightMatrixScratch* lightScratch;
    MATRIX*                        dirMtx;
    MATRIX*                        colorMtx;

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightMatrixScratch);
    lightScratch                   = SCRATCH_STACK_CURSOR(_WorldCoordLightMatrixScratch);
    dirMtx                         = model->lightMtx;
    colorMtx                       = model->colorMtx;
    lightScratch->lightToObject.vx = objectPosition->vx - light->transform.coord.workm.t[0];
    lightScratch->lightToObject.vy = objectPosition->vy - light->transform.coord.workm.t[1];
    lightScratch->lightToObject.vz = objectPosition->vz - light->transform.coord.workm.t[2];
    gfxNormalizeLightDirection(&lightScratch->lightToObject, &lightScratch->result.direction);

    dirMtx->m[lightIndex][0] = -lightScratch->result.direction.vx;
    dirMtx->m[lightIndex][1] = -lightScratch->result.direction.vy;
    dirMtx->m[lightIndex][2] = -lightScratch->result.direction.vz;

    // Reuse the direction storage for the attenuated RGB column.
    WORLD_COORDINATE_WRITE_ATTENUATED_LIGHT_COLOR_COLUMN(lightIndex, light, lightScratch, colorMtx);

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightMatrixScratch);
}

/// Inserts a positive light contribution into a descending four-entry table.
///
/// `slotIndex` is 0..3; admission helpers begin at 2, reserving entry 3 as the
/// ambient cutoff. The table must already be ordered. Nonpositive scores are
/// ignored; equal scores retain earlier sources first. Recursion walks toward
/// entry 0, copying complete records to the next entry before insertion.
/// `sourceKind` identifies the borrowed common header; the source must remain
/// live through consumption. Does not change source attenuation or allocate.
static void _worldCoordInsertRankedLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, s32 slotIndex)
{
    _WorldCoordRankedLight* slot;
    _WorldCoordRankedLight* nextSlot;

    if (contributionScore <= 0) {
        return;
    }

    // Keep the scaled index first to preserve the target addition operands.
    slot = (_WorldCoordRankedLight*)(slotIndex * sizeof(*rankedLights) + (s32)rankedLights);
    if (slot->rank < contributionScore) {
        if (slotIndex < WORLD_COORDINATE_RANKED_LIGHT_COUNT - 1) {
            slot[1] = *slot;
        }
        if (slotIndex > 0) {
            _worldCoordInsertRankedLight(rankedLights, contributionScore, sourceKind, light, slotIndex - 1);
        } else {
            rankedLights->rank  = contributionScore;
            rankedLights->kind  = sourceKind;
            rankedLights->light = light;
        }
    } else if (slotIndex < WORLD_COORDINATE_RANKED_LIGHT_COUNT - 1) {
        nextSlot        = slot + 1;
        nextSlot->rank  = contributionScore;
        slot[1].kind    = sourceKind;
        nextSlot->light = light;
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

/// Borrows the lighting descriptor selected by stage, area and room.
///
/// `location` supplies valid 1-based indices within the loaded tables. A null
/// stage or area room-table pointer returns NULL; otherwise the indexed room
/// descriptor is returned even when its light and ambient pointers are null.
/// The descriptor must not be retained after its owning overlay is unloaded.
static WorldCoordRoomLighting* _worldCoordGetRoomLighting(const GameLocationKey* location)
{
    return _worldCoordLookupRoomLighting(location);
}

void taskRunExitCallbackTask(Task* task)
{
    taskCallExit(task);
}

/// Copies the complete default ambient-light entry into caller-owned storage.
///
/// `ambientEntry` must point to one writable WorldCoordRoomAmbientEntry. The
/// eight-byte value is copied, so the destination has no borrowed lifetime.
static void _worldCoordCopyDefaultRoomAmbient(WorldCoordRoomAmbientEntry* ambientEntry)
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
        roomLights = _worldCoordGetRoomLights(&gGameSession->location.loc);
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

/// Places a collision body's local centre/origin in its cached composition frame.
///
/// `body->coord->workm` must already be composed. Rotates the local SVECTOR
/// offset and adds the cached translation, in game-coordinate units. Writes
/// only position's xyz; its final word is untouched. Requires an initialized
/// scratch stack with 48 free bytes, released before return; only the leading
/// VECTOR is accessed. Replaces GTE rotation and arithmetic state, retains no
/// pointer and does not compose or modify the body.
static __inline__ void _worldCollisionGetBodyComposedPosition(const WorldCollisionBody* body, VECTOR* position)
{
    // Only the leading VECTOR is accessed; the rest of the reservation is unproven.
    enum { WORLD_COLLISION_BODY_POSITION_SCRATCH_BYTES = 0x30 };

    u8*     scratchTop;
    VECTOR* rotatedOffset;
    scratchTop                 = SCRATCH_STACK_CURSOR(u8);
    rotatedOffset              = (VECTOR*)(scratchTop - WORLD_COLLISION_BODY_POSITION_SCRATCH_BYTES);
    SCRATCH_STACK_CURSOR(void) = rotatedOffset;
    gte_SetRotMatrix(&body->coord->workm);
    gte_ldv0(&body->pos);
    gte_rtv0();
    gte_stlvnl(rotatedOffset);
    position->vx = body->coord->workm.t[0] + rotatedOffset->vx;
    position->vy = body->coord->workm.t[1] + rotatedOffset->vy;
    position->vz = body->coord->workm.t[2] + rotatedOffset->vz;
    SCRATCH_STACK_RELEASE_BYTES(WORLD_COLLISION_BODY_POSITION_SCRATCH_BYTES);
}

/// The same lookup as `Gp_GetIdParam0`, returned at the tables' own width.
static inline u16 _gpIdParam0(s32 id)
{
    if ((id & 0x8000) == 0) {
        return Gp_IdParamLo[id & 0x7F].hitReaction;
    }
    return Gp_IdParamHi.rows[id & 0x7F].column.outcome.hitReaction;
}
