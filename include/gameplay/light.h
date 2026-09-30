#ifndef GAMEPLAY_LIGHT_H
#define GAMEPLAY_LIGHT_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// View filter accepting every current session view.
///
/// A world-coordinate light stores this in its view filter. The filter is
/// compared by equality with the current location view: any other value is one
/// 1-based room view and contributes only while that view is current. This
/// zero value contributes in every view. Session views are numbered from 1,
/// so the sentinel lies outside the view slots. A filter left at this value
/// therefore admits the light in every view.
enum { WORLD_COORDINATE_LIGHT_ALL_VIEWS = 0 };

/// A world-coordinate light source with a transform, view filter and RGB intensity.
///
/// Room directional lights use this record directly; point, cone and transient
/// lights embed it. Lighting ranks their contributions at a model's position
/// and writes the selected directions and attenuated colours into its matrices.
///
/// `transform.coord` is used for coordinate composition; `transform.lighting`
/// gives the owner-managed `GfxCoord.param` bytes their lighting interpretation.
/// Room lights are parented to the view and composed with that ancestor excluded.
/// A directional light's composed translation supplies its direction; a point
/// or cone light's supplies its position. Parent coordinates are borrowed and
/// must remain live while the light is composed. Attenuation is query scratch,
/// overwritten when lighting a new position and adjusted during light selection.
typedef struct {
    union {
        GfxCoord coord;              // Transform node passed to coordinate composition
        struct {
            u32       composeStamp;  // Composition cache stamp; see `GfxCoord.composeStamp`
            MATRIX    local;         // Local-to-parent matrix; translation is position or directional-light vector
            MATRIX    composed;      // Cached placement; origin-based falloff uses t[0] for a contribution rank
            s16       viewId;        // View filter (0 every view, otherwise a matching GameLocationKey.view)
            byte      unknown_46[4]; // Role unproven; room initializers supply zero
            s16       attenuation;   // Current contribution scale, 12 fractional bits (0 dark, ONE full strength)
            GfxCoord* parent;        // Borrowed parent coordinate
        } lighting;                  // Transform with the light's view filter and contribution scale
    } transform;
    struct {
        s16 r;          // Red intensity, 12 fractional bits (ONE is 1.0)
        s16 g;          // Green intensity, 12 fractional bits
        s16 b;          // Blue intensity, 12 fractional bits
    } color;            // RGB intensity loaded into the GTE and scaled by attenuation
    byte unknown_56[2]; // Role unproven; room initializers supply zero
} WorldCoordLight;
STATIC_ASSERT_SIZEOF(WorldCoordLight, 0x58);

/// A light that fades with distance: full strength within `inner` of its
/// world position, falling to nothing at `outer`.
typedef struct GpPointLight {
    WorldCoordLight head;
    s32             inner; // radius the light is at full strength within
    s32             outer; // radius beyond which it casts nothing
} GpPointLight;
STATIC_ASSERT_SIZEOF(GpPointLight, 0x60);

/// A world-coordinate light limited to a cone around an axis.
///
/// The record is the 0x6C-byte room-light extension of `WorldCoordLight`.
/// That header is the first member, so placement, the view filter,
/// attenuation and colour are the shared record, and ranking reads the cone
/// light through it. Distance falloff matches a point light: full strength
/// within `inner` of the position, none beyond `outer`, and full strength
/// out to that radius when the two are equal. A sample must also lie inside
/// the cone.
///
/// `axis` is the aim in the same frame as the light's position, with length
/// about `ONE`. The first coordinate update replaces the local rotation so
/// its Z column is this axis and leaves the translation in place. The cone
/// test reads that column from the composed matrix. `angle` is the full
/// opening, 0x1000 units per turn; the test passes half of it to `rcos`,
/// which reduces a non-negative argument to one turn. A selected cone light
/// shades along the direction toward the light, as a point light does.
typedef struct {
    WorldCoordLight head;  // Shared placement, view filter, attenuation and colour
    SVECTOR         axis;  // Cone axis in the position's frame, length about ONE. Room data leaves the fourth component zero.
    s32             inner; // Distance of full strength from the light's position
    s32             outer; // Distance beyond which the light casts nothing
    s32             angle; // Full cone opening, 0x1000 units per turn
} WorldCoordSpotLight;
STATIC_ASSERT_SIZEOF(WorldCoordSpotLight, 0x6C);

/// One of the eight transient point lights gameplay keeps on top of a room's
/// own lights, which effects, weapons, parasite energies, actors and rooms
/// switch on for as long as something of theirs glows.
///
/// A slot is lit while `framesLeft` is non-zero. The per-frame gameplay tick
/// counts it down outside events, so an owner keeps its light on by re-arming
/// the count every frame it draws. While a slot is lit, gameplay re-evaluates
/// `light.head.transform.coord` against the view each frame and ranks the slot with the room's
/// point lights whenever a model is lit, reading it as `light`. Nothing
/// allocates the slots: each kind of owner writes indices of its own.
typedef struct _GpCoord64 {
    s32          framesLeft; // frames the light stays lit; 0 leaves the slot dark
    GpPointLight light;      // Placement, colour and falloff radii of the transient light
} GpCoord64;
STATIC_ASSERT_SIZEOF(GpCoord64, 0x64);

#endif // GAMEPLAY_LIGHT_H
