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

/// 0x20 work block a room's "show a two-line message" task allocates and parks
/// in `Task::work`: a `TextBlockDesc` handed to `Ui_SpawnTextBlock` followed by
/// the two `TextLineNode`s the descriptor's list points at, so one allocation
/// carries both. The room picks which pair of strings to publish from
/// `Task::spawnArg1`.
typedef struct RoomTextBlock {
    /* 0x00 */ TextBlockDesc desc;
    /* 0x0C */ u8            field_C;
    /* 0x0D */ byte          pad_D[3];
    /* 0x10 */ TextLineNode  lines[2];
} RoomTextBlock;
STATIC_ASSERT_SIZEOF(RoomTextBlock, 0x20);

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

/// The scratch block a room's sprite or light-shaft drawer takes from
/// the scratch stack for one projected point: `vec` is the point in world
/// space, pushed through `GsWSMATRIX` with a single `RTPS`; `sx` / `sy` are the
/// projected centre and `otz` its depth. `halfWidth` is a size divided by
/// `otz`, the on-screen half extent the primitive's corners are offset by, so
/// it narrows with distance.
typedef struct {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     halfWidth;
    /* 0x08 */ SVECTOR vec;
    /* 0x10 */ u16     sx;
    /* 0x12 */ u16     sy;
} RoomShaftScratch;
STATIC_ASSERT_SIZEOF(RoomShaftScratch, 0x14);

/// Overlay of `Task::spawnArg1` for that task: `phase` steps the shaft's
/// pulsing red channel off the global frame counter, `height` is the length
/// the two halves are drawn at before the `1 / otz` divide.
typedef struct _RoomShaftArg {
    /* 0x0 */ u8   phase;
    /* 0x1 */ u8   height;
    /* 0x2 */ byte pad_2[2];
} RoomShaftArg;
STATIC_ASSERT_SIZEOF(RoomShaftArg, 0x4);

/// The scratch block a room's quad drawer takes from the scratch stack to
/// build one quad in: `v` holds its four corners once they are placed in
/// world space, and `otz` the ordering-table depth of their projection, which
/// picks the bucket the primitive is linked into and lets the drawer reject a
/// quad too close to the camera.
typedef struct _RoomQuadScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR v[4];
} RoomQuadScratch;
STATIC_ASSERT_SIZEOF(RoomQuadScratch, 0x24);

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

/// 0xC-byte scratch block `Room_Draw20`, `Room_Draw25`, `Room_Draw29` and
/// `Room_Draw30` take from the scratch stack. `otz` is the `gte_stszotz` of
/// `arg0` through `gGfxViewCoord.workm`; `sx`/`sy` are that screen point and
/// `radius` is `(s16)arg2 * 39 / otz` for `Room_Draw20` or `(s16)arg1 * 64 /
/// otz` for the gouraud discs, the on-screen half-extent of the primitive.
typedef struct _RoomDraw25Scratch {
    /* 0x00 */ s32 otz;
    /* 0x04 */ s32 radius;
    /* 0x08 */ u16 sx;
    /* 0x0A */ u16 sy;
} RoomDraw25Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw25Scratch, 0xC);

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

/// 0x14-byte scratch block `Room_Draw05` takes from the scratch stack. Same
/// projection as `GlowCentreScratch` (`arg0` through `gGfxViewCoord.workm`, one
/// `RTPS`) plus a second radius: `rOuter` is `(s16)arg2 * 64 / otz` and
/// `rInner` is `(s16)arg2 * 8 / otz`. `flag` is `gte_stflg` and `sx`/`sy` are
/// the projected centre.
typedef struct _RoomDraw05Scratch {
    /* 0x00 */ s32 otz;
    /* 0x04 */ s32 flag;
    /* 0x08 */ s32 rOuter;
    /* 0x0C */ s32 rInner;
    /* 0x10 */ u16 sx;
    /* 0x12 */ u16 sy;
} RoomDraw05Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw05Scratch, 0x14);

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

/// 0x18-byte scratch block `Room_Draw11`, `Room_Draw12`, `Room_Draw33` and `Room_Draw34` take
/// from the scratch stack. Two `SVECTOR`s (`arg0` and `arg0 + 1`) are projected
/// through `gGfxViewCoord.workm`.
/// `otz0`/`otz1` are the `gte_stszotz` of those points, `r0`/`r1` are
/// `(s16)arg1 * 64 / otz`, and `sx0`/`sy0` plus `sx1`/`sy1` are the two
/// `gte_stsxy` centres.
typedef struct _RoomDraw11Scratch {
    /* 0x00 */ s32 otz0;
    /* 0x04 */ s32 otz1;
    /* 0x08 */ s32 r0;
    /* 0x0C */ s32 r1;
    /* 0x10 */ u16 sx0;
    /* 0x12 */ u16 sy0;
    /* 0x14 */ u16 sx1;
    /* 0x16 */ u16 sy1;
} RoomDraw11Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw11Scratch, 0x18);

/// The scratch block a room's fan drawer takes from the scratch stack for the
/// fan's centre: `vec` is the centre, pushed through `GsWSMATRIX` with one
/// `RTPS` into `sx` / `sy`, `otz` and `flag`. `radius` is a size divided by
/// `otz + 1`, the on-screen length of the `POLY_G4` wedges fanned about it.
typedef struct {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     radius;
    /* 0x10 */ s32     flag;
    /* 0x14 */ u16     sx;
    /* 0x16 */ u16     sy;
} RoomFanScratch;
STATIC_ASSERT_SIZEOF(RoomFanScratch, 0x18);

/// The scratch block a room's billboard drawer takes from the scratch stack:
/// `vec` is the billboard's world position, pushed through `GsWSMATRIX` with
/// one `RTPS` into `sx` / `sy`, `otz` and `flag`. `rOuter` and `rInner` are
/// sizes divided by `otz + 1`, the on-screen radii of the outer and inner
/// rings of `POLY_G4` wedges it is drawn with.
typedef struct {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     rOuter;
    /* 0x10 */ s32     rInner;
    /* 0x14 */ s32     flag;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} RoomBillboardScratch;
STATIC_ASSERT_SIZEOF(RoomBillboardScratch, 0x1C);

/// 0x28-byte scratch block `Room_Draw24` takes from the scratch stack. `vec0`
/// and `vec1` are the beam's two endpoints, rotated out of the caller's local
/// space by the coordinate's `workm` and offset by its translation; `sx0`/`sy0`
/// and `sx1`/`sy1` are those two points projected through `GsWSMATRIX`, with
/// `otz0` / `otz1` their `gte_stszotz`. `r0` and `r1` are the matching screen
/// radii, `(s16)arg3 * 64` divided by each `otz`, so the beam narrows with
/// distance.
typedef struct _RoomDraw24Scratch {
    /* 0x00 */ s32     otz0;
    /* 0x04 */ s32     otz1;
    /* 0x08 */ s32     r0;
    /* 0x0C */ s32     r1;
    /* 0x10 */ SVECTOR vec0;
    /* 0x18 */ SVECTOR vec1;
    /* 0x20 */ u16     sx0;
    /* 0x22 */ u16     sy0;
    /* 0x24 */ u16     sx1;
    /* 0x26 */ u16     sy1;
} RoomDraw24Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw24Scratch, 0x28);

/// 0x1C-byte scratch block `Room_Draw02` takes from the scratch stack. Same
/// projection and two-radius ring as `RoomBillboardScratch`, but `otz` sits at
/// 0x0 with `rOuter` at 0x4, `rInner` at 0x8, `flag` at 0xC and `vec` at 0x10.
/// `rOuter` is `(s16)arg1 * 64 / (otz + 1)` and `rInner` is
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`.
typedef struct _RoomDraw02Scratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     rOuter;
    /* 0x08 */ s32     rInner;
    /* 0x0C */ s32     flag;
    /* 0x10 */ SVECTOR vec;
    /* 0x18 */ u16     sx;
    /* 0x1A */ u16     sy;
} RoomDraw02Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw02Scratch, 0x1C);

/// 0x3C-byte scratch block `Room_Draw03` takes from the scratch stack. `v` is
/// the quad's four corners, copied from `workm.t` of two adjacent slots on
/// each trail. `flag` is `gte_stflg` (negative rejects the quad) and `otz` is
/// `gte_stszotz` (then incremented). `sx0`..`sy3` are the `gte_stsxy` /
/// `gte_stsxy3` of the four corners, copied onto the `POLY_G4` after it is
/// allocated.
typedef struct _RoomDraw03Scratch {
    /* 0x00 */ SVECTOR v[4];
    /* 0x20 */ s32     otz;
    /* 0x24 */ s32     flag;
    /* 0x28 */ s32     unused;
    /* 0x2C */ u16     sx0;
    /* 0x2E */ u16     sy0;
    /* 0x30 */ u16     sx1;
    /* 0x32 */ u16     sy1;
    /* 0x34 */ u16     sx2;
    /* 0x36 */ u16     sy2;
    /* 0x38 */ u16     sx3;
    /* 0x3A */ u16     sy3;
} RoomDraw03Scratch;
STATIC_ASSERT_SIZEOF(RoomDraw03Scratch, 0x3C);

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
