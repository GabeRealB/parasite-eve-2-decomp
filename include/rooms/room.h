#ifndef INCLUDE_ROOMS_ROOM_H
#define INCLUDE_ROOMS_ROOM_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"

/// Script record for a room's staged event.
///
/// The message handler fills the record on the stack. The event is eligible
/// when `flagId` is 0 or the game-flag nibble it names is 0. Accepting an
/// executing message copies the record into the room's latched event, copies
/// the message into the latched destination, sets a non-zero `flagId` nibble
/// to 1 and spawns the staged event task. A query leaves that state alone.
/// The task runs `capCmd`, starts the screen fade when `fade` asks for it,
/// plays `stageSnd` and waits for that voice, then commits the latched
/// message's area, warp and room. `flagId` is the handler's latch key.
typedef struct {
    s32 capCmd;   // CAP command index the event task runs first
    s32 stageSnd; // Stage sound id played after that command; 0 skips it. A set stage nibble is replaced with the current stage
    s16 flagId;   // Game-flag nibble set to 1 on latch; 0 records nothing and stays eligible
    u8  fade;     // (0 skip, nonzero start fade task 0x31: subtract blend, running phase, 30-frame ramp; return not requested)
} RoomLatchedEvent;
STATIC_ASSERT_SIZEOF(RoomLatchedEvent, 0xC);

/// What a room's cutscene runner plays: the record a room hands the runner
/// task as `Task::spawnArg2`. The runner forces the scene's view into the save
/// location, loads the capture file and starts the capture slot with the
/// scene's sound task beside it, lets the player cut it short, and restores
/// everything when it ends.
typedef struct RoomCutsceneRec {
    s8  field_0;  // Positive: the view forced into the save location for the scene; otherwise its negation is the view restored after
    s8  field_1;  // Capture slot the scene starts, which also picks the command run after it
    s8  field_2;  // Non-zero skips straight to the abort path
    s8  field_3;  // Capture file to load first; 0 for none
    s32 field_4;  // Sound event at the start
    s32 field_8;  // Sound event at the end
    s32 field_C;  // Sound event after a scene that was not skipped
    s32 field_10; // Sound the scene's sound task plays, and that task's spawn argument
    s16 field_14; // First of the pair handed on once the capture file is loaded; 0 selects 0x3C0 with a second of 0
    s16 field_16; // Second of that pair
} RoomCutsceneRec;
STATIC_ASSERT_SIZEOF(RoomCutsceneRec, 0x18);

/// One row of a shop's price ladder, a table of thirteen in the room's data.
/// The row's three items join the shop's stock once the row's bit is set in
/// `Mc_SaveData[0].state.shopTiers`. The rooms read only `items`; the leading word grows
/// row by row up to `S32_MAX` in the last, which reads as the spend that
/// unlocks the row, but nothing here confirms it.
typedef struct RoomShopTier {
    s32  spendThreshold;
    s16  items[3];
    byte pad_A[0x2];
} RoomShopTier;
STATIC_ASSERT_SIZEOF(RoomShopTier, 0xC);

/// `facing` value that tells the departure task not to turn the player.
#define ROOM_DEPARTURE_SKIP_FACING (-1)

/// Destination a room stages before spawning its departure task.
///
/// The room writes `stage`, `area`, `warp` and `room`, then passes the last
/// three through the stage's room-variant resolver. The resolver reads `area`
/// and may replace `room`. The task turns the player to `facing` unless it is
/// `ROOM_DEPARTURE_SKIP_FACING` and waits until that turn finishes, plays
/// `sndEvent` unless it is 0 and waits for that voice, then copies the four
/// destination fields into the save location and starts room-change task 0x11,
/// which copies that save into the live session. The four fields are the save
/// location's stage, area, warp and room; they are not a `GameLocationKey`.
typedef struct {
    u8   stage;      // Destination stage copied into the save location
    u8   area;       // Destination area within that stage; the resolver keys off it
    u8   warp;       // Arrival record in the destination area's warp table
    u8   room;       // Room within the destination area; the resolver may replace it
    s16  facing;     // Target yaw, 4096 units per turn, sent to the player as message 0x3EE; -1 sends nothing
    byte pad_6[0x2]; // No recovered access; keeps `sndEvent` 4-byte aligned
    s32  sndEvent;   // Type-6 sound event played before the room change; 0 skips it
} RoomDeparture;
STATIC_ASSERT_SIZEOF(RoomDeparture, 0xC);

/// The scratchpad block a mirror task takes while it rebuilds the reflected
/// coordinate frame in its `RoomMirrorWork`. A floor mirror only needs
/// `viewRow`, the view matrix's second row, which it negates through the GTE.
/// The other mirrors reflect through a plane: `normal` is the plane's unit
/// normal, `refAxis` the coordinate axis least aligned with it (picked through
/// `leastAbs`, `leastAxis` and `axisAbs`), `basis` the orthonormal frame built
/// from the two and `reflect` the reflection matrix derived from it. `offset`
/// is the plane's position relative to the view, rotated in place into the
/// reflected frame.
typedef struct RoomMirrorPlaneScratch {
    SVECTOR viewRow;
    SVECTOR refAxis;
    byte    unknown_10[8];
    MATRIX  basis;
    MATRIX  reflect;
    SVECTOR normal;
    SVECTOR offset;
    s16     leastAbs;
    s16     leastAxis;
    s16     axisAbs;
    byte    unknown_6E[2];
} RoomMirrorPlaneScratch;
STATIC_ASSERT_SIZEOF(RoomMirrorPlaneScratch, 0x70);

/// The scratchpad block a mirror task takes to find where the reflection
/// lands on screen. It projects two points above and below one of the
/// reflected model's parts through `pos`: `sxyHead` and `otzHead` for the
/// upper point, `sxyFoot` and `otzFoot` for the lower. `left` to `bottom` is
/// the screen rectangle the reflection quads cover, and `texX` the x of the
/// texture page they sample the off-screen copy of the frame from.
typedef struct RoomMirrorExtentScratch {
    SVECTOR pos;
    s32     dp;
    s32     flag;
    s32     otzFoot;
    s32     otzHead;
    DVECTOR sxyFoot;
    DVECTOR sxyHead;
    u16     texX;
    s32     left;
    s32     right;
    s32     top;
    s32     bottom;
} RoomMirrorExtentScratch;
STATIC_ASSERT_SIZEOF(RoomMirrorExtentScratch, 0x34);

/// Per-surface values a room's water drawer keeps in a block it takes from
/// the scratchpad stack rather than in registers: the surface height `y`, the
/// spacing `dx` and `dz` between vertices along X and Z, the height `wave`
/// adds to the vertex being placed, and the corner `x`, `z` of the surface
/// being drawn.
typedef struct RoomWaterScratch {
    s16 y;
    s16 dx;
    s16 wave;
    s16 dz;
    s16 x;
    s16 z;
} RoomWaterScratch;
STATIC_ASSERT_SIZEOF(RoomWaterScratch, 0xC);

/// One water surface in the list a room's water drawer walks: a rectangle at
/// (`x`, `z`) spanning `width` along X and `depth` along Z. A drawer that cuts
/// surfaces into a varying number of quads takes that number from `count`;
/// every list ends at an entry whose `count` is -1.
typedef struct RoomWaterSurface {
    s16 x;
    s16 z;
    s16 width;
    s16 depth;
    s32 count;
} RoomWaterSurface;
STATIC_ASSERT_SIZEOF(RoomWaterSurface, 0xC);

/// The spawn argument of a room model task whose visibility follows a 2-bit
/// game flag: the task hides its model while the flag `flagId` names reads 2.
/// Nothing else of the record is read.
typedef struct RoomFlagModelArg {
    u8 unk0[8];
    u8 flagId;
} RoomFlagModelArg;

/// The work block a room's streamed-scene task allocates at `Task::work`. The
/// task walks the translation of `mtx` along the scene's path table once per
/// streamed frame, addresses its messages to `target`, the task in pointer
/// slot 3, and reparents itself under `script`, the scene's script task.
/// `child` is a task it spawns on the way (a skip or prompt task) and polls
/// with `Task_PollKill`; `spawned` says that it exists, since the block starts
/// out zeroed.
typedef struct RoomStreamWork {
    MATRIX* mtx;
    Task*   target;
    Task*   child;
    Task*   script;
    u16     spawned;
    byte    pad_12[0x2];
} RoomStreamWork;
STATIC_ASSERT_SIZEOF(RoomStreamWork, 0x14);

/// The scratch block a room's mote or mist-puff drawer takes from
/// the scratch stack for one projection: `vec` is the point's world position,
/// projected with a single `RTPS` through `GsWSMATRIX`, and `otz` the depth
/// the resulting tile is linked into the ordering table at; a depth below
/// 0x11 drops it.
typedef struct RoomMoteScratch {
    s32     otz;
    SVECTOR vec;
} RoomMoteScratch;
STATIC_ASSERT_SIZEOF(RoomMoteScratch, 0xC);

/// The scratch block a room's beam drawer takes from the scratch stack: the
/// beam's base in world space and the tip offset from it, and both points'
/// projections - `otz0`, `sx0` and `sy0` for `base`, `otz1`, `sx1` and `sy1`
/// for `tip`. `r0` and `r1` are the wedge radii at each end.
typedef struct RoomBeamScratch {
    SVECTOR base;
    SVECTOR tip;
    s32     otz0;
    s32     otz1;
    s32     flag;
    s32     r0;
    s32     r1;
    u16     sx0;
    u16     sy0;
    u16     sx1;
    u16     sy1;
} RoomBeamScratch;
STATIC_ASSERT_SIZEOF(RoomBeamScratch, 0x2C);

/// The scratch block a room's glow-sprite drawer takes from the scratch stack:
/// `pos` is the task coordinate's translation, projected through `GsWSMATRIX`
/// into `sxy`; `otz` is the resulting depth and `half` the half extent the
/// camera-facing quad is drawn at, divided by `otz` so the sprite shrinks with
/// distance.
typedef struct RoomGlowSpriteScratch {
    s32     otz;
    s32     half;
    SVECTOR pos;
    DVECTOR sxy;
} RoomGlowSpriteScratch;
STATIC_ASSERT_SIZEOF(RoomGlowSpriteScratch, 0x14);

/// The scratch block a room's disc drawer takes from the scratch stack: the
/// depth of the projected centre, the two on-screen radii derived from it, the
/// GTE flag word and the projected centre.
typedef struct RoomDiscScratch {
    s32 otz;
    s32 rOuter;
    s32 rInner;
    s32 flag;
    u16 sx;
    u16 sy;
} RoomDiscScratch;
STATIC_ASSERT_SIZEOF(RoomDiscScratch, 0x14);

/// The scratch block a room's quad drawer projects one quad in when it keeps
/// the screen corners: the four corners in world space, the GTE depth and
/// flag of their projection (a negative flag rejects the quad), and the
/// projected corners, copied onto the primitive once one is allocated.
typedef struct RoomQuadProjScratch {
    SVECTOR v[4];
    s32     otz;
    s32     flag;
    DVECTOR sxy[4];
} RoomQuadProjScratch;
STATIC_ASSERT_SIZEOF(RoomQuadProjScratch, 0x38);

/// The scratch block a room's light-shaft drawer takes from the scratch stack
/// for one shaft: the depth of its projection and its four corners in world
/// space - the two roots, then the tip reached from each.
typedef struct RoomLightShaftScratch {
    s32     otz;
    SVECTOR rootA;
    SVECTOR rootB;
    SVECTOR tipA;
    SVECTOR tipB;
} RoomLightShaftScratch;
STATIC_ASSERT_SIZEOF(RoomLightShaftScratch, 0x24);

/// One row of a halo effect's shade table, picked by the effect's palette
/// selector: each field is the right shift applied to the effect's fade level
/// to get that colour channel, so a row sets the tint of the halo.
typedef struct RoomHaloShade {
    s16 r;
    s16 g;
    s16 b;
} RoomHaloShade;
STATIC_ASSERT_SIZEOF(RoomHaloShade, 0x6);

/// One row of a ring effect's per-band table, added to the effect work's ring
/// parameters: `rInner` widens the ring drawn at the origin height, `yOff`
/// raises the second ring, and `rExtra` widens the second ring beyond the
/// first.
typedef struct RoomRingShape {
    s16 rInner;
    s16 yOff;
    s16 rExtra;
} RoomRingShape;
STATIC_ASSERT_SIZEOF(RoomRingShape, 0x6);

#endif // INCLUDE_ROOMS_ROOM_H
