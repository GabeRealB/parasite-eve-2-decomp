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

/// A world-coordinate point light: full strength near its position, then fading out.
///
/// The record is the 0x60-byte room-light extension of `WorldCoordLight`.
/// That header is the first member, so placement, the view filter,
/// attenuation and colour are the shared record, and light selection reads
/// the point light through it. Room tables store the entries contiguously.
///
/// `inner` and `outer` are distances in the same integer world units as the
/// light's composed translation. The light is at full strength within
/// `inner` of its position and casts nothing once the distance reaches
/// `outer`. Between those distances the contribution scale falls in
/// proportion to squared distance, from full strength to none. Equal radii
/// are full strength out to that distance. Room tables, and the effects,
/// weapons and actors that fill a transient slot, keep `inner` less than or
/// equal to `outer`.
///
/// A selected point light shades along the direction toward the light.
typedef struct {
    WorldCoordLight head;  // Shared placement, view filter, attenuation and colour
    s32             inner; // Full-strength distance from the position, in world units
    s32             outer; // Distance at which the light casts nothing, in world units
} WorldCoordPointLight;
STATIC_ASSERT_SIZEOF(WorldCoordPointLight, 0x60);

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

/// Countdown value that disables a transient point-light slot.
enum { WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE = 0 };

/// An expiring point-light contribution alongside a room's authored lights.
///
/// Effects, weapons, actors and rooms write directly into selected entries of
/// the eight-slot `Gp_RoomCoords` pool. Slots are shared storage, with no
/// allocation or reference count; another writer can replace a contribution.
/// Set `framesLeft` to a positive frame count to enable the slot, or zero to
/// disable it. The shared effect update decrements nonzero counts once per
/// frame unless effect control is paused. An owner may also decrement or clear
/// its count, or refresh it to keep a light alive. Expiration retains the light
/// record and its transform; it releases no resource.
///
/// Initialization disables the slot and borrows the persistent view coordinate
/// as its transform parent. Placement is local to that parent, and writers
/// must mark `light.head.transform.coord.composeStamp` dirty after moving it.
/// Active transforms are composed with the view ancestor excluded. Lighting
/// ranks `light` as a point source using its position, RGB and distance radii;
/// transient queries ignore `light.head.transform.lighting.viewId`.
typedef struct {
    s32                  framesLeft; // Expiry countdown in gameplay frames (0 inactive); owners may shorten or refresh it
    WorldCoordPointLight light;      // Retained placement, RGB intensity and distance falloff; transform parent is borrowed
} WorldCoordTransientPointLight;
STATIC_ASSERT_SIZEOF(WorldCoordTransientPointLight, 0x64);

#endif // GAMEPLAY_LIGHT_H
