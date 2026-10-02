#ifndef GAMEPLAY_EFFECTS_H
#define GAMEPLAY_EFFECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

struct GfxCoord;
struct Task;

/// The scratch-pad block of a shape drawn about one projected point: `vec` is
/// the point, and one RTPS fills `sx`, `sy`, `flag` and `otz`. `step` is a
/// length scaled by the depth, the radius a ring is swept at or the half size
/// of a billboard sprite.
typedef struct _GpRingScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     step;
    /* 0x14 */ s16     sx;
    /* 0x16 */ s16     sy;
} GpRingScratch;
STATIC_ASSERT_SIZEOF(GpRingScratch, 0x18);

/// Scratch-stack workspace for a screen-space effect centred on one world point.
///
/// A drawer stages the signed 16-bit world position in `worldPoint`, or projects
/// a caller's vector directly, then constructs sprites, rings or radial spikes
/// around the projected centre. `extent` holds the interpretation needed by
/// that construction; every component is a signed integer pixel distance.
/// Spinning sprites reuse the corner offsets for each pair of opposite corners.
///
/// `depth` starts as SZ3 divided by four and may be biased or clamped before
/// sizing and ordering the primitives. `screenX` and `screenY` retain the raw
/// 16-bit encodings of the signed GTE coordinates for GPU packet arithmetic;
/// one GTE word store fills both, starting at `screenX`.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order after drawing. Fields are initialized as needed; pointers into the
/// block must not survive its release.
typedef struct {
    SVECTOR worldPoint;      // Optional staging point in world-coordinate units, narrowed to s16
    s32     depth;           // Projection depth used for screen sizing and ordering-table placement
    s32     projectionFlags; // GTE FLAG word; bit 31 makes it negative and rejects the projection
    union {
        struct {
            s32 x; // Horizontal corner displacement or half-width, in pixels
            s32 y; // Vertical corner displacement or half-height, in pixels
        } corner;
        struct {
            s32 inner; // Inner ring radius, in pixels
            s32 outer; // Outer ring radius, in pixels
        } ring;
        struct {
            s32 outer; // Outer burst radius, in pixels
            s32 inner; // Inner burst radius, in pixels
        } burst;
        struct {
            s32 radius;    // Distance from the centre to a spike's base, in pixels
            s32 halfWidth; // Transverse half-width of a spike, in pixels
        } spike;
    } extent;              // Sizing workspace reused for the current screen-space construction
    u16 screenX;           // Raw projected centre X; adjacent to screenY for the GTE word store
    u16 screenY;           // Raw projected centre Y
} EffectShapeScratch;
STATIC_ASSERT_SIZEOF(EffectShapeScratch, 0x1C);

/// 0x118-byte scratch from the scratch stack used by `Gp_DrawBandEx`. Holds
/// the two 16-vertex rings of a shaded band: `inner[i]` is the ring of
/// radius `arg1` and `outer[i]` the ring of radius `arg1 + arg2`, both built
/// in the XZ plane by `rsin` / `rcos`, rotated by the coordinate's `workm`
/// and offset by its `workm.t[]`. The second pass projects each segment:
/// `sxy0` is the `gte_stsxy` of `inner[i]` and `sxy1` / `sxy2` / `sxy3` the
/// `gte_stsxy3` of `inner[i + 1]` / `outer[i]` / `outer[i + 1]`, with `otz`
/// from `gte_stszotz` (then incremented) and `flag` from `gte_stflg`.
typedef struct _GpBandScratch {
    /* 0x000 */ SVECTOR inner[16];
    /* 0x080 */ SVECTOR outer[16];
    /* 0x100 */ s32     otz;
    /* 0x104 */ s32     flag;
    /* 0x108 */ DVECTOR sxy0;
    /* 0x10C */ DVECTOR sxy1;
    /* 0x110 */ DVECTOR sxy2;
    /* 0x114 */ DVECTOR sxy3;
} GpBandScratch;
STATIC_ASSERT_SIZEOF(GpBandScratch, 0x118);

/// Argument record for `func_800FDB18`, the id-dispatched effect spawner.
///
/// `coord` is the coordinate the effects hang off, and it is borrowed: the
/// record does not own it. While it is NULL the spawner copies in the
/// coordinate the call passed, or the view coordinate when the call passed
/// none, and a later call that passes no coordinate reuses it. Some effect
/// ids place the effect on this stored coordinate even when the call also
/// passes one.
///
/// `spawnArgLo` and `spawnArgHi` are the signed halves of one spawn-argument
/// word, the low half in bits 0..15. Effect ids that hand the word on pack it
/// into the spawned effect's `Task::spawnArg1`, and that effect decides what
/// the halves mean. Ids that spawn a series use `spawnArgHi` as the repeat
/// count, one of them triples it, and do not pass `spawnArgLo`. Some ids
/// pass only `spawnArgLo` and force the high half to 1. One id reads neither
/// half. Callers store non-negative magnitudes in the low half and small
/// positive counts in the high half.
///
/// A caller keeps the record as a global or as a member of the work block
/// beside the spawner. One effect task also allocates its own into `Task::work`.
typedef struct {
    GfxCoord* coord;      // borrowed coordinate the effects hang off; NULL until a call fills it
    s16       spawnArgLo; // signed low half of the spawn-argument word; series spawns leave it unused
    s16       spawnArgHi; // signed high half of that word, or the repeat count of a series spawn
} EffectSpawnArg;
STATIC_ASSERT_SIZEOF(EffectSpawnArg, 0x8);

/// Work block of one spawned effect.
///
/// The effect spawner allocates one, `sizeof(EffectWork)` bytes, and stores it
/// in that task's `Task::spawnArg2`. The task frees the block when it exits.
/// A caller may also keep the returned pointer for the effect's lifetime.
///
/// `task` is the effect's own task. `parent` is the coordinate the spawn was
/// given, or the view coordinate when that argument was NULL. The effect's own
/// coordinate is parented to the view either way; a task copies `parent` onto
/// a coordinate only when it hangs the effect there, and some tasks read
/// `parent`'s matrix instead. `age` counts frames from zero. `pos` and `move`
/// are vectors. For most effects they are a position and a displacement; some
/// store a rotation triple in `pos` and a direction in `move`. Spawn copies
/// the caller's offset into `pos`, reading a NULL offset as zero, and zeroes
/// `move`.
///
/// The four trailing halfwords are signed parameters. Each effect stores its
/// own magnitudes there, and a reader that wants one unsigned converts it at
/// the use. The comments name the reading most tasks give them.
typedef struct {
    struct Task*     task;    // the effect task; this block is its `spawnArg2`
    s32              field_4; // zeroed at spawn; no decompiled reader; role unproven
    struct GfxCoord* parent;  // coordinate the spawn was given, or the view when that argument was NULL
    SVECTOR*         field_C; // offset pointer the spawn was given, NULL left as NULL; no decompiled reader; role unproven
    SVECTOR          move;    // displacement or direction; zero at spawn
    SVECTOR          pos;     // position or offset copied from the spawn argument; some effects store a rotation triple
    s16              index;   // selector: a frame, variant or sprite column
    s16              age;     // frames since spawn; tasks count it up from zero
    s16              scale;   // size, brightness or fixed-point scale, as the effect uses it
    s16              angle;   // spin, or another angular magnitude; some effects store a radius or a limit
    s16              period;  // frames the current phase lasts, or the value held for that phase
    s16              step;    // per-frame increment, or a packed draw parameter
} EffectWork;
STATIC_ASSERT_SIZEOF(EffectWork, 0x2C);

/// Texture origin and palette location for one frame of an effect sprite.
///
/// UV coordinates are texels within the texture page selected by the drawer.
/// Palette coordinates are unencoded VRAM coordinates for `getClut`.
/// The drawer supplies cell dimensions and the texture page; UV-only users
/// select their palette separately. The intervening bytes have no proven role.
typedef struct {
    u8  u;       // Left texture column in texels (0..255).
    u8  field_1; // Unread byte; role unproven.
    u8  v;       // Top texture row in texels (0..255).
    u8  field_3; // Unread byte; role unproven.
    u16 clutX;   // Palette X in VRAM words, aligned to 16 words.
    u16 clutY;   // Palette Y in VRAM scanlines.
} EffectSpriteTextureFrame;
STATIC_ASSERT_SIZEOF(EffectSpriteTextureFrame, 8);

/// Dimensionless sign pair selecting a corner of a centred effect quad.
///
/// Multiply each sign by the corresponding half-extent, then narrow to a
/// signed 16-bit coordinate. The drawer chooses the two local axes (usually
/// X/Z, sometimes Y/Z) before transforming the quad. Unsigned reads preserve
/// the same low 16 bits when the product is narrowed.
typedef struct {
    s16 axis0Sign; // First local-axis factor (-1 or +1).
    s16 axis1Sign; // Second local-axis factor (-1 or +1).
} EffectUnitQuadCorner;
STATIC_ASSERT_SIZEOF(EffectUnitQuadCorner, 0x4);

/// 0x38-byte scratch from the scratch stack used by `Gp_DrawEffSprite7C` and
/// `Room_Draw16`. `vec[]` holds the four rotated + translated quad corners
/// fed to the GTE; `otz` is `gte_stszotz` (then incremented by the sprite
/// helpers, not by `Room_Draw16`), `flag` is `gte_stflg`, and `sxy0` (RTPS
/// of `vec[0]`) plus `sxy1`..`sxy3` (RTPT of the rest) are the projected
/// screen positions copied into the `POLY_FT4`.
typedef struct _GpQuadScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ DVECTOR sxy0;
    /* 0x2C */ DVECTOR sxy1;
    /* 0x30 */ DVECTOR sxy2;
    /* 0x34 */ DVECTOR sxy3;
} GpQuadScratch;
STATIC_ASSERT_SIZEOF(GpQuadScratch, 0x38);

/// Scratch-stack workspace for a spinning textured billboard about one world point.
///
/// One perspective transform supplies the centre, GTE status and SZ3 / 4
/// depth. Drawers may increment `depth` before using it for both screen sizing
/// and ordering-table placement. `cornerOffsetX` and `cornerOffsetY` hold the
/// signed pixel displacement to one pair of opposite corners, then are reused
/// at a quarter turn for the other pair. Some drawers narrow these offsets
/// before addition; others retain their full words until the GPU halfword store.
///
/// `screenX` and `screenY` retain the raw 16-bit encodings of signed GTE pixel
/// coordinates. They are adjacent so a single GTE word store fills both.
/// Reserve a complete, word-aligned block and initialize fields as needed;
/// release it in scratch-stack order after drawing. No pointer into it survives
/// release. This layout differs from the corner form of `EffectShapeScratch`.
typedef struct {
    s32     depth;           // SZ3 / 4, optionally biased; divisor for sizing and depth for sorting
    s32     cornerOffsetX;   // Signed horizontal displacement from the centre to a corner, in pixels
    s32     cornerOffsetY;   // Signed vertical displacement from the centre to a corner, in pixels
    s32     projectionFlags; // GTE FLAG word; a negative value rejects the projection
    SVECTOR worldPoint;      // World position, with each translation component narrowed to s16
    u16     screenX;         // Raw projected centre X; first half of the GTE screen-position word
    u16     screenY;         // Raw projected centre Y; second half of the GTE screen-position word
} EffectBillboardScratch;
STATIC_ASSERT_SIZEOF(EffectBillboardScratch, 0x1C);

#endif // GAMEPLAY_EFFECTS_H
