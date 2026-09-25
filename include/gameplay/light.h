#ifndef GAMEPLAY_LIGHT_H
#define GAMEPLAY_LIGHT_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// A light source: a coordinate that places it, and the colour it casts.
///
/// Every light a model is lit by is one of these or begins with one - the
/// room's directional lights are exactly this, its point and spot lights and
/// gameplay's transient lights extend it - so the code that ranks the lights
/// around a model and loads the winners into its light matrices takes any of
/// them through this type.
///
/// The coordinate is an ordinary `GsCOORDINATE2`, parented to the view and
/// updated with it, whose `coord.t` is where the light sits and whose `workm` is
/// where that is in the world. A light never uses the coordinate's `param` or
/// its link upwards, so it keeps two values of its own in those words, which
/// `at` names.
typedef struct GpLight {
    union {
        GsCOORDINATE2 coord; // the light's placement, parented to the view
        struct {
            u32            flg;
            MATRIX         local;  // `coord.coord`: `t` is the light's position under its parent
            MATRIX         world;  // `coord.workm`: `t` is the light's world position
            s16            room;   // view the light belongs to; 0 lights every view
            byte           pad_46[4];
            s16            scale;  // attenuation last computed for the point being lit, 1.0 = 0x1000
            GsCOORDINATE2* parent; // `coord.sub`, the coordinate the light hangs from
        } at;                      // the same words as the lighting code reads them
    } u;
    s16  r;                        // colour, fed to the colour matrix scaled by `u.at.scale`
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
/// `data.coord` against the view each frame and ranks the slot with the room's
/// point lights whenever a model is lit, reading it as `data.light`. Nothing
/// allocates the slots: each kind of owner writes indices of its own.
typedef struct _GpCoord64 {
    s32 framesLeft;          // frames the light stays lit; 0 leaves the slot dark
    union {
        GsCOORDINATE2 coord; // where the light is, parented to the view
        GpPointLight  light; // the same object read as a light: colour and falloff radii
    } data;
} GpCoord64;
STATIC_ASSERT_SIZEOF(GpCoord64, 0x64);

extern GpCoord64 Gp_RoomCoords[8];

#endif // GAMEPLAY_LIGHT_H
