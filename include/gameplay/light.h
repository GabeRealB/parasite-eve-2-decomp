#ifndef GAMEPLAY_LIGHT_H
#define GAMEPLAY_LIGHT_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// A light source: a coordinate that places it, and the colour it casts.
///
/// Every light a model is lit by is one of these or begins with one - the
/// room's directional lights are exactly this, its point and spot lights and
/// gameplay's transient lights extend it - so the code that ranks the lights
/// around a model and loads the winners into its light matrices takes any of
/// them through this type.
///
/// The coordinate is parented to the view, but composed with that ancestor
/// excluded, so `workm` places the light in world space. Its parent link is
/// used normally. View membership and attenuation share the bytes ordinary
/// nodes use for `param`; `at` exposes that lighting interpretation.
typedef struct GpLight {
    union {
        GfxCoord coord;             // the light's placement, parented to the view
        struct {
            u32       composeStamp; // The coordinate's composition cache stamp
            MATRIX    local;        // `coord.coord`: `t` is the light's position under its parent
            MATRIX    world;        // `coord.workm`: `t` is the light's world position
            s16       room;         // view the light belongs to; 0 lights every view
            byte      pad_46[4];
            s16       scale;        // attenuation last computed for the point being lit, 1.0 = 0x1000
            GfxCoord* parent;       // `coord.parent`, the coordinate the light hangs from
        } at;                       // the same words as the lighting code reads them
    } u;
    s16  r;                         // colour, fed to the colour matrix scaled by `u.at.scale`
    s16  g;
    s16  b;
    byte pad_56[2];
} GpLight;
STATIC_ASSERT_SIZEOF(GpLight, 0x58);

/// A light that fades with distance: full strength within `inner` of its
/// world position, falling to nothing at `outer`.
typedef struct GpPointLight {
    GpLight head;
    s32     inner; // radius the light is at full strength within
    s32     outer; // radius beyond which it casts nothing
} GpPointLight;
STATIC_ASSERT_SIZEOF(GpPointLight, 0x60);

/// A point light narrowed to a cone: `dir` is the axis the room data aims it
/// along, from which gameplay builds `head.u.at.local` so its Z column is that
/// axis, and `angle` is the cone's full opening.
typedef struct GpSpotLight {
    GpLight head;
    SVECTOR dir;   // the cone's axis, as the room data gives it
    s32     inner; // radius the light is at full strength within
    s32     outer; // radius beyond which it casts nothing
    s32     angle; // full opening angle of the cone (0x1000 a full turn)
} GpSpotLight;
STATIC_ASSERT_SIZEOF(GpSpotLight, 0x6C);

/// One of the eight transient point lights gameplay keeps on top of a room's
/// own lights, which effects, weapons, parasite energies, actors and rooms
/// switch on for as long as something of theirs glows.
///
/// A slot is lit while `framesLeft` is non-zero. The per-frame gameplay tick
/// counts it down outside events, so an owner keeps its light on by re-arming
/// the count every frame it draws. While a slot is lit, gameplay re-evaluates
/// `light.head.u.coord` against the view each frame and ranks the slot with the room's
/// point lights whenever a model is lit, reading it as `light`. Nothing
/// allocates the slots: each kind of owner writes indices of its own.
typedef struct _GpCoord64 {
    s32          framesLeft; // frames the light stays lit; 0 leaves the slot dark
    GpPointLight light;      // Placement, colour and falloff radii of the transient light
} GpCoord64;
STATIC_ASSERT_SIZEOF(GpCoord64, 0x64);

#endif // GAMEPLAY_LIGHT_H
