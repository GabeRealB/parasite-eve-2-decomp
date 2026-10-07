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

/// Exact gate value enabling a model-light override; all other values disable it.
enum { WORLD_COORDINATE_MODEL_LIGHT_OVERRIDE_ENABLED = 1 };

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

static void _worldCoordFindNearestRoomLight(const VECTOR* samplePosition, _WorldCoordNearestRoomLight* nearestLight);

static __inline__ void _worldCoordWriteDirectionalLightMatrix(s32 lightIndex, const WorldCoordLight* light, const VECTOR* unusedObjectPosition, const TmdObject* model);

static __inline__ void _worldCoordWritePositionalLightMatrix(s32 lightIndex, const WorldCoordLight* light, const VECTOR* objectPosition, const TmdObject* model);

static __inline__ s32 _worldCoordScoreDirectionalLight(WorldCoordLight* light);

static __inline__ void _worldCoordAdmitRankedLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, _WorldCoordRankedLight* cutoffLight);

static __inline__ void _worldCoordAdmitDirectionalLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, _WorldCoordLightQueryScratch* lightQuery);

/// Number of player-lighting task states (0 initialization, 1 recurring update).
enum { WORLD_COORDINATE_PLAYER_LIGHTING_STATE_COUNT = 2 };

/// No attachment-driven pulse is pending for the player's light-colour matrix.
enum { WORLD_COORDINATE_ATTACHMENT_LIGHT_PULSE_NONE = 0 };

static void _worldCoordUpdatePlayerLighting(Task* unusedTask);

static void _worldCoordRemapActorColor(Enemy* enemy, MATRIX* colorMtx, s32 colorMode);

static const WorldCoordRoomAmbientEntry* _worldCoordGetRoomAmbientEntry(const GameLocationKey* location);

static s32 _worldCoordCountActiveTransientPointLights(void);

static WorldCoordRoomLights* _worldCoordGetRoomLights(const GameLocationKey* location);

static void _worldCoordInsertRankedLight(_WorldCoordRankedLight* rankedLights, s32 contributionScore, s32 sourceKind, WorldCoordLight* light, s32 slotIndex);

static void _worldCoordFillLightColorMatrixOutOfLine(MATRIX* colorMtx, s16 r, s16 g, s16 b);

static void _worldCoordInitPlayerLighting(Task* task);

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

static inline u16 _objectFieldGetAttackHitReaction(s32 attackId);

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

/// Builds a cone light's local rotation around its authored aim direction.
///
/// `coneLight->axis` is in the local translation's frame, conventionally Q12.
/// Writes the light's local rotation, preserving translation, cached transform
/// and composition stamp; the caller must mark the coordinate dirty afterwards.
/// `yHintScratch` supplies a disjoint, halfword-aligned writable SVECTOR. Its
/// xyz become a perpendicular Y hint; its ignored pad is read but not written.
/// Requires another sizeof(MATRIX) bytes on the scratch stack for the basis
/// builder and changes GTE state. Zero axes have no fallback; cross products
/// must survive Q12 shifting and signed-halfword saturation for a useful basis.
static inline void _worldCoordBuildConeLightRotation(WorldCoordSpotLight* coneLight, SVECTOR* yHintScratch)
{
    GfxCoord* localCoord;

    localCoord = &coneLight->head.transform.coord;
    // Choose a perpendicular Y hint even when the aim lies along X.
    if (coneLight->axis.vy != 0 || coneLight->axis.vz != 0) {
        yHintScratch->vx = 0;
        yHintScratch->vy = -coneLight->axis.vz;
        yHintScratch->vz = coneLight->axis.vy;
    } else {
        yHintScratch->vx = coneLight->axis.vy;
        yHintScratch->vy = -coneLight->axis.vx;
        yHintScratch->vz = 0;
    }
    gfxBuildOrthonormalBasis(&localCoord->coord, &coneLight->axis, yHintScratch);
}

void worldCoordUpdateRoomLightsTask(Task* task)
{
    // Only the leading SVECTOR is identified; retain the full scratch reservation.
    enum { WORLD_COORDINATE_ROOM_LIGHT_SCRATCH_BYTES = 0x1C,
           WORLD_COORDINATE_ROOM_LIGHT_INIT          = 0 };

    WorldCoordRoomLights* roomLights;
    SVECTOR*              axisHint;
    WorldCoordLight*      directionalLight;
    WorldCoordPointLight* pointLight;
    WorldCoordSpotLight*  coneLight;
    GfxCoord*             coord;
    s32                   lightIndex;
    s32                   transientIndex;

    roomLights = _worldCoordGetRoomLights(&gGameSession->location.loc);
    if (roomLights == NULL) {
        taskKill(task);
        return;
    }

    axisHint = SCRATCH_STACK_RESERVE_BYTES(WORLD_COORDINATE_ROOM_LIGHT_SCRATCH_BYTES);
    if (task->state == WORLD_COORDINATE_ROOM_LIGHT_INIT) {
        pointLight = roomLights->pointLights;
        for (lightIndex = 0; lightIndex < roomLights->pointLightCount; lightIndex++, pointLight++) {
            coord               = &pointLight->head.transform.coord;
            coord->parent       = &gGfxViewCoord;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }

        coneLight = roomLights->coneLights;
        for (lightIndex = 0; lightIndex < roomLights->coneLightCount; lightIndex++, coneLight++) {
            coord         = &coneLight->head.transform.coord;
            coord->parent = &gGfxViewCoord;
            _worldCoordBuildConeLightRotation(coneLight, axisHint);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }

        if (roomLights->directionalLightCount > 0) {
            WorldCoordLight* directionalLightEntry;

            directionalLightEntry = roomLights->directionalLights;
            for (lightIndex = 0; lightIndex < roomLights->directionalLightCount; lightIndex++, directionalLightEntry++) {
                coord               = &directionalLightEntry->transform.coord;
                coord->parent       = &gGfxViewCoord;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            }
        }

        // Drop previous transient contributions before composing the room's lighting.
        for (transientIndex = 0; transientIndex < ARRAY_SIZE(gWorldCoordTransientPointLights); transientIndex++) {
            gWorldCoordTransientPointLights[transientIndex].framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
            coord                                                      = &gWorldCoordTransientPointLights[transientIndex].light.head.transform.coord;
            coord->parent                                              = &gGfxViewCoord;
        }

        task->state++;
    }

    // Keep the view composed, but exclude it from each light's cached transform.
    actorRenderComposeCoord(&gGfxViewCoord);

    _worldCoordComposeTransientPointLights();

    pointLight = roomLights->pointLights;
    for (lightIndex = 0; lightIndex < roomLights->pointLightCount; lightIndex++, pointLight++) {
        coord = &pointLight->head.transform.coord;
        actorRenderComposeCoordRelative(coord, &gGfxViewCoord);
    }

    coneLight = roomLights->coneLights;
    for (lightIndex = 0; lightIndex < roomLights->coneLightCount; lightIndex++, coneLight++) {
        coord = &coneLight->head.transform.coord;
        actorRenderComposeCoordRelative(coord, &gGfxViewCoord);
    }

    if (roomLights->directionalLightCount > 0) {
        directionalLight = roomLights->directionalLights;
        for (lightIndex = 0; lightIndex < roomLights->directionalLightCount; lightIndex++, directionalLight++) {
            coord = &directionalLight->transform.coord;
            actorRenderComposeCoordRelative(coord, &gGfxViewCoord);
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(WORLD_COORDINATE_ROOM_LIGHT_SCRATCH_BYTES);
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

/// Measures squared distance to a cached light after halving each coordinate difference.
///
/// Reads signed xyz from `samplePosition` and the composed light translation.
/// A geometric distance requires both to share a game-coordinate frame; no
/// composition or frame conversion is performed. Writes light minus sample
/// into `halfOffset`, arithmetically halved with negative odd values rounded
/// down. Each offset unit spans two game-coordinate units; each squared-sum
/// unit spans four squared game-coordinate units, with rounding before squaring.
///
/// Retains signed 32-bit subtraction, products and sums, returning their target
/// word as u32 without overflow checks. Inputs and writable word-aligned scratch
/// must be disjoint. Only xyz are accessed; both VECTOR pads are untouched.
/// Allocates no scratch, retains no pointer and changes no GTE state.
static inline u32 _worldCoordGetHalfScaleLightDistanceSquared(const WorldCoordLight* lightHeader, const VECTOR* samplePosition, VECTOR* halfOffset)
{
    enum { WORLD_COORDINATE_LIGHT_DISTANCE_COMPONENT_SHIFT = 1 };

    halfOffset->vx = (lightHeader->transform.lighting.composed.t[0] - samplePosition->vx) >> WORLD_COORDINATE_LIGHT_DISTANCE_COMPONENT_SHIFT;
    halfOffset->vy = (lightHeader->transform.lighting.composed.t[1] - samplePosition->vy) >> WORLD_COORDINATE_LIGHT_DISTANCE_COMPONENT_SHIFT;
    halfOffset->vz = (lightHeader->transform.lighting.composed.t[2] - samplePosition->vz) >> WORLD_COORDINATE_LIGHT_DISTANCE_COMPONENT_SHIFT;
    return halfOffset->vx * halfOffset->vx + halfOffset->vy * halfOffset->vy + halfOffset->vz * halfOffset->vz;
}

/// Selects the closest authored room point or cone light by cached translation.
///
/// Reads only samplePosition's signed xyz, without composing or converting its
/// frame. Room-light transforms must already be composed; meaningful geometric
/// distances require the sample and candidates to share that frame. Clears the
/// result even when the room has no lights. The borrowed selection remains valid
/// only while its room overlay is loaded; no view, cone or intensity filter applies.
///
/// Differences are arithmetically halved, squared as signed words, then compared
/// as an unsigned sum strictly below 0x7FFFFFFF. Equal distances keep the earlier
/// source, with points before cones. Requires one VECTOR of scratch storage,
/// released before return; its fourth word is unused. Changes no GTE state.
static void _worldCoordFindNearestRoomLight(const VECTOR* samplePosition, _WorldCoordNearestRoomLight* nearestLight)
{
    enum { WORLD_COORDINATE_NEAREST_LIGHT_DISTANCE_LIMIT = 0x7FFFFFFF };

    WorldCoordRoomLights* roomLights;
    WorldCoordPointLight* pointLight;
    WorldCoordLight*      lightHeader;
    WorldCoordSpotLight*  coneLight;
    VECTOR*               halfOffset;
    u32                   nearestDistanceSquared;
    u32                   distanceSquared;
    s32                   lightIndex;

    roomLights             = _worldCoordGetRoomLights(&gGameSession->location.loc);
    nearestDistanceSquared = WORLD_COORDINATE_NEAREST_LIGHT_DISTANCE_LIMIT;
    nearestLight->kind     = WORLD_COORDINATE_NEAREST_LIGHT_NONE;
    nearestLight->field_4  = 0;
    nearestLight->light    = NULL;
    if (roomLights != NULL) {
        SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
        halfOffset = SCRATCH_STACK_CURSOR(VECTOR);
        if (roomLights->pointLightCount > 0) {
            pointLight = roomLights->pointLights;
            for (lightIndex = 0; lightIndex < roomLights->pointLightCount; lightIndex++, pointLight++) {
                lightHeader     = &pointLight->head;
                distanceSquared = _worldCoordGetHalfScaleLightDistanceSquared(lightHeader, samplePosition, halfOffset);
                if (distanceSquared < nearestDistanceSquared) {
                    nearestDistanceSquared = distanceSquared;
                    nearestLight->kind     = WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT;
                    nearestLight->light    = lightHeader;
                }
            }
        }
        if (roomLights->coneLightCount > 0) {
            coneLight = roomLights->coneLights;
            for (lightIndex = 0; lightIndex < roomLights->coneLightCount; lightIndex++, coneLight++) {
                lightHeader     = &coneLight->head;
                distanceSquared = _worldCoordGetHalfScaleLightDistanceSquared(lightHeader, samplePosition, halfOffset);
                if (distanceSquared < nearestDistanceSquared) {
                    nearestDistanceSquared = distanceSquared;
                    nearestLight->kind     = WORLD_COORDINATE_RANKED_LIGHT_CONE;
                    nearestLight->light    = lightHeader;
                }
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
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
/// Scales one RGB channel's three model-light coefficients in place.
///
/// `colorRow` borrows three writable signed Q12 coefficients in light-slot
/// order. `channelScale` borrows one raw scale halfword: it is loaded unsigned
/// after staging the row, then interpreted as signed Q12 by GTE IR0 (zero
/// suppresses, `ONE` is unity). GPF12 arithmetically shifts each product by 12
/// and saturates to -32768..32767.
///
/// `coefficientScratch` must be the `viewOffset` member of a live
/// `_WorldCoordLightQueryScratch`, disjoint from the row and scale. Its xyz
/// stage the coefficients; its final halfword and all other query bytes are
/// untouched. All storage is borrowed until return. Allocates no scratch
/// storage; changes GTE IR0..3, MAC1..3, RGB FIFO and FLAG.
static inline void _worldCoordScaleLightColorRow(s16 colorRow[3], const u16* channelScale, SVECTOR* coefficientScratch)
{
    _WorldCoordLightQueryScratch* queryScratch;

    // Keep scalar accesses relative to the query while GTE uses its vector member.
    queryScratch                = PARENT_OF(coefficientScratch, _WorldCoordLightQueryScratch, viewOffset);
    queryScratch->viewOffset.vx = colorRow[0];
    queryScratch->viewOffset.vy = colorRow[1];
    queryScratch->viewOffset.vz = colorRow[2];
    gte_lddp(*channelScale);
    gte_ldsv(coefficientScratch);
    gte_gpf12();
    gte_stsv(coefficientScratch);
    colorRow[0] = queryScratch->viewOffset.vx;
    colorRow[1] = queryScratch->viewOffset.vy;
    colorRow[2] = queryScratch->viewOffset.vz;
}

/// Applies the stored RGB scales to a model's light-colour coefficients.
///
/// `colorMtx` supplies nine writable signed Q12 coefficients: each RGB row
/// holds that channel's contributions from the three lights. Stored scales are
/// read as unsigned Q12 halfwords (zero suppresses, ONE is unity). GTE GPF12
/// shifts each product by 12 and saturates the result to a signed halfword.
/// Ambient translation is preserved.
///
/// The caller checks the override gate before calling. `lightQuery` borrows the
/// live query workspace until return, after position and direction calculations
/// have finished. Its `viewOffset` xyz are overwritten; the final halfword and
/// the rest of the query are untouched. Matrix and override storage must be
/// disjoint from the workspace. Allocates no scratch storage and changes GTE
/// IR0..3, MAC1..3, RGB FIFO and FLAG.
static inline void _worldCoordApplyModelLightColorScales(MATRIX* colorMtx, _WorldCoordLightQueryScratch* lightQuery)
{
    const u16* channelScale;
    s16(*colorRows)[3];
    s32 channelIndex;

    channelScale = Gp_OverrideVec2.channelScales;
    channelIndex = 0;
    colorRows    = colorMtx->m;
    do {
        _worldCoordScaleLightColorRow(colorRows[channelIndex], channelScale, &lightQuery->viewOffset);
        channelIndex++;
        channelScale++;
    } while (channelIndex < (s32)ARRAY_SIZE(Gp_OverrideVec2.channelScales));
}

void worldCoordSetModelLighting(const TmdObject* model, const void* worldPosition, s32 firstLightIndex, s32 lightCount)
{
    // Q12 attenuation differences below one eighth blend the weakest selected light into ambient.
    enum {
        WORLD_COORDINATE_LIGHT_BLEND_THRESHOLD     = ONE / 8,
        WORLD_COORDINATE_LIGHT_BLEND_FRACTION_BITS = 9,
        WORLD_COORDINATE_LIGHT_AMBIENT_RANK_SHIFT  = 2,
        WORLD_COORDINATE_LIGHT_AMBIENT_COLOR_SHIFT = 6
    };

    const VECTOR3* position;

    register WorldCoordRoomLights*         roomLights;
    register MATRIX*                       colorMtx;
    register _WorldCoordLightQueryScratch* lightQuery;

    s32                   sourceCount;
    s32                   activeTransientCount;
    s32                   transientIndex;
    s32                   lightIndex;
    s32                   requestedEnd;
    s32                   contributionScore;
    WorldCoordPointLight* pointLight;

    roomLights           = _worldCoordGetRoomLights(&gGameSession->location.loc);
    colorMtx             = model->colorMtx;
    activeTransientCount = 0;
    if (roomLights == NULL) {
        return;
    }

    sourceCount = roomLights->directionalLightCount + roomLights->pointLightCount + roomLights->coneLightCount;
    for (transientIndex = 0; transientIndex < ARRAY_SIZE(gWorldCoordTransientPointLights); transientIndex++) {
        if (gWorldCoordTransientPointLights[transientIndex].framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            activeTransientCount++;
        }
    }

    requestedEnd = firstLightIndex + lightCount;
    sourceCount += activeTransientCount;
    if ((u32)requestedEnd >= ARRAY_SIZE(lightQuery->rankedLights)) {
        return;
    }
    if (lightCount == 0) {
        return;
    }

    // Clear every light-colour coefficient before reducing an oversized request.
    _worldCoordFillLightColorMatrixOutOfLine(colorMtx, 0, 0, 0);

    if ((u32)(requestedEnd - 1) >= (u32)sourceCount) {
        worldCoordSetModelLighting(model, worldPosition, firstLightIndex, lightCount - 1);
        return;
    }

    SCRATCH_STACK_RESERVE_BLOCK(_WorldCoordLightQueryScratch);
    lightQuery = SCRATCH_STACK_CURSOR(_WorldCoordLightQueryScratch);

    {
        s32 columnIndex;

        columnIndex = firstLightIndex;
        for (; (u32)columnIndex < (u32)lightCount;) {
            colorMtx->m[0][columnIndex] = 0;
            colorMtx->m[1][columnIndex] = 0;
            colorMtx->m[2][columnIndex] = 0;
            columnIndex++;
        }
    }

    {

        lightIndex = 0;
        do {
            lightQuery->rankedLights[lightIndex].rank  = WORLD_COORDINATE_RANKED_LIGHT_EMPTY_RANK;
            lightQuery->rankedLights[lightIndex].light = NULL;
            lightIndex++;
        } while (lightIndex < (s32)ARRAY_SIZE(lightQuery->rankedLights));
    }

    // Keep the caller's world position, then rotate the view-relative offset into view space.
    position                    = worldPosition;
    lightQuery->viewPosition.vx = position->vx;
    lightQuery->viewPosition.vy = position->vy;
    lightQuery->viewPosition.vz = position->vz;
    lightQuery->viewOffset.vx   = position->vx - gGfxViewCoord.workm.t[0];
    lightQuery->viewOffset.vy   = position->vy - gGfxViewCoord.workm.t[1];
    lightQuery->viewOffset.vz   = position->vz - gGfxViewCoord.workm.t[2];
    gte_TransposeMatrix(&gGfxViewCoord.workm, &lightQuery->viewRotation);

    _gfxLoadRotSv(&lightQuery->viewRotation, &lightQuery->viewOffset);
    gte_rtv0();
    gte_stsv(&lightQuery->viewOffset);

    {
        s32                                     transientIndex;
        register WorldCoordTransientPointLight* transientSlot;

        register _WorldCoordRankedLight* cutoffEntry;

        // Rank active transient points alongside the room's authored lights.
        transientSlot  = gWorldCoordTransientPointLights;
        transientIndex = 0;
        cutoffEntry    = &lightQuery->rankedLights[ARRAY_SIZE(lightQuery->rankedLights) - 1];

        // Falloff and light directions read this rotated sample.
        lightQuery->viewPosition.vx = lightQuery->viewOffset.vx;
        lightQuery->viewPosition.vy = lightQuery->viewOffset.vy;
        lightQuery->viewPosition.vz = lightQuery->viewOffset.vz;
        do {
            if (transientSlot->framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
                pointLight               = &transientSlot->light;
                contributionScore        = _worldCoordScoreTransientPointLight(pointLight, &lightQuery->viewPosition);
                lightQuery->contribution = contributionScore;
                _worldCoordAdmitRankedLight(lightQuery->rankedLights, contributionScore, WORLD_COORDINATE_RANKED_LIGHT_TRANSIENT_POINT, &pointLight->head, cutoffEntry);
            }
            transientIndex++;
            transientSlot++;
        } while (transientIndex < ARRAY_SIZE(gWorldCoordTransientPointLights));
    }

    if (roomLights->pointLightCount > 0) {
        pointLight = roomLights->pointLights;
        for (lightIndex = 0; lightIndex < roomLights->pointLightCount; lightIndex++, pointLight++) {
            contributionScore        = _worldCoordScoreRoomPointLight(pointLight, &lightQuery->viewPosition);
            lightQuery->contribution = contributionScore;
            _worldCoordAdmitRankedLight(lightQuery->rankedLights, contributionScore, WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT, &pointLight->head, &lightQuery->rankedLights[ARRAY_SIZE(lightQuery->rankedLights) - 1]);
        }
    }

    if (roomLights->coneLightCount > 0) {
        register WorldCoordSpotLight* coneLight;
        s32                           coneKind;

        coneLight  = roomLights->coneLights;
        lightIndex = 0;

        for (; lightIndex < roomLights->coneLightCount;) {
            contributionScore        = _worldCoordScoreConeLight(coneLight, &lightQuery->viewPosition);
            coneKind                 = WORLD_COORDINATE_RANKED_LIGHT_CONE;
            lightQuery->contribution = contributionScore;
            _worldCoordAdmitRankedLight(lightQuery->rankedLights, contributionScore, coneKind, &coneLight->head, &lightQuery->rankedLights[ARRAY_SIZE(lightQuery->rankedLights) - 1]);
            lightIndex++;
            coneLight++;
        }
    }

    if (roomLights->directionalLightCount > 0) {
        register WorldCoordLight* directionalLight;

        directionalLight = roomLights->directionalLights;
        lightIndex       = 0;
        for (; lightIndex < roomLights->directionalLightCount;) {
            contributionScore        = _worldCoordScoreDirectionalLight(directionalLight);
            lightQuery->contribution = contributionScore;
            _worldCoordAdmitDirectionalLight(lightQuery->rankedLights, contributionScore, WORLD_COORDINATE_RANKED_LIGHT_DIRECTIONAL, directionalLight, lightQuery);
            lightIndex++;
            directionalLight++;
        }
    }

    // The next ranked source supplies ambient; nearby attenuation ranks share the weakest light.
    colorMtx->t[2] = 0;
    colorMtx->t[1] = 0;
    colorMtx->t[0] = 0;

    {

        s32 cutoffIndex;

        WorldCoordLight* selectedLight;
        WorldCoordLight* ambientLight;

        s32 ambientAttenuation;
        s32 ambientLevel;

        lightIndex = firstLightIndex;
        if ((u32)lightIndex < (u32)lightCount) {
            cutoffIndex = lightIndex + lightCount;
            do {
                selectedLight = lightQuery->rankedLights[lightIndex].light;
                if (selectedLight != NULL) {
                    if (lightIndex == cutoffIndex - 1) {
                        if (lightQuery->rankedLights[lightCount].light != NULL) {
                            WorldCoordLight* cutoffLight;
                            s32              attenuation;
                            s32              residualAttenuation;
                            s32              cutoffAttenuation;
                            cutoffLight        = lightQuery->rankedLights[cutoffIndex].light;
                            ambientAttenuation = 0;
                            if (lightQuery->rankedLights[lightCount].kind != WORLD_COORDINATE_RANKED_LIGHT_DIRECTIONAL) {
                                attenuation         = selectedLight->transform.lighting.attenuation;
                                cutoffAttenuation   = cutoffLight->transform.lighting.attenuation;
                                residualAttenuation = attenuation - cutoffAttenuation;
                                if (residualAttenuation < 0) {
                                    residualAttenuation = 0;
                                }
                                if (residualAttenuation < WORLD_COORDINATE_LIGHT_BLEND_THRESHOLD) {
                                    residualAttenuation                           = (residualAttenuation * attenuation) >> WORLD_COORDINATE_LIGHT_BLEND_FRACTION_BITS;
                                    ambientAttenuation                            = attenuation - residualAttenuation;
                                    selectedLight->transform.lighting.attenuation = residualAttenuation;
                                }
                            }
                            ambientLight         = lightQuery->rankedLights[cutoffIndex].light;
                            ambientAttenuation >>= WORLD_COORDINATE_LIGHT_AMBIENT_RANK_SHIFT;
                            ambientLevel         = (lightQuery->rankedLights[cutoffIndex].rank >> WORLD_COORDINATE_LIGHT_AMBIENT_RANK_SHIFT) + ambientAttenuation;
                            colorMtx->t[2]       = ambientLevel;
                            colorMtx->t[1]       = ambientLevel;
                            colorMtx->t[0]       = ambientLevel;
                            colorMtx->t[0]       = ambientLevel + (ambientLight->color.r >> WORLD_COORDINATE_LIGHT_AMBIENT_COLOR_SHIFT);
                            colorMtx->t[1]       = colorMtx->t[1] + (ambientLight->color.g >> WORLD_COORDINATE_LIGHT_AMBIENT_COLOR_SHIFT);
                            colorMtx->t[2]       = colorMtx->t[2] + (ambientLight->color.b >> WORLD_COORDINATE_LIGHT_AMBIENT_COLOR_SHIFT);
                        }
                    }

                    switch (lightQuery->rankedLights[lightIndex].kind) {
                        case WORLD_COORDINATE_RANKED_LIGHT_ROOM_POINT:
                        case WORLD_COORDINATE_RANKED_LIGHT_TRANSIENT_POINT:
                            _worldCoordWritePositionalLightMatrix(lightIndex, lightQuery->rankedLights[lightIndex].light, &lightQuery->viewPosition, model);
                            break;
                        case WORLD_COORDINATE_RANKED_LIGHT_CONE:
                            _worldCoordWritePositionalLightMatrix(lightIndex, lightQuery->rankedLights[lightIndex].light, &lightQuery->viewPosition, model);
                            break;
                        default:
                            _worldCoordWriteDirectionalLightMatrix(lightIndex, lightQuery->rankedLights[lightIndex].light, &lightQuery->viewPosition, model);
                            break;
                    }
                }

                lightIndex++;

            } while ((u32)lightIndex < (u32)lightCount);
        }
    }

    // Apply the ambient override or the view's minimum RGB levels.
    if ((s8)Gp_OverrideVecFlag == WORLD_COORDINATE_MODEL_LIGHT_OVERRIDE_ENABLED) {
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

    if ((s8)Gp_OverrideVec2Flag == WORLD_COORDINATE_MODEL_LIGHT_OVERRIDE_ENABLED) {
        _worldCoordApplyModelLightColorScales(model->colorMtx, lightQuery);
    }

    // Retain the ranked snapshot before releasing the query's scratch storage.
    if (Pad_RemapState->diagnosticMode == GAME_DEBUG_DIAGNOSTIC_LIGHT_PROBE && D_80760618->captureEnabled == WORLD_COORDINATE_LIGHT_CAPTURE_ENABLED) {
        lightIndex = 0;
        do {
            D_80760618->rankedLights[lightIndex] = lightQuery->rankedLights[lightIndex];
            lightIndex++;
        } while (lightIndex < (s32)ARRAY_SIZE(D_80760618->rankedLights));
    }

    SCRATCH_STACK_RELEASE_BLOCK(_WorldCoordLightQueryScratch);
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

/// Refreshes player and companion lighting and shares it with their child models.
///
/// A missing player skips all updates. Each actor needs a live model with part 1
/// and writable light/colour matrices, plus live GameActor work. Optional child
/// tasks borrow the actor's matrix pair; those matrices must outlive the children.
/// The player uses the pair installed at initialization, the companion its own.
///
/// Samples cached part-1 xyz with Y reduced by 100 game-coordinate units after
/// full-chain composition. Cache reuse can affect the composition frame; no
/// explicit conversion precedes the lighting query. The light-probe mode captures
/// the player's ranks/nearest source and draws a marker after s16 narrowing.
/// Normal updates apply attachment/status tints to directional coefficients only.
/// The task argument is unused. Changes GTE state and borrows nested scratch blocks.
static void _worldCoordUpdatePlayerLighting(Task* unusedTask)
{
    enum {
        WORLD_COORDINATE_PLAYER_LIGHT_SAMPLE_Y_OFFSET = 100,
        WORLD_COORDINATE_PLAYER_LIGHT_COUNT           = 3,
        WORLD_COORDINATE_ATTACHMENT_PULSE_ANGLE_SHIFT = 6,
        WORLD_COORDINATE_ATTACHMENT_PULSE_BASE        = ONE + ONE / 2,
        WORLD_COORDINATE_ATTACHMENT_PULSE_LOW         = ONE / 8,
        WORLD_COORDINATE_STATUS_TINT_LOW              = ONE / 4,
        WORLD_COORDINATE_STATUS_TINT_HIGH             = ONE * 2,
        WORLD_COORDINATE_STATUS_TINT_PERIOD_FRAMES    = 3,
        WORLD_COORDINATE_LIGHT_PROBE_TEXT_OT_INDEX    = 4,
        WORLD_COORDINATE_LIGHT_PROBE_TEXT_RGB         = 0x037A78
    };

    Task*                        playerTask;
    Task*                        companionTask;
    const PlayerStatus*          playerStatus;
    TmdObject*                   model;
    GfxCoord*                    coord;
    GameActor*                   playerActor;
    GameActor*                   companionActor;
    MATRIX*                      colorMtx;
    WorldCoordProjectionScratch* projection;
    SVECTOR*                     inputPoint;
    VECTOR                       samplePosition;
    TextDrawReq                  probeMarker;
    s32                          childIndex;
    s32                          pulseGreen;

    playerTask   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerStatus = &gPlayerStatus;
    if (playerTask == NULL) {
        return;
    }

    // Sample the retained composed translation without assuming a world-space cache.
    model = playerTask->extra.tmd;
    coord = &model->coords[1];
    actorRenderComposeCoord(coord);
    samplePosition.vx = coord->workm.t[0];
    samplePosition.vy = coord->workm.t[1] - WORLD_COORDINATE_PLAYER_LIGHT_SAMPLE_Y_OFFSET;
    samplePosition.vz = coord->workm.t[2];

    if (Pad_RemapState->diagnosticMode == GAME_DEBUG_DIAGNOSTIC_LIGHT_PROBE) {
        SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
        projection = SCRATCH_STACK_CURSOR(WorldCoordProjectionScratch);
        // Open the ranked snapshot gate only for the player's diagnostic sample.
        D_80760618->captureEnabled = WORLD_COORDINATE_LIGHT_CAPTURE_ENABLED;
        worldCoordSetModelLighting(model, &samplePosition, 0, WORLD_COORDINATE_PLAYER_LIGHT_COUNT);
        _worldCoordFindNearestRoomLight(&samplePosition, &D_80760618->nearestRoomLight);
        inputPoint                 = &projection->point;
        D_80760618->captureEnabled = WORLD_COORDINATE_LIGHT_CAPTURE_DISABLED;
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_SetTransMatrix(&GsWSMATRIX);
        // Project the sampled position to place the light-probe marker.
        projection->point.vx = samplePosition.vx;
        projection->point.vy = samplePosition.vy;
        projection->point.vz = samplePosition.vz;
        gte_RotTransPers(inputPoint, &projection->screen, &projection->depthCue, &projection->projectionFlags, &projection->orderingDepth);
        if (projection->projectionFlags >= 0) {
            probeMarker.x          = projection->screen.vx;
            probeMarker.y          = projection->screen.vy;
            probeMarker.otIndex    = WORLD_COORDINATE_LIGHT_PROBE_TEXT_OT_INDEX;
            probeMarker.colorRgb   = WORLD_COORDINATE_LIGHT_PROBE_TEXT_RGB;
            probeMarker.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            probeMarker.alignment  = TEXT_ALIGNMENT_CENTER;
            probeMarker.drawMode   = TEXT_DRAW_FILL_ONLY;
            textDrawString(&probeMarker, (const u8*)D_8009745C);
        }
        SCRATCH_STACK_RELEASE_BLOCK(WorldCoordProjectionScratch);
    } else {
        worldCoordSetModelLighting(model, &samplePosition, 0, WORLD_COORDINATE_PLAYER_LIGHT_COUNT);
        // A pending attachment update takes precedence over the periodic status tints.
        if (D_80114F28 != WORLD_COORDINATE_ATTACHMENT_LIGHT_PULSE_NONE) {
            colorMtx   = model->colorMtx;
            pulseGreen = rsin(gDisplayState.loopCount << WORLD_COORDINATE_ATTACHMENT_PULSE_ANGLE_SHIFT) + WORLD_COORDINATE_ATTACHMENT_PULSE_BASE;
            if ((gDisplayState.loopCount & 1) == 0) {
                pulseGreen >>= 1;
            }
            _worldCoordFillLightColorMatrix(colorMtx, WORLD_COORDINATE_ATTACHMENT_PULSE_LOW, pulseGreen, WORLD_COORDINATE_ATTACHMENT_PULSE_LOW);
            D_80114F28 = WORLD_COORDINATE_ATTACHMENT_LIGHT_PULSE_NONE;
        } else if ((gDisplayState.animFrame % WORLD_COORDINATE_STATUS_TINT_PERIOD_FRAMES) == 0 && playerStatus->hp > 0 && gGameSession->eventState == 0) {
            // Cyan for both wards/metabolism, blue for mind ward, yellow for body ward.
            if (Gp_StateC08.metabolismTicks > 0 || (Gp_StateC08.mindWard != 0 && Gp_StateC08.bodyWard != 0)) {
                _worldCoordFillLightColorMatrix(model->colorMtx, WORLD_COORDINATE_STATUS_TINT_LOW, WORLD_COORDINATE_STATUS_TINT_HIGH, WORLD_COORDINATE_STATUS_TINT_HIGH);
            } else if (Gp_StateC08.mindWard != 0) {
                _worldCoordFillLightColorMatrix(model->colorMtx, WORLD_COORDINATE_STATUS_TINT_LOW, WORLD_COORDINATE_STATUS_TINT_LOW, WORLD_COORDINATE_STATUS_TINT_HIGH);
            } else if (Gp_StateC08.bodyWard != 0) {
                _worldCoordFillLightColorMatrix(model->colorMtx, WORLD_COORDINATE_STATUS_TINT_HIGH, WORLD_COORDINATE_STATUS_TINT_HIGH, WORLD_COORDINATE_STATUS_TINT_LOW);
            }
            // Berserker red replaces any ward tint on these status-update frames.
            if (playerStatus->statusFlags & PLAYER_STATUS_BERSERKER) {
                _worldCoordFillLightColorMatrix(model->colorMtx, WORLD_COORDINATE_STATUS_TINT_HIGH, WORLD_COORDINATE_STATUS_TINT_LOW, WORLD_COORDINATE_STATUS_TINT_LOW);
            }
        }
    }

    // Child models share the player's persistent matrices, including any tint.
    playerActor = playerTask->work;
    {
        Task* childTask;

        for (childIndex = 0; childIndex < (s32)ARRAY_SIZE(playerActor->attachmentTasks); childIndex++) {
            childTask = playerActor->attachmentTasks[childIndex];
            if (childTask != NULL) {
                model           = childTask->extra.tmd;
                model->lightMtx = &Gp_DefaultMtx;
                model->colorMtx = &Gp_DefaultMtx2;
            }
        }
        for (childIndex = 0; childIndex < (s32)ARRAY_SIZE(playerActor->equipmentTasks); childIndex++) {
            childTask = playerActor->equipmentTasks[childIndex];
            if (childTask != NULL) {
                model           = childTask->extra.tmd;
                model->lightMtx = &Gp_DefaultMtx;
                model->colorMtx = &Gp_DefaultMtx2;
            }
        }
    }

    // The companion has a separate pair and receives ordinary room lighting.
    companionTask = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION];
    if (companionTask != NULL) {
        TmdObject* companionModel;

        companionModel  = companionTask->extra.tmd;
        companionActor  = companionTask->work;
        coord           = &companionModel->coords[1];
        model           = companionModel;
        model->colorMtx = &D_80114EF8;
        model->lightMtx = &D_80114ED8;
        actorRenderComposeCoord(coord);
        samplePosition.vx = coord->workm.t[0];
        samplePosition.vy = coord->workm.t[1] - WORLD_COORDINATE_PLAYER_LIGHT_SAMPLE_Y_OFFSET;
        samplePosition.vz = coord->workm.t[2];
        worldCoordSetModelLighting(model, &samplePosition, 0, WORLD_COORDINATE_PLAYER_LIGHT_COUNT);
        {
            Task* childTask;

            for (childIndex = 0; childIndex < (s32)ARRAY_SIZE(companionActor->attachmentTasks); childIndex++) {
                childTask = companionActor->attachmentTasks[childIndex];
                if (childTask != NULL) {
                    model           = childTask->extra.tmd;
                    model->lightMtx = &D_80114ED8;
                    model->colorMtx = &D_80114EF8;
                }
            }
            for (childIndex = 0; childIndex < (s32)ARRAY_SIZE(companionActor->equipmentTasks); childIndex++) {
                childTask = companionActor->equipmentTasks[childIndex];
                if (childTask != NULL) {
                    model           = childTask->extra.tmd;
                    model->lightMtx = &D_80114ED8;
                    model->colorMtx = &D_80114EF8;
                }
            }
        }
    }
}

/// Replaces light-colour coefficients with a green hit pulse and consumes its request.
///
/// Every column gets red/blue at ONE/8 and green at a Q12 sine plus 1.5*ONE,
/// halved on even display-loop iterations. The sine phase advances by 64 angle
/// units per iteration (4096 per turn). Preserves the ambient translation and
/// all packed colour-mode bits except ENEMY_COLOR_HIT_FLASH. Both arguments
/// must be writable; the caller establishes that the flash is eligible.
static inline void _worldCoordApplyActorHitFlash(Enemy* enemy, MATRIX* colorMtx)
{
    enum {
        WORLD_COORDINATE_HIT_FLASH_PHASE_SHIFT = 6,
        WORLD_COORDINATE_HIT_FLASH_GREEN_BASE  = ONE + ONE / 2,
        WORLD_COORDINATE_HIT_FLASH_RED_BLUE    = ONE / 8
    };
    s32 flashGreen;

    flashGreen = rsin(gDisplayState.loopCount << WORLD_COORDINATE_HIT_FLASH_PHASE_SHIFT) + WORLD_COORDINATE_HIT_FLASH_GREEN_BASE;
    if ((gDisplayState.loopCount & 1) == 0) {
        flashGreen >>= 1;
    }
    _worldCoordFillLightColorMatrix(colorMtx, WORLD_COORDINATE_HIT_FLASH_RED_BLUE, flashGreen, WORLD_COORDINATE_HIT_FLASH_RED_BLUE);
    enemy->colorMode &= ENEMY_COLOR_HIT_FLASH_CLEAR;
}

/// Zeros the directional light colours for an actor's black colour mode.
///
/// `colorRows` supplies a writable 3x3 of signed Q12 coefficients, with RGB
/// rows and one column per light. Only those 18 bytes are written; the enclosing
/// matrix's alignment bytes and ambient RGB translation are preserved.
static inline void _worldCoordClearActorLightColor(s16 colorRows[3][3])
{
    colorRows[0][0] = 0;
    colorRows[0][1] = 0;
    colorRows[0][2] = 0;
    colorRows[1][0] = 0;
    colorRows[1][1] = 0;
    colorRows[1][2] = 0;
    colorRows[2][0] = 0;
    colorRows[2][1] = 0;
    colorRows[2][2] = 0;
}

/// Applies one actor colour mode to a writable light-colour 3x3.
///
/// Coefficients are signed Q12; the ambient translation is preserved. Weighted
/// mode maps each column's (7r+6g+3b)/33 to (4,2,1) times that value. Black
/// clears the coefficients, and tint fills every column with (0x180,0x100,0x100).
/// Default, including unrecognized modes, retains lighting unless damage over
/// time maps the weighted value to (3,1,3). Stores narrow to signed halfwords.
///
/// Except in weighted mode, a pending hit flash with spawnState 0 overrides
/// the mode and clears its request. This side effect precedes a later remap
/// of the previous mode during blending; enemy and colorMtx must be live.
static void _worldCoordRemapActorColor(Enemy* enemy, MATRIX* colorMtx, s32 colorMode)
{
    enum {
        WORLD_COORDINATE_HIT_FLASH_SPAWN_STATE = 0,
        WORLD_COORDINATE_ACTOR_TINT_RED        = 0x180,
        WORLD_COORDINATE_ACTOR_TINT_GREEN_BLUE = 0x100
    };
    s32 lightIndex;

    /// Recolours all columns from signed Q12 (7r+6g+3b)/33 and integer RGB scales.
    ///
    /// colorMatrix is a writable MATRIX*. Arguments must be side-effect-free;
    /// captures this function's s32 lightIndex. Ambient translation is preserved;
    /// division truncates toward zero and stores narrow to signed halfwords.
#define WORLD_COORDINATE_APPLY_WEIGHTED_ACTOR_TINT(colorMatrix, redScale, greenScale, blueScale)                                                                      \
    {                                                                                                                                                                 \
        s32 weightedIntensity;                                                                                                                                        \
        for (lightIndex = 0; lightIndex < (s32)ARRAY_SIZE((colorMatrix)->m[0]); lightIndex++) {                                                                       \
            weightedIntensity               = ((colorMatrix)->m[0][lightIndex] * 7 + (colorMatrix)->m[1][lightIndex] * 6 + (colorMatrix)->m[2][lightIndex] * 3) / 33; \
            (colorMatrix)->m[0][lightIndex] = weightedIntensity * (redScale);                                                                                         \
            (colorMatrix)->m[1][lightIndex] = weightedIntensity * (greenScale);                                                                                       \
            (colorMatrix)->m[2][lightIndex] = weightedIntensity * (blueScale);                                                                                        \
        }                                                                                                                                                             \
    }

    switch (colorMode) {
        case ENEMY_COLOR_WEIGHTED:
            WORLD_COORDINATE_APPLY_WEIGHTED_ACTOR_TINT(colorMtx, 4, 2, 1);
            break;

        case ENEMY_COLOR_TINT:
            if ((enemy->colorMode & ENEMY_COLOR_HIT_FLASH) && (enemy->spawnState == WORLD_COORDINATE_HIT_FLASH_SPAWN_STATE)) {
                _worldCoordApplyActorHitFlash(enemy, colorMtx);
                break;
            }
            _worldCoordFillLightColorMatrix(colorMtx, WORLD_COORDINATE_ACTOR_TINT_RED, WORLD_COORDINATE_ACTOR_TINT_GREEN_BLUE, WORLD_COORDINATE_ACTOR_TINT_GREEN_BLUE);
            break;

        case ENEMY_COLOR_BLACK:
            if ((enemy->colorMode & ENEMY_COLOR_HIT_FLASH) && (enemy->spawnState == WORLD_COORDINATE_HIT_FLASH_SPAWN_STATE)) {
                _worldCoordApplyActorHitFlash(enemy, colorMtx);
                break;
            }
            _worldCoordClearActorLightColor(colorMtx->m);
            break;

        case ENEMY_COLOR_DEFAULT:
        default:
            if ((enemy->colorMode & ENEMY_COLOR_HIT_FLASH) && (enemy->spawnState == WORLD_COORDINATE_HIT_FLASH_SPAWN_STATE)) {
                _worldCoordApplyActorHitFlash(enemy, colorMtx);
            } else if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
                WORLD_COORDINATE_APPLY_WEIGHTED_ACTOR_TINT(colorMtx, 3, 1, 3);
            }
            break;
    }
#undef WORLD_COORDINATE_APPLY_WEIGHTED_ACTOR_TINT
}

/// Saves sampled directional light colours for an actor colour-mode blend.
///
/// `destination` and `source` supply writable and readable 3x3 coefficient
/// arrays, respectively, in disjoint storage. Rows are RGB and columns are
/// lights; signed Q12 values are copied unchanged. Exactly 18 bytes are copied,
/// leaving the enclosing matrices' alignment bytes and ambient RGB untouched.
/// The caller owns both arrays; neither pointer is retained.
static inline void _worldCoordCopyActorLightColor(s16 destination[3][3], const s16 source[3][3])
{
    destination[0][0] = source[0][0];
    destination[0][1] = source[0][1];
    destination[0][2] = source[0][2];
    destination[1][0] = source[1][0];
    destination[1][1] = source[1][1];
    destination[1][2] = source[1][2];
    destination[2][0] = source[2][0];
    destination[2][1] = source[2][1];
    destination[2][2] = source[2][2];
}

void worldCoordUpdateActorColor(Enemy* enemy, const void* worldPosition, s32 unusedArg2, s32 unusedArg3)
{
    enum {
        WORLD_COORDINATE_ACTOR_LIGHTING_PAUSED    = 1,
        WORLD_COORDINATE_ACTOR_BLEND_WEIGHT_SHIFT = 8
    };
    TmdObject*                   model;
    MATRIX*                      colorMtx;
    s32                          currentMode;
    WorldCoordActorColorScratch* blendScratch;
    s32                          lightIndex;
    s32                          previousWeight;
    s32                          currentWeight;

    model       = enemy->task->extra.tmd;
    colorMtx    = model->colorMtx;
    currentMode = enemy->colorMode & ENEMY_COLOR_MODE_MASK;
    if ((!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && (model->buffer != NULL)) || (gGameSession->sceneUpdatesPaused != WORLD_COORDINATE_ACTOR_LIGHTING_PAUSED)) {
        blendScratch = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordActorColorScratch);
        // Sample all three lights before applying either colour mode.
        worldCoordSetModelLighting(model, worldPosition, 0, ARRAY_SIZE(colorMtx->m[0]));
        if ((s8)enemy->colorBlend <= 0) {
            _worldCoordRemapActorColor(enemy, colorMtx, currentMode);
        } else {
            // Keep the same sampled coefficients; ambient is never copied or blended.
            _worldCoordCopyActorLightColor(blendScratch->previousColor.m, colorMtx->m);
            // The current remap may consume a hit flash before the previous remap.
            _worldCoordRemapActorColor(enemy, colorMtx, currentMode);
            _worldCoordRemapActorColor(enemy, &blendScratch->previousColor, (enemy->colorMode >> ENEMY_COLOR_PREVIOUS_SHIFT) & ENEMY_COLOR_MODE_MASK);
            previousWeight = (s8)enemy->colorBlend << WORLD_COORDINATE_ACTOR_BLEND_WEIGHT_SHIFT;
            currentWeight  = ONE - previousWeight;
            for (lightIndex = 0; lightIndex < (s32)ARRAY_SIZE(colorMtx->m[0]); lightIndex++) {
                blendScratch->currentColumn.vx  = colorMtx->m[0][lightIndex];
                blendScratch->currentColumn.vy  = colorMtx->m[1][lightIndex];
                blendScratch->currentColumn.vz  = colorMtx->m[2][lightIndex];
                blendScratch->previousColumn.vx = blendScratch->previousColor.m[0][lightIndex];
                blendScratch->previousColumn.vy = blendScratch->previousColor.m[1][lightIndex];
                blendScratch->previousColumn.vz = blendScratch->previousColor.m[2][lightIndex];
                gte_LoadAverageShort12(&blendScratch->currentColumn, &blendScratch->previousColumn, currentWeight, previousWeight, &blendScratch->currentColumn);
                colorMtx->m[0][lightIndex] = blendScratch->currentColumn.vx;
                colorMtx->m[1][lightIndex] = blendScratch->currentColumn.vy;
                colorMtx->m[2][lightIndex] = blendScratch->currentColumn.vz;
            }
            if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
                enemy->colorBlend--;
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

void worldCoordSetActorColorMode(Enemy* enemy, s32 colorMode)
{
    u8 packedModes;

    packedModes = enemy->colorMode;
    colorMode  &= ENEMY_COLOR_MODE_MASK;
    if ((packedModes & ENEMY_COLOR_MODE_MASK) != colorMode) {
        enemy->colorMode  = (packedModes & ENEMY_COLOR_KEPT_BITS) | ((packedModes & ENEMY_COLOR_MODE_MASK) << ENEMY_COLOR_PREVIOUS_SHIFT) | colorMode;
        enemy->colorBlend = ENEMY_COLOR_BLEND_STEPS;
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

/// Counts transient point-light slots with a nonzero expiry countdown.
///
/// Returns 0..WORLD_COORDINATE_TRANSIENT_LIGHT_COUNT. Does not age, clear,
/// compose or reserve slots; even a negative countdown is counted as active.
/// Retained out-of-line entry with no current callers.
static s32 _worldCoordCountActiveTransientPointLights(void)
{
    s32 activeCount;
    s32 slotIndex;

    activeCount = 0;
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(gWorldCoordTransientPointLights); slotIndex++) {
        if (gWorldCoordTransientPointLights[slotIndex].framesLeft != WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE) {
            activeCount++;
        }
    }
    return activeCount;
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

void worldCoordPlayerLightingTask(Task* task)
{
    TaskFunc stateHandlers[WORLD_COORDINATE_PLAYER_LIGHTING_STATE_COUNT] = { _worldCoordInitPlayerLighting, _worldCoordUpdatePlayerLighting };

    stateHandlers[task->state](task);
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

    if (rankedLights[slotIndex].rank < contributionScore) {
        if (slotIndex < WORLD_COORDINATE_RANKED_LIGHT_COUNT - 1) {
            rankedLights[slotIndex + 1] = rankedLights[slotIndex];
        }
        if (slotIndex > 0) {
            _worldCoordInsertRankedLight(rankedLights, contributionScore, sourceKind, light, slotIndex - 1);
        } else {
            rankedLights->rank  = contributionScore;
            rankedLights->kind  = sourceKind;
            rankedLights->light = light;
        }
    } else if (slotIndex < WORLD_COORDINATE_RANKED_LIGHT_COUNT - 1) {
        slot            = &rankedLights[slotIndex];
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

/// Binds the player's persistent light matrices and starts per-frame lighting.
///
/// Requires a live player model/work and both attachment-anchor tasks; the
/// player slot is dereferenced before its NULL test, preserving the original
/// caller requirement. Missing room lights kill the lighting task. Otherwise
/// spawnArg2 borrows the room descriptor, both overrides and the pending
/// attachment pulse are disabled, and state advances from 0 to 1 before the
/// first update. Matrix storage belongs to the gameplay overlay.
static void _worldCoordInitPlayerLighting(Task* task)
{
    enum { WORLD_COORDINATE_LIGHT_OVERRIDE_DISABLED = 0 };

    Task*                 playerTask;
    TmdObject*            model;
    GameActor*            playerActor;
    WorldCoordRoomLights* roomLights;
    s32                   attachmentIndex;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    model      = playerTask->extra.tmd;
    if (playerTask != NULL) {
        roomLights      = _worldCoordGetRoomLights(&gGameSession->location.loc);
        attachmentIndex = 0;
        if (roomLights == NULL) {
            taskKill(task);
            return;
        }
        task->spawnArg2.pointer = roomLights;
        model->lightMtx         = &Gp_DefaultMtx;
        model->colorMtx         = &Gp_DefaultMtx2;
        playerActor             = playerTask->work;
        Gp_OverrideVecFlag      = WORLD_COORDINATE_LIGHT_OVERRIDE_DISABLED;
        Gp_OverrideVec2Flag     = WORLD_COORDINATE_LIGHT_OVERRIDE_DISABLED;
        D_80114F28              = WORLD_COORDINATE_ATTACHMENT_LIGHT_PULSE_NONE;
        do {
            model           = playerActor->attachmentTasks[attachmentIndex]->extra.tmd;
            model->lightMtx = &Gp_DefaultMtx;
            model->colorMtx = &Gp_DefaultMtx2;
            attachmentIndex++;
        } while (attachmentIndex < (s32)ARRAY_SIZE(playerActor->attachmentTasks));
        task->state++;
        _worldCoordUpdatePlayerLighting(task);
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

/// Returns an attack's reaction/special attribute at the tables' unsigned halfword width.
///
/// Bit 15 selects attachment-ability rows instead of weapon-attack rows; the
/// low seven bits select a row and all other bits are ignored. The row must be
/// below 47 for weapon attacks or ATTACHMENT_LEVEL_ROW_COUNT for attachments.
/// Returns the same reaction field as `damageGetPlayerAttackReaction`.
/// This translation unit has no caller; the helper emits no out-of-line body.
static inline u16 _objectFieldGetAttackHitReaction(s32 attackId)
{
    enum {
        OBJECT_FIELD_ATTACK_ATTACHMENT_FLAG = 0x8000,
        OBJECT_FIELD_ATTACK_ROW_MASK        = 0x7F
    };
    if ((attackId & OBJECT_FIELD_ATTACK_ATTACHMENT_FLAG) == 0) {
        return Gp_IdParamLo[attackId & OBJECT_FIELD_ATTACK_ROW_MASK].hitReaction;
    }
    return Gp_IdParamHi.rows[attackId & OBJECT_FIELD_ATTACK_ROW_MASK].column.outcome.hitReaction;
}
