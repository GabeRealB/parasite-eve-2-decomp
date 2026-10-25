#ifndef STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_NIGHT_H
#define STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_NIGHT_H

#include "gameplay/collision.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "rooms/room_common.h"

/// Point-light collection shared by both room selectors.
///
/// Contains no directional or cone lights. The room owns the mutable light
/// array; coordinate setup and queries require this overlay to remain loaded.
extern WorldCoordRoomLights gDryfieldNightFactoryRoomLights;

/// 14 directed camera boundaries in room-local game units.
///
/// Source/destination views are 1-based: 3->2, 2->3, 4->7, 7->4, 8->7, 7->8, 4->6, 6->4, 8->9,
/// 9->8, 4->3, 3->4, 2->6, 6->2.
/// Room binding installs the view coordinate and enables all records through LAST.
/// Normals use 4096 per unit; keep the writable list live until it is unlinked.
extern WorldCollisionTrigger gDryfieldNightFactoryViewBoundaryTriggers[14];

/// Minimum ambient colours for logical views 1..19.
///
/// Entry zero stores the count; each subsequent entry belongs to that 1-based
/// view. RGB uses 16 units per 8-bit colour level. Lighting reads the table
/// while this overlay is loaded and uses default minima above its count.
extern WorldCoordRoomAmbientEntry gDryfieldNightFactoryAmbientByView[20];

/// 19 interaction quads: 3 warps, 6 room commands, 10 CAP commands.
///
/// Room-local origins/corners and radii use game units; normals use 4096 per unit.
/// Warp bytes select destination area and departure/arrival endpoint nibbles;
/// CAP IDs index the loaded command table or route through the room-message sentinel.
/// Room binding enables all records through LAST; keep them live until unlinking.
extern WorldCollisionTrigger gDryfieldNightFactoryActionTriggers[19];

extern u8 gRoomEventActive;

extern RoomEventReq gRoomEventReq;

/// Empty sprite-batch list retaining the decoded background in mapped view 1.
///
/// The zero-count first record precedes `SPRITE_BATCH_END`. The view retains
/// this list's address in its empty source slot; it supplies no source sprites.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView1SpriteBatches[2];

/// 13 mutable sprite rectangles for mapped view 2.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView2SpriteSources[13];

/// Sprite ranges retaining the decoded background in mapped view 2.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 13 elements of `gDryfieldNightFactoryView2SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView2SpriteBatches[6];

/// 12 mutable sprite rectangles for mapped view 3.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView3SpriteSources[12];

/// Sprite ranges retaining the decoded background in mapped view 3.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 12 elements of `gDryfieldNightFactoryView3SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView3SpriteBatches[5];

/// 82 mutable sprite rectangles for mapped view 4.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView4SpriteSources[82];

/// Sprite ranges retaining the decoded background in mapped view 4.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 82 elements of `gDryfieldNightFactoryView4SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView4SpriteBatches[7];

/// Empty sprite-batch list retaining the decoded background in mapped view 5.
///
/// The zero-count first record precedes `SPRITE_BATCH_END`. The view retains
/// this list's address in its empty source slot; it supplies no source sprites.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView5SpriteBatches[2];

/// 32 mutable sprite rectangles for mapped view 6.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView6SpriteSources[32];

/// Sprite ranges retaining the decoded background in mapped view 6.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 32 elements of `gDryfieldNightFactoryView6SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView6SpriteBatches[7];

/// 39 mutable sprite rectangles for mapped view 7.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView7SpriteSources[39];

/// Sprite ranges retaining the decoded background in mapped view 7.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 39 elements of `gDryfieldNightFactoryView7SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView7SpriteBatches[8];

/// 12 mutable sprite rectangles for mapped view 8.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView8SpriteSources[12];

/// Sprite ranges retaining the decoded background in mapped view 8.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 12 elements of `gDryfieldNightFactoryView8SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView8SpriteBatches[3];

/// 4 mutable sprite rectangles for mapped view 9.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView9SpriteSources[4];

/// Sprite ranges retaining the decoded background in mapped view 9.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 4 elements of `gDryfieldNightFactoryView9SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView9SpriteBatches[3];

/// Empty sprite-batch list retaining the decoded background in mapped view 10.
///
/// The zero-count first record precedes `SPRITE_BATCH_END`. The view retains
/// this list's address in its empty source slot; it supplies no source sprites.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView10SpriteBatches[2];

/// Empty sprite-batch list retaining the decoded background in mapped view 11.
///
/// The zero-count first record precedes `SPRITE_BATCH_END`. The view retains
/// this list's address in its empty source slot; it supplies no source sprites.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView11SpriteBatches[2];

/// Empty sprite-batch list retaining the decoded background in mapped view 12.
///
/// The zero-count first record precedes `SPRITE_BATCH_END`. The view retains
/// this list's address in its empty source slot; it supplies no source sprites.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView12SpriteBatches[2];

/// Empty sprite-batch list retaining the decoded background in mapped view 13.
///
/// The zero-count first record precedes `SPRITE_BATCH_END`. The view retains
/// this list's address in its empty source slot; it supplies no source sprites.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView13SpriteBatches[2];

/// Empty sprite-batch list retaining the decoded background in mapped view 14.
///
/// The zero-count first record precedes `SPRITE_BATCH_END`. The view retains
/// this list's address in its empty source slot; it supplies no source sprites.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView14SpriteBatches[2];

/// 78 mutable sprite rectangles for mapped view 15.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView15SpriteSources[78];

/// Sprite ranges retaining the decoded background in mapped view 15.
///
/// The zero-count first record selects image strips; subsequent records cover
/// all 78 elements of `gDryfieldNightFactoryView15SpriteSources` and end with `SPRITE_BATCH_END`.
/// Storage remains valid while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView15SpriteBatches[6];

/// 32 mutable sprite rectangles for mapped view 16.
///
/// Positions are view-centred pixels; depths are unscaled sorting values.
/// The view's batches select ranges in source elements, without a source
/// terminator. Storage remains valid while the night factory overlay is loaded.
extern SpriteSource gDryfieldNightFactoryView16SpriteSources[32];

/// Three contiguous foreground ranges covering all 32 mapped-view-16 sprites.
///
/// The leading zero count retains decoded background strips; the final record
/// ends with `SPRITE_BATCH_END`. Source indices count elements. Visibility stays
/// writable, and authored metadata bytes retain their unproven meaning. Borrow
/// the list and its source pool while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView16SpriteBatches[5];

/// Mapped view 17's background-only sprite list.
///
/// A zero-count selector precedes `SPRITE_BATCH_END`, retaining decoded image
/// strips. The same address occupies `sources.empty` and supplies no sprites.
/// Borrow this distinct authored list while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView17SpriteBatches[2];

/// Seventeen mutable foreground rectangles shared by mapped views 18 and 19.
///
/// Positions are pixels relative to the view centre; dimensions are pixels.
/// UVs use texels, depth is unscaled and RGB 128 is neutral modulation. Both descriptors
/// borrow this same pool and its batch list, with no source sentinel. Keep the
/// night factory overlay loaded during packet allocation and drawing.
extern SpriteSource gDryfieldNightFactoryView18And19SpriteSources[17];

/// Four contiguous sprite ranges shared by mapped views 18 and 19.
///
/// Ranges cover the shared 17-source pool in elements. A leading zero count
/// retains decoded background strips, and `SPRITE_BATCH_END` ends the list.
/// Visibility is mutable; authored metadata bytes have unproven meaning. Both
/// descriptors borrow this list while the night factory overlay is loaded.
extern SpriteBatch gDryfieldNightFactoryView18And19SpriteBatches[6];

// Callbacks referenced by the overlay's shared data tables.

#endif // STAGES_DRYFIELD_ROOMS_FACTORY_FACTORY_NIGHT_H
