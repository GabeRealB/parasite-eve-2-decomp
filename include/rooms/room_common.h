#ifndef INCLUDE_ROOMS_ROOM_COMMON_H
#define INCLUDE_ROOMS_ROOM_COMMON_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/display.h"

#include "main/coord.h"
#include "main/ui_types.h"

/// Per-room screen-fade record and the word that follows it.
///
/// Callers pass `fade` to the resident full-screen fade. The following word
/// is zero in every room that carries this allocation, and nothing reads or
/// writes it. Its role is unproven.
typedef struct {
    ScreenFade fade;    // Record passed to the resident full-screen fade
    u32        field_4; // Role unproven; zero, with no reads or writes
} RoomFadeStorage;
STATIC_ASSERT_SIZEOF(RoomFadeStorage, 8);

/// Screen rectangle outlined by `Room_Draw26`: the corners it draws are
/// (`x`, `y`) and (`x + w`, `y + h`), so `w` and `h` are extents rather than a
/// second corner. The fields are unsigned because the drawer loads every one
/// of them with `lhu`.
typedef struct RoomRect {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
    /* 0x4 */ u16 w;
    /* 0x6 */ u16 h;
} RoomRect;
STATIC_ASSERT_SIZEOF(RoomRect, 0x8);

/// Work block of a room task that asks the player to choose between two options.
///
/// The task allocates it, parks it in `Task::work` and passes `request` to
/// `Ui_SpawnTextBlock`. The option dialog keeps that pointer while it is open
/// and reads the list through it, so the request and the two nodes its
/// `options` head points at share one allocation, which the task's teardown
/// frees. The task polls `request.result` for the answer.
typedef struct {
    UiOptionDialogRequest request;    // Dialog request and the place its answer arrives; lists both nodes below
    UiDialogOption        options[2]; // The two rows in display order, linked first to second
} RoomOptionDialog;
STATIC_ASSERT_SIZEOF(RoomOptionDialog, 0x20);

/// 0xA4 work block a shop / vending-machine panel task allocates and parks in
/// `Task::work`: the `UiList` the panel is drawn from, followed by the ids of
/// the items the room currently offers. The overlay's list builder fills
/// `items` while counting them into `list.itemCount`, then sorts that prefix in
/// place, so one allocation carries both the list state and its contents.
typedef struct RoomShopList {
    /* 0x00 */ UiList list;
    /* 0x24 */ u16    items[0x40];
} RoomShopList;
STATIC_ASSERT_SIZEOF(RoomShopList, 0xA4);

/// 0xC4 work block the "Play Data" item-usage panel allocates and parks in
/// `Task::work`. The builder walks item ids 0x80-0x9F, keeps the ones the save
/// has a non-zero use count for, and fills three parallel arrays indexed by the
/// row the list is drawing: the item id, the share of all recorded uses in
/// hundredths of a percent (0-10000, printed as `NN.NN%`), and the width of the
/// row's gauge as a 12-bit fraction of the panel's inner width. The tail of the
/// allocation is unused.
typedef struct RoomItemUsage {
    /* 0x00 */ s16  itemIds[0x20];
    /* 0x40 */ s16  percents[0x20];
    /* 0x80 */ s16  barWidths[0x20];
    /* 0xC0 */ byte pad_C0[0x4];
} RoomItemUsage;
STATIC_ASSERT_SIZEOF(RoomItemUsage, 0xC4);

/// 0xC4 work block the "Play Data" PE-usage panel allocates and parks in
/// `Task::work`, laid out exactly like `RoomItemUsage`. The builder walks the
/// twelve Parasite Energy slots, keeps the ones the save has a non-zero use
/// count for, and fills three parallel arrays indexed by the row the list is
/// drawing: the id of the slot's known level, that slot's share of all recorded
/// uses in hundredths of a percent (0-10000, printed as `NN.NN%`), and the
/// width of the row's gauge as a 12-bit fraction of the panel's inner width.
/// The tail of the allocation is unused.
typedef struct RoomPeUsage {
    /* 0x00 */ s16  peIds[0x20];
    /* 0x40 */ s16  percents[0x20];
    /* 0x80 */ s16  barWidths[0x20];
    /* 0xC0 */ byte pad_C0[0x4];
} RoomPeUsage;
STATIC_ASSERT_SIZEOF(RoomPeUsage, 0xC4);

/// 0xAC work block the mirror-reflection task keeps at `Task::work`. Rooms
/// with a reflective surface (the Acropolis elevator halls and square, motel
/// room 6, the Neo Ark observatory) spawn a task that re-attaches the player's
/// own TMD source and draws it through `coord`, which is `gGfxViewCoord` with
/// one GTE rotation column negated.
///
/// `viewFlg` caches `gGfxViewCoord.composeStamp & GRAPHICS_COORD_STAMP_MASK` so the
/// mirror only rebuilds its matrices when the view moves, and `field_4` marks the block as live;
/// both are set to their "dirty" values (`-1` / `0`) as the task starts so the
/// first frame always rebuilds. `light` and `color` are the matrices hung off
/// the clone's `TmdObject`, `field_A0` is the screen-space clip rectangle
/// (-160, 160, -120, 120) and `configRev` caches `gPlayerStatus.weapon`.
typedef struct RoomMirrorWork {
    /* 0x00 */ s32      viewFlg;
    /* 0x04 */ s32      field_4;
    /* 0x08 */ s32      field_8;
    /* 0x0C */ s32      field_C;
    /* 0x10 */ GfxCoord coord;
    /* 0x60 */ MATRIX   light;
    /* 0x80 */ MATRIX   color;
    /* 0xA0 */ s16      field_A0[4];
    /* 0xA8 */ s32      configRev;
} RoomMirrorWork;
STATIC_ASSERT_SIZEOF(RoomMirrorWork, 0xAC);

/// Request a room hands its event gate. The gate starts the event when the
/// game-flag nibble allows it and any required collectible is held; the event
/// then runs the capture and the two sound events before changing room.
///
/// A positive `flagId` allows the event while that nibble is 0 and is stored
/// as 1 when the event starts. A negative `flagId` allows it while the nibble
/// is nonzero and is stored as 0 when the event starts. `collectedBit` 0
/// requires nothing. Either sound is skipped when its id is 0.
typedef struct {
    s32 capCmd;        // CAP command run when the event starts
    s32 missingCapCmd; // CAP command run when a required collected bit is missing
    s32 firstSnd;      // Sound event played first after the event starts; 0 skips it
    s32 secondSnd;     // Sound event played after `firstSnd` finishes; 0 skips it
    s16 flagId;        // Signed game-flag nibble index (magnitude 0..503); the sign selects the polarity above
    s16 collectedBit;  // Collected-item bit required first (item id & 0x7F); 0 for none
} RoomEventReq;
STATIC_ASSERT_SIZEOF(RoomEventReq, 0x14);

/// Four-byte room storage for the event-start indication.
///
/// `eventStarted` records whether the latest event-gate call latched a
/// request and spawned its task. Each call clears it first, including queries.
/// The three trailing initialized bytes have no recovered access; their role
/// and grouping are unproven.
typedef struct {
    u8 eventStarted; // Latest gate call spawned an event (0 no, 1 yes)
    u8 unknown[3];   // Initialized image bytes; role and grouping unproven
} RoomEventActiveBytes;
STATIC_ASSERT_SIZEOF(RoomEventActiveBytes, 4);

/// Eight-byte room storage for an event-start indication.
///
/// `eventStarted` records whether the latest call to the corresponding event
/// gate latched an event and spawned its task. Each call clears it first,
/// including queries; an eligible query leaves it clear even when it returns 2.
/// The event task does not update it. The seven trailing zero bytes have no
/// recovered access; their role and grouping are unproven.
typedef struct {
    u8 eventStarted; // Latest gate call spawned an event (0 no, 1 yes)
    u8 unknown[7];   // Zero image bytes; role and grouping unproven
} RoomEventStartStorage;
STATIC_ASSERT_SIZEOF(RoomEventStartStorage, 8);

/// Latched room-event request with twelve bytes of unidentified trailing storage.
///
/// The room owns this storage. Starting an event copies only the twenty-byte
/// `request`, preserving the caller's stack-built request for the event task's
/// later CAP command and sound playback. Queries do not replace the request;
/// it remains latched until another event starts or the room is unloaded.
/// The trailing bytes are zero in the room image and have no recovered access.
/// Their role and grouping are unproven.
typedef struct {
    RoomEventReq request;     // Complete request copied when an event starts
    u8           unknown[12]; // Unidentified trailing bytes; role and grouping unproven
} RoomEventReqStorage;
STATIC_ASSERT_SIZEOF(RoomEventReqStorage, 0x20);

/// Scratch-stack block for drawing one glow sprite centred on a projected point.
///
/// Room glow drawers - lamps, flares, stars, beacons and flashes - fill
/// `worldPos`, project it through `GsWSMATRIX` with one `RTPS`, and draw only
/// when `otz` exceeds 0x10. The sprite's world size divided by `otz` gives
/// `halfExtent`, so the sprite shrinks with distance. Reserve the complete
/// block and release it in scratch-stack order after drawing.
typedef struct {
    s32     otz;        // Projected depth (SZ3 / 4); also the ordering-table depth and blend depth
    s32     halfExtent; // On-screen half size or radius scale in pixels: a world size divided by `otz`
    SVECTOR worldPos;   // Sprite centre in world coordinates, the input to the projection
    DVECTOR screenPos;  // Projected centre in screen pixels, stored as one GTE word
} RoomGlowSpriteScratch;
STATIC_ASSERT_SIZEOF(RoomGlowSpriteScratch, 0x14);

/// Overlay of `Task::spawnArg1` for that task: `phase` steps the shaft's
/// pulsing red channel off the global frame counter, `height` is the length
/// the two halves are drawn at before the `1 / otz` divide.
typedef struct _RoomShaftArg {
    /* 0x0 */ u8   phase;
    /* 0x1 */ u8   height;
    /* 0x2 */ byte pad_2[2];
} RoomShaftArg;
STATIC_ASSERT_SIZEOF(RoomShaftArg, 0x4);

/// Scratch block a room's glow or flare drawer takes from the scratch stack.
/// `vec` is the glow's anchor point in world space; `otz` and `sx` / `sy` are
/// that point projected through `GsWSMATRIX`. `rOuter` and `rInner` are sizes
/// divided by `otz`, so they shrink with distance: the on-screen radii of the
/// glow and of its inner quads.
typedef struct {
    s32     otz;
    s32     rOuter;
    s32     rInner;
    SVECTOR vec;
    u16     sx;
    u16     sy;
} RoomGlowScratch;
STATIC_ASSERT_SIZEOF(RoomGlowScratch, 0x18);

/// Scratch block a glow drawer takes from the scratch stack for one projected
/// centre and the on-screen half-extent of the primitive around it.
///
/// One perspective transform of a world point through `gGfxViewCoord.workm`
/// writes the screen position and the ordering-table depth. The drawer stores
/// the half-extent, a caller size divided by that depth, and builds the
/// primitive around the centre when the depth is at least 17. `sx` and `sy`
/// are written by one screen-XY store, so they stay adjacent.
///
/// `GlowCentreScratch` keeps a GTE flag word between the depth and the
/// half-extent. `GlowCentreRadiusFirstScratch` keeps that flag word between
/// the half-extent and the screen position. Both are 16 bytes. This record
/// holds the depth, the half-extent and the screen position.
typedef struct {
    s32 otz;    // Ordering-table depth of the centre; also the divisor for the half-extent
    s32 radius; // On-screen half-extent of the primitive around the centre
    u16 sx;     // Projected centre, x
    u16 sy;     // Projected centre, y
} GlowCentreRadiusScratch;
STATIC_ASSERT_SIZEOF(GlowCentreRadiusScratch, 0xC);

/// Scratch block a glow drawer takes from the scratch stack for one projected
/// centre.
///
/// One perspective transform of a world point through `gGfxViewCoord.workm`
/// writes the screen position and the GTE flag word. A negative flag word
/// means the transform reported an error, and the drawer links nothing.
/// Otherwise it stores the ordering-table depth and the on-screen half-extent,
/// a size divided by that depth, and builds the primitive around the centre.
/// `sx` and `sy` are written by one screen-XY store, so they stay adjacent.
///
/// `GlowCentreRadiusFirstScratch` is this record with the half-extent and the
/// flag word exchanged. `glowDrawDisc` uses that layout when a room defines
/// `GLOW_DRAW_DISC_SCRATCH` as `GlowCentreRadiusFirstScratch` before including
/// the glow header; otherwise the disc uses this one.
typedef struct {
    s32 otz;    // Ordering-table depth of the centre; also the divisor for the half-extent
    s32 flag;   // GTE flag word; negative means the transform reported an error
    s32 radius; // On-screen half-extent of the primitive around the centre
    u16 sx;     // Projected centre, x
    u16 sy;     // Projected centre, y
} GlowCentreScratch;
STATIC_ASSERT_SIZEOF(GlowCentreScratch, 0x10);

/// Scratch block a two-radius glow drawer takes from the scratch stack for one
/// projected centre.
///
/// Same projected centre as `GlowCentreScratch`: one perspective transform of
/// a world point through `gGfxViewCoord.workm` writes the screen position and
/// the flag word, and a negative flag word means the drawer links nothing.
/// Otherwise it stores the ordering-table depth and two on-screen radii, sizes
/// divided by that depth: the outer one spans the sixteen-wedge disc and the
/// inner one the cross blades drawn over it. `sx` and `sy` are written by one
/// screen-XY store, so they stay adjacent.
typedef struct {
    s32 otz;         // Ordering-table depth of the centre; also the divisor for both radii
    s32 flag;        // GTE flag word; negative means the transform reported an error
    s32 outerRadius; // On-screen radius of the outer disc
    s32 innerRadius; // On-screen radius of the inner cross blades
    u16 sx;          // Projected centre, x
    u16 sy;          // Projected centre, y
} GlowCentreRadiiScratch;
STATIC_ASSERT_SIZEOF(GlowCentreRadiiScratch, 0x14);

/// Scratch block `glowDrawDisc` takes when a room stores the half-extent
/// ahead of the GTE flag word.
///
/// Same projected centre as `GlowCentreScratch`: one perspective transform
/// of a world point through `gGfxViewCoord.workm` writes the screen position
/// and the flag word, and a non-negative flag stores the ordering-table
/// depth and the on-screen half-extent. Those two words are exchanged, so
/// the records stay separate. A room selects this layout by defining
/// `GLOW_DRAW_DISC_SCRATCH` as this type before including the glow header.
typedef struct {
    s32 otz;    // Ordering-table depth of the centre; also the divisor for the half-extent
    s32 radius; // On-screen half-extent of the primitive around the centre
    s32 flag;   // GTE flag word; negative means the transform reported an error
    u16 sx;     // Projected centre, x
    u16 sy;     // Projected centre, y
} GlowCentreRadiusFirstScratch;
STATIC_ASSERT_SIZEOF(GlowCentreRadiusFirstScratch, 0x10);

/// Scratch-stack block for projecting the two ends of a glow and sizing the
/// primitive around each.
///
/// Both points go through `gGfxViewCoord.workm`, one perspective transform
/// each; every transform writes that end's screen position as one screen-XY
/// word, so its two halves stay adjacent, and then its ordering-table depth.
/// No GTE flag word is kept: unlike `OverlayPointPairScratch`, the drawer
/// rejects the pair by the second end's depth alone. The radius at each end
/// is a caller size times 64 divided by that end's depth, so the glow narrows
/// with distance.
typedef struct {
    s32 otz0;    // Ordering-table depth of the first point, and the divisor for its radius
    s32 otz1;    // Ordering-table depth of the second point, and the divisor for its radius
    s32 radius0; // On-screen radius at the first point, in pixels
    s32 radius1; // On-screen radius at the second point, in pixels
    u16 sx0;     // Raw projected X of the first point; first half of its screen-XY word
    u16 sy0;     // Raw projected Y of the first point; second half of that word
    u16 sx1;     // Raw projected X of the second point; first half of its screen-XY word
    u16 sy1;     // Raw projected Y of the second point; second half of that word
} GlowPointPairScratch;
STATIC_ASSERT_SIZEOF(GlowPointPairScratch, 0x18);

/// Scratch-stack block for a glow drawn between two points of a coordinate's
/// local space.
///
/// The drawer rotates each point by the coordinate's `workm`, adds that
/// matrix's translation, and keeps the resulting world points here because
/// they are the input to the projection that follows. Both then go through
/// `GsWSMATRIX`, one perspective transform each; every transform writes that
/// end's screen position as one screen-XY word, so its two halves stay
/// adjacent, and then its ordering-table depth. No GTE flag word is kept: the
/// drawer rejects the pair by the second end's depth alone. The radius at
/// each end is a caller size times 64 divided by that end's depth, so the
/// glow narrows with distance.
///
/// `GlowPointPairScratch` holds the same depths, radii and screen positions
/// for a drawer whose two points are already in world space, and so has no
/// world points. Reserve one complete block and release it in scratch-stack
/// order after drawing.
typedef struct {
    s32     otz0;        // Ordering-table depth of the first point, and the divisor for its radius
    s32     otz1;        // Ordering-table depth of the second point, and the divisor for its radius
    s32     radius0;     // On-screen radius at the first point, in pixels
    s32     radius1;     // On-screen radius at the second point, in pixels
    SVECTOR worldPoint0; // First point in world space, each component narrowed to s16; projection input
    SVECTOR worldPoint1; // Second point in world space, each component narrowed to s16; projection input
    u16     sx0;         // Raw projected X of the first point; first half of its screen-XY word
    u16     sy0;         // Raw projected Y of the first point; second half of that word
    u16     sx1;         // Raw projected X of the second point; first half of its screen-XY word
    u16     sy1;         // Raw projected Y of the second point; second half of that word
} GlowWorldPointPairScratch;
STATIC_ASSERT_SIZEOF(GlowWorldPointPairScratch, 0x28);

/// Scratch-stack workspace for one eight-wedge room-effect disc.
///
/// The drawer copies a coordinate's world translation into `worldPoint`,
/// narrowed to signed 16-bit coordinate units, and projects that point through
/// `GsWSMATRIX`. One perspective transform supplies the screen centre, the GTE
/// flag word and the SZ3 / 4 depth. A negative flag word rejects the
/// projection. Otherwise the depth is incremented by one and used both as the
/// divisor that scales the disc onto the screen and as the ordering-table
/// depth of every wedge.
///
/// `screenX` and `screenY` keep the raw 16-bit encodings of the signed GTE
/// pixel coordinates. They are adjacent so one screen-XY store fills both.
/// `radius` is the disc radius in pixels, `size * 64 / depth`.
///
/// The radius word sits ahead of the flag word. `EffectCentreScratch` holds
/// the same projected centre with those two words exchanged, so the flying
/// disc does not use this record. Reserve one complete block and release it
/// before any pointer into it is used again.
typedef struct {
    SVECTOR worldPoint;      // World position at projection, each component narrowed to s16
    s32     depth;           // SZ3 / 4 plus one; divisor for the radius and ordering-table depth
    s32     radius;          // Disc radius in pixels, size * 64 / depth
    s32     projectionFlags; // GTE FLAG word; bit 31 set rejects the projection
    u16     screenX;         // Raw projected centre X; first half of the GTE screen-position word
    u16     screenY;         // Raw projected centre Y; second half of the same GTE word
} RoomFxFanScratch;
STATIC_ASSERT_SIZEOF(RoomFxFanScratch, 0x18);

/// Scratch-stack workspace for a radial room effect about one world point.
///
/// The drawer copies a coordinate's world translation into `worldPoint`,
/// narrowed to signed 16-bit coordinate units, and projects that point through
/// `GsWSMATRIX`. One perspective transform supplies the screen centre, the GTE
/// flag word and the SZ3 / 4 depth. A negative flag word rejects the
/// projection. Otherwise the depth is incremented by one and used both as the
/// divisor that scales the two radii onto the screen and as the ordering-table
/// depth of every primitive.
///
/// `screenX` and `screenY` keep the raw 16-bit encodings of the signed GTE
/// pixel coordinates. They are adjacent so one screen-XY store fills both.
///
/// The two radii are signed pixel distances from the centre. A star reads them
/// as the disc radius (`size * 64 / depth`) and the spike-shoulder radius
/// (`size * 8 / depth`). A ring reads the same words as the black edge and the
/// tinted edge, each `size * 64 / depth`. Reserve one complete block and
/// release it before any pointer into it is used again.
typedef struct {
    SVECTOR worldPoint; // World position at projection, each component narrowed to s16
    s32     depth;      // SZ3 / 4 plus one; divisor for the radii and ordering-table depth
    union {
        struct {
            s32 disc;  // Disc radius in pixels; spikes also reach half and twice this
            s32 spike; // Spike-shoulder radius in pixels, one eighth of the disc scale
        } star;
        struct {
            s32 black; // Black edge radius in pixels
            s32 tint;  // Tinted edge radius in pixels
        } ring;
    } radii;
    s32 projectionFlags; // GTE FLAG word; bit 31 set rejects the projection
    u16 screenX;         // Raw projected centre X; first half of the GTE screen-position word
    u16 screenY;         // Raw projected centre Y; second half of the same GTE word
} RoomFxRadialScratch;
STATIC_ASSERT_SIZEOF(RoomFxRadialScratch, 0x1C);

/// Scratch-stack workspace for one sixteen-quad flash ring.
///
/// The drawer copies a coordinate's world translation into `worldPoint`,
/// narrowed to signed 16-bit coordinate units, and projects that point through
/// `GsWSMATRIX`. One perspective transform supplies the screen centre, the GTE
/// flag word and the SZ3 / 4 depth. A negative flag word rejects the
/// projection. Otherwise the depth is incremented by one and used both as the
/// divisor that scales the two radii onto the screen and as the ordering-table
/// depth of every quad.
///
/// `screenX` and `screenY` keep the raw 16-bit encodings of the signed GTE
/// pixel coordinates. They are adjacent so one screen-XY store fills both.
///
/// The two radii are signed pixel distances from the centre. The black edge is
/// the first size and the tinted edge is the sum of the two sizes; each size
/// is narrowed to 16 bits, multiplied by 64 and divided by the depth. Which
/// edge lies farther from the centre depends on the sign of the second size.
/// Depth, both radii and the flag word sit ahead of the world point.
/// `RoomFxRadialScratch` holds the same words with the world point first, so
/// the flash ring does not use that record. Reserve one complete block and
/// release it before any pointer into it is used again.
typedef struct {
    s32 depth;               // SZ3 / 4 plus one; divisor for the radii and ordering-table depth
    struct {
        s32 black;           // Black edge radius in pixels
        s32 tint;            // Tinted edge radius in pixels
    } radii;
    s32     projectionFlags; // GTE FLAG word; bit 31 set rejects the projection
    SVECTOR worldPoint;      // World position at projection, each component narrowed to s16
    u16     screenX;         // Raw projected centre X; first half of the GTE screen-position word
    u16     screenY;         // Raw projected centre Y; second half of the same GTE word
} RoomFxFlashRingScratch;
STATIC_ASSERT_SIZEOF(RoomFxFlashRingScratch, 0x1C);

/// Scratch-stack workspace for one quad of a twin trail.
///
/// The twin-trail drawer reserves one block and reuses it for each of the
/// seven quads between two rings of eight coordinate frames. The corners are
/// those frames' world translations, narrowed to signed 16-bit coordinate
/// units. Corner 0 is the first trail's newer slot and is projected on its
/// own. Corners 1, 2 and 3 are the second trail's newer slot, the first
/// trail's older slot and the second trail's older slot, projected together.
///
/// `projectionFlags` is the GTE flag word of that three-vertex transform. A
/// negative word skips the quad. Otherwise `depth` is SZ3 / 4 plus one, and
/// it is the quad's ordering-table depth.
///
/// Each `screenX` / `screenY` pair keeps the raw 16-bit encodings of one
/// corner's signed GTE pixel coordinates. The halves of a pair are adjacent
/// so one screen-XY store fills both, and the drawer copies them onto the
/// gouraud quad in corner order.
///
/// The word between the flag and the screen coordinates is never read or
/// written. Its role is unproven. Reserve one complete block and release it
/// before any pointer into it is used again.
typedef struct {
    SVECTOR worldCorners[4]; // World-space quad corners, each component narrowed to s16
    s32     depth;           // SZ3 / 4 plus one; ordering-table depth of the quad
    s32     projectionFlags; // GTE FLAG word of the three-vertex transform; bit 31 set skips the quad
    s32     field_28;        // Role unproven; the drawer never reads or writes this word
    u16     screenX0;        // Raw projected X of corner 0; first half of that corner's GTE screen-position word
    u16     screenY0;        // Raw projected Y of corner 0; second half of the same word
    u16     screenX1;        // Raw projected X of corner 1; same encoding as screenX0
    u16     screenY1;        // Raw projected Y of corner 1; same encoding as screenY0
    u16     screenX2;        // Raw projected X of corner 2; same encoding as screenX0
    u16     screenY2;        // Raw projected Y of corner 2; same encoding as screenY0
    u16     screenX3;        // Raw projected X of corner 3; same encoding as screenX0
    u16     screenY3;        // Raw projected Y of corner 3; same encoding as screenY0
} RoomFxTwinTrailScratch;
STATIC_ASSERT_SIZEOF(RoomFxTwinTrailScratch, 0x3C);

/// One entry of a room's ambience table: the table holds one entry per area and
/// is indexed by `gGameSession->location.loc.view`. A room's ambience task passes
/// `pan` to `SndEvt_EnqueueType6` / `SndEvt_EnqueueTypeA` as the event's pan,
/// and derives the event's attenuation from `vol` - some rooms pass it as is,
/// others halve it first.
typedef struct RoomAmbienceEntry {
    s16 pan;
    s16 pad_2;
    s16 vol;
    s16 pad_6;
} RoomAmbienceEntry;
STATIC_ASSERT_SIZEOF(RoomAmbienceEntry, 0x8);

#endif // INCLUDE_ROOMS_ROOM_COMMON_H
