#ifndef GAMEPLAY_SPRITES_H
#define GAMEPLAY_SPRITES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

/// Sentinel in the 16-bit `SpriteBatch.firstSprite` index ending a view's batch list.
///
/// The terminal record contributes no sprites, regardless of `spriteCount`;
/// a zero-count nonterminal record does not end the list. The first record's
/// count still selects background presentation, even when that record is terminal.
enum { SPRITE_BATCH_END = 0xFFFF };

/// A contiguous range of source sprites in a view's drawing list.
///
/// Room tables own these mutable, eight-byte records. `SpriteView.batches`
/// points to a list ending with `firstSprite == SPRITE_BATCH_END`. Each
/// nonterminal range indexes that view's `SpriteView.sources.elements`; its
/// start plus count must fit the source array.
///
/// The first record's count also selects background presentation: zero requests
/// decoded image strips and is skipped by cached-sprite linking; nonzero
/// suppresses the strips. This test applies even to a terminal first record.
/// Hidden ranges retain their positions in the cached packet buffers, while
/// `skipCachedPackets` ranges do not advance the initialization/link/shade
/// cursors. Allocation still reserves space for all nonterminal counts.
typedef struct {
    u16  firstSprite;       // Zero-based source index, or SPRITE_BATCH_END
    u16  spriteCount;       // Number of source elements; first record also selects the background
    u8   hidden;            // Cached sprites (0 linked for drawing, nonzero hidden)
    u8   skipCachedPackets; // Cached packet processing (0 included, nonzero excluded)
    byte field_6[2];        // Serialized bytes; role and subdivision unproven
} SpriteBatch;
STATIC_ASSERT_SIZEOF(SpriteBatch, 8);

/// GPU command bits carried by `SpriteSource.codeFlags`.
///
/// Zero selects opaque, colour-modulated emission; the bits may be combined.
/// Raw texture emission ignores RGB and skips copying it into the packet.
enum {
    SPRITE_SOURCE_RAW_TEXTURE      = 0x01,
    SPRITE_SOURCE_SEMI_TRANSPARENT = 0x02,
};

/// Mutable texture rectangle and sorting depth for a room-view sprite.
///
/// Room overlays own arrays referenced by `SpriteView.sources.elements`;
/// `SpriteBatch` selects ranges in elements, with no sentinel in the source
/// array itself. The selected overlay and each range must remain valid while
/// allocating packets or linking them for drawing.
///
/// Position is the rectangle's upper-left corner in pixels relative to the
/// drawing origin (normally the view centre). Depth is an unscaled sorting
/// value: its OT slot is `((u32)depth << gDisplayState.otDepthShift) >> 4 & 0x3FF`.
///
/// Packet allocation snapshots texture, position, size and code flags into both
/// frame buffers; cached linking reads depth from the source each frame. Cached
/// packets start with raw texture enabled and do not copy source RGB. Direct
/// emission copies RGB unless `SPRITE_SOURCE_RAW_TEXTURE` is set.
/// The SDK-compatible XY and RGB member names support aligned whole-word reads;
/// an RGB read includes `codeFlags` as its high byte.
typedef struct {
    u16 tpage; // Encoded GPU texture page and blend mode
    u16 clut;  // Encoded GPU colour lookup-table address
    union {
        struct {
            s16 w;  // Rectangle width in pixels, also the sampled texture width
            s16 h;  // Rectangle height in pixels, also the sampled texture height
        } fields;   // Individual dimensions in the SDK's signed-halfword representation
        u32 packed; // Width in the low halfword, height in the high halfword
    } size;         // Unscaled dimensions copied together into the SPRT packet
    s16 x0;         // Upper-left X in pixels relative to the drawing origin
    s16 y0;         // Upper-left Y in pixels relative to the drawing origin
    u16 depth;      // Unscaled OT sorting depth, quantized and masked when linking
    union {
        struct {
            u8 u0;  // Left texture coordinate in texels within the texture page
            u8 v0;  // Top texture coordinate in texels within the texture page
        } fields;   // Individual texture coordinates
        u16 packed; // U in the low byte, V in the high byte
    } uv;           // Texture origin copied together into the SPRT packet
    u8 r0;          // Red modulation (128 neutral), used by colour-modulated direct emission
    u8 g0;          // Green modulation (128 neutral), used by colour-modulated direct emission
    u8 b0;          // Blue modulation (128 neutral), used by colour-modulated direct emission
    u8 codeFlags;   // GPU code bits (0 opaque modulation, bit 0 raw texture, bit 1 semitransparent)
} SpriteSource;
STATIC_ASSERT_SIZEOF(SpriteSource, 0x14);

/// Sentinel in `SpriteDrawArea.restoreDepth` ending a view's draw-area list.
enum { SPRITE_DRAW_AREA_END = 0xFFFF };

/// A view clipping rectangle and the sorting depth that restores full-screen drawing.
///
/// Room overlays own lists referenced by `SpriteView.drawAreas`; NULL selects no
/// clipping commands. Lists end with `restoreDepth == SPRITE_DRAW_AREA_END`;
/// the renderer emits no command for the terminal record. The overlay and list
/// must remain valid while building drawing commands. Frame capture consults
/// only the first record when choosing its restored clip.
///
/// Coordinates and dimensions are pixels measured from the draw buffer's
/// upper-left corner; emission adds the buffer's VRAM Y origin (0 or 272).
/// The renderer queues the clip at OT slot 1023 and a 320x240 full-screen restore at
/// `((u32)restoreDepth << gDisplayState.otDepthShift) >> 4 & 0x3FF`.
/// This is a masked sorting value, not a clamped OT index; it clips primitives
/// drawn before the restore command, including scene objects as well as sprites.
typedef struct {
    RECT clipRect;     // Clipping rectangle in draw-buffer pixels, before the VRAM Y offset
    u16  restoreDepth; // Unscaled full-screen restore depth (0..65534, SPRITE_DRAW_AREA_END ends the list)
} SpriteDrawArea;
STATIC_ASSERT_SIZEOF(SpriteDrawArea, 0xA);

/// Borrowed sprite sources, drawing batches and clipping areas for one room view.
///
/// Room overlays own arrays selected through `gSpriteAreaTables` by stage and area.
/// The 1-based byte from `gViewIndexTables` selects an element in that area's
/// array; it must be nonzero and within the array's extent. The descriptor and
/// its referenced lists must remain valid while allocating or drawing sprites.
///
/// Nonterminal batches index `sources.elements` in source elements, with each
/// start plus count within that array. Views without source sprites may store
/// NULL or retain their batch-list address as `sources.empty`; neither supplies
/// source elements. Batch lists always include a terminal record, whose count
/// can still select background presentation. `drawAreas` is independently
/// nullable and clips depth-sorted scene drawing as well as sprites.
typedef struct {
    union {
        SpriteSource* elements; // Mutable source array, or NULL when no source elements are selected
        SpriteBatch*  empty;    // Retained batch-list address for a view with no source sprites
    } sources;                  // Alternative source-slot interpretations; empty is never dereferenced as sprites
    SpriteBatch*    batches;    // Mutable range list ending with firstSprite == SPRITE_BATCH_END
    SpriteDrawArea* drawAreas;  // Borrowed clip list ending with restoreDepth == SPRITE_DRAW_AREA_END, or NULL
} SpriteView;
STATIC_ASSERT_SIZEOF(SpriteView, 0xC);

/// A stage map's directory of per-area sprite-view arrays.
///
/// `gSpriteAreaTables[stage - 1]` selects the record owned by that stage's map
/// overlay. Callers that index `[spriteVariant - 1]` or `[0]` address the
/// same record: each published stage entry is one record and the session
/// sprite variant is 1. The record stores no further variant slots.
///
/// `areaViews` is indexed by the 1-based area minus one. An entry borrows
/// that area's room-overlay `SpriteView` array, or is NULL when the area has
/// no sprite views. The mapped 1-based view index then selects an element at
/// index minus one. Neither level stores a count or terminator. Lookups
/// require a valid 1-based stage and area within the loaded directory, a
/// non-NULL area entry, and a view index inside that array. The pointer
/// array and the view arrays stay valid while their owning map and room
/// overlays are loaded. Consumers read the directory and may change batch
/// visibility and source geometry in the borrowed view records.
typedef struct {
    SpriteView** areaViews; // Borrowed per-area sprite-view arrays, indexed by area - 1; NULL when that area has none
} SpriteAreaTable;
STATIC_ASSERT_SIZEOF(SpriteAreaTable, 4);

/// Variable-size GPU sprite primitive, as an SDK `SPRT` and as transfer words.
///
/// Room-sprite emission stores this 20-byte primitive as the sprite half of a
/// `SpriteDrawModePacket`, per frame and in the cached sprite lists. `sprt` is the SDK primitive: GPU
/// macros take its address, and the command byte and CLUT are updated through
/// it. `packed` groups the same bytes into the copies taken from a
/// `SpriteSource`. Colour, the upper-left position and the size move as words;
/// the texture origin moves as a halfword. Storing either member replaces the
/// bytes they share.
///
/// A colour-word store replaces the command byte. Writers set the sprite
/// code after that store.
typedef union {
    SPRT sprt;        // SDK sprite. Command byte is 0x64 plus SPRITE_SOURCE_RAW_TEXTURE and SPRITE_SOURCE_SEMI_TRANSPARENT
    struct {
        u32 tag;      // Next primitive address and packet length (4 words after the tag)
        u32 color;    // Red, green, blue, and the command byte in bits 24..31
        u32 position; // Upper-left X in the low halfword, Y in the high halfword, in pixels
        u16 uv;       // Texture U in the low byte, V in the high byte
        u16 clut;     // Encoded GPU colour lookup-table address
        u32 size;     // Width in the low halfword, height in the high halfword, in pixels
    } packed;         // Word and halfword transfers of the same primitive
} SpritePacket;
STATIC_ASSERT_SIZEOF(SpritePacket, 0x14);

/// A textured sprite preceded by the draw-mode command it is drawn with.
///
/// Writers give `drawMode` one word and `sprite` four, then `MargePrim` the
/// sprite onto the draw-mode packet, so both travel as one 6-word GPU packet
/// behind `drawMode`'s tag: one ordering-table link sets the texture page and
/// blending, then draws the sprite. The merge only adds the lengths and does
/// not check adjacency; it is valid because the sprite follows the draw-mode
/// word directly, and the sprite's zeroed tag is sent as a no-op. The packets are
/// built per frame in the `gGpuPrimCursor` arena, or once into the cached
/// dual-buffer room sprite lists and relinked each frame.
typedef struct {
    DR_TPAGE     drawMode; // Packet tag, then the GPU draw-mode word (0xE1000000 | texture page and blend bits)
    SpritePacket sprite;   // Sprite primitive; the merge zeroes its tag word
} SpriteDrawModePacket;
STATIC_ASSERT_SIZEOF(SpriteDrawModePacket, 0x1C);

#endif // GAMEPLAY_SPRITES_H
