#ifndef GAMEPLAY_EFFECTS_H
#define GAMEPLAY_EFFECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

struct GfxCoord;
struct Task;

/// Scratch-stack workspace for drawing an effect around one projected centre.
///
/// `worldPoint` optionally stages a world position narrowed to signed 16-bit
/// coordinate units; a drawer may first transform a local offset in place or
/// project the caller's vector directly. One RTPS supplies the screen centre,
/// GTE status and SZ3 / 4 depth. Drawers may bias or clamp `depth` before using
/// it for sizing and ordering-table placement. `screenExtent` is a signed
/// integer pixel distance, used as a radius or half-extent and reused when
/// drawing another primitive or changing axes.
///
/// `screenX` and `screenY` retain the raw 16-bit encodings of signed GTE pixel
/// coordinates. One GTE word store fills both, starting at `screenX`.
/// Reserve a complete, word-aligned block, initialize fields as needed, and
/// release it in scratch-stack order after drawing. Pointers into the block
/// must not survive release.
typedef struct {
    SVECTOR worldPoint;      // Optional position workspace; world coordinates at projection
    s32     depth;           // SZ3 / 4, then the drawer's bias or clamp for sizing and sorting
    s32     projectionFlags; // GTE FLAG word; bit 31 makes it negative and rejects the projection
    s32     screenExtent;    // Signed pixel radius or half-extent, reused between primitives or axes
    u16     screenX;         // Raw projected centre X; first half of the GTE screen-position word
    u16     screenY;         // Raw projected centre Y; adjacent to screenX for the GTE word store
} EffectCentreScratch;
STATIC_ASSERT_SIZEOF(EffectCentreScratch, 0x18);

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

/// Number of vertices in each ring of an `EffectBandScratch`, and so the number
/// of quads in the band. A power of two: drawers wrap the next vertex index
/// with `& (EFFECT_BAND_SEGMENT_COUNT - 1)`.
#define EFFECT_BAND_SEGMENT_COUNT 16

/// Scratch-stack workspace for drawing a band of quads between two rings.
///
/// A drawer places `EFFECT_BAND_SEGMENT_COUNT` vertices evenly round each ring
/// in a coordinate frame's local space, moves each through that frame's world
/// matrix and translation, and stores the result back as a world position
/// narrowed to 16 bits. The rings may differ in radius, offset or both. Each
/// segment `i` then becomes one quad whose vertices 0 and 1 are `topRing[i]`
/// and `topRing[i + 1]` and whose vertices 2 and 3 are `bottomRing[i]` and
/// `bottomRing[i + 1]`, wrapping at the last segment: the top ring is the
/// textured quads' top row and the lit edge of a Gouraud band.
///
/// Reserve one complete block on the scratch stack and release it in reverse
/// order after drawing. Pointers into the block must not survive its release.
typedef struct {
    SVECTOR topRing[EFFECT_BAND_SEGMENT_COUNT];    // World-space vertices forming each quad's vertices 0 and 1
    SVECTOR bottomRing[EFFECT_BAND_SEGMENT_COUNT]; // World-space vertices forming each quad's vertices 2 and 3
    s32     otz;                                   // Ordering-table depth: SZ3 / 4 of the quad's last vertex, plus 1 in some drawers
    s32     projectionFlags;                       // GTE FLAG word after the segment's RTPT; bit 31 makes it negative and drops the quad
    DVECTOR sxy0;                                  // Screen position of the current quad's vertex 0
    DVECTOR sxy1;                                  // Screen position of vertex 1
    DVECTOR sxy2;                                  // Screen position of vertex 2
    DVECTOR sxy3;                                  // Screen position of vertex 3
} EffectBandScratch;
STATIC_ASSERT_SIZEOF(EffectBandScratch, 0x118);

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

/// Scratch-stack workspace for projecting one four-corner effect quad.
///
/// `vertices` stages local corners and is reused for their rotated, translated
/// world positions, narrowed to signed 16-bit coordinate units. Corners and
/// screen positions share indices 0..3 in GPU quad strip order.
///
/// One RTPS projects corner 0 and one RTPT projects corners 1..3. `depth` is
/// the last corner's SZ3 divided by four, optionally biased before ordering
/// the primitive. `projectionFlags` holds the most recently stored GTE FLAG
/// word.
/// The drawer decides which projections to reject before copying the screen
/// positions to a textured or Gouraud-shaded quad packet.
///
/// Reserve one complete, word-aligned block and initialize fields as needed.
/// Release it in scratch-stack order after drawing; pointers into the block
/// must not survive release.
typedef struct {
    SVECTOR vertices[4];      // Local corner workspace, then world positions supplied to the projection
    s32     depth;            // Last projected corner's SZ3 / 4, with the drawer's ordering bias
    s32     projectionFlags;  // Latest GTE FLAG word; bit 31 makes it negative and rejects that projection
    DVECTOR screenCorners[4]; // Signed screen X/Y pixels, written together as one GTE word per corner
} EffectQuadScratch;
STATIC_ASSERT_SIZEOF(EffectQuadScratch, 0x38);

/// Scratch-stack workspace for projecting one four-corner quad straight into its packet.
///
/// `vertices` stages each corner and is reused for its rotated, translated
/// world position, narrowed to signed 16-bit coordinate units. Corners share
/// indices 0..3 in GPU quad strip order. One RTPS projects corner 0 and one
/// RTPT corners 1..3, and the screen positions are stored directly in the
/// primitive, so unlike `EffectQuadScratch` the block keeps neither them nor
/// a GTE FLAG word.
///
/// `depth` is the last corner's SZ3 divided by four, optionally biased by the
/// drawer. Most drawers skip a quad whose depth is below 0x11 (too near the
/// camera); otherwise it selects the ordering-table entry and the blend
/// packet's depth.
///
/// Reserve one complete, word-aligned block and initialize fields as needed.
/// Release it in scratch-stack order after drawing; pointers into the block
/// must not survive release.
typedef struct {
    s32     depth;       // Last projected corner's SZ3 / 4, with the drawer's ordering bias
    SVECTOR vertices[4]; // Local corner workspace, then world positions supplied to the projection
} EffectQuadCornersScratch;
STATIC_ASSERT_SIZEOF(EffectQuadCornersScratch, 0x24);

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
