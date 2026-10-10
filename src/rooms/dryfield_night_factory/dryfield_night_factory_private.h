#ifndef SRC_ROOMS_DRYFIELD_NIGHT_FACTORY_DRYFIELD_NIGHT_FACTORY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_FACTORY_DRYFIELD_NIGHT_FACTORY_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

#include "rooms/room_common.h"

/// Point-light collection shared by both room selectors.
///
/// Contains no directional or cone lights. The room owns the mutable light
/// array; coordinate setup and queries require this overlay to remain loaded.
extern WorldCoordRoomLights gDryfieldNightFactoryRoomLights;

extern WorldCollisionTrigger D_dryfield_night_factory_80189CA0[14];

/// Minimum ambient colours for logical views 1..19.
///
/// Entry zero stores the count; each subsequent entry belongs to that 1-based
/// view. RGB uses 16 units per 8-bit colour level. Lighting reads the table
/// while this overlay is loaded and uses default minima above its count.
extern WorldCoordRoomAmbientEntry gDryfieldNightFactoryAmbientByView[20];

extern WorldCollisionTrigger D_dryfield_night_factory_8018A168[19];

/// Snapshot of the accepted transition, copied by the gate for the deferred event.
///
/// All eight bytes are replaced on another accepted start. Keep this room's
/// snapshot unchanged and loaded until the task commits its destination.
extern RoomEventMsg gRoomEventMsg;

/// Selects this overlay's externally linked transition snapshot for the shared event code.
#define ROOM_EVENT_MSG gRoomEventMsg

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

extern SpriteBatch D_dryfield_night_factory_80189858[5];

extern SpriteBatch D_dryfield_night_factory_80189880[2];

extern SpriteSource D_dryfield_night_factory_80189890[17];

extern SpriteBatch D_dryfield_night_factory_801899E4[6];

// Callbacks referenced by the overlay's shared data tables.

#endif // SRC_ROOMS_DRYFIELD_NIGHT_FACTORY_DRYFIELD_NIGHT_FACTORY_PRIVATE_H
