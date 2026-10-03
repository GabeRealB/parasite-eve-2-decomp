#ifndef INCLUDE_ROOMS_ROOM_H
#define INCLUDE_ROOMS_ROOM_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"

#include "overlay.h"

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
/// location, loads the CAP file and starts the CAP slot with the scene's sound
/// task beside it, lets the player cut it short, runs the follow-up CAP
/// command, and restores everything when it ends. The room owns the record and
/// must leave it untouched until the runner has finished.
typedef struct {
    s8  view;            // Positive: view forced for the scene, the previous one restored after; otherwise the negation is the view set when it ends
    s8  capSlot;         // CAP slot the scene starts, and the CAP command run after it (slot 1 runs command 0x10 + the story dialogue index)
    s8  skipScene;       // Nonzero: start no scene and play no `afterSceneSound`; only load, run the follow-up command and restore
    s8  capFile;         // CAP file to load before the scene (0 keeps the current one; nonzero also resets the CAP state at the end)
    s32 startSound;      // Sound event queued when the runner starts (0 none)
    s32 endSound;        // Sound event queued when the runner finishes
    s32 afterSceneSound; // Sound event queued after a started scene, whether it ran out or the player skipped it
    s32 sceneSound;      // Sound event the scene's sound task plays; stopped when the player skips the scene
    s16 capTPageX;       // VRAM x of the texture page the loaded CAP file draws from (0 selects 0x3C0, with y 0)
    s16 capTPageY;       // VRAM y of that texture page; used only with a nonzero `capTPageX`
} RoomCutsceneRec;
STATIC_ASSERT_SIZEOF(RoomCutsceneRec, 0x18);

/// A room's cutscene record when that symbol is 32 bytes.
///
/// `rec` is the record the room fills and hands the cutscene runner. The
/// eight bytes after it are zero in all four rooms with this extent and have
/// no recovered access. Their role is unproven: a room whose image ends at
/// its record stores exactly `RoomCutsceneRec`, so they are not established
/// as fields of the record.
typedef struct {
    RoomCutsceneRec rec;        // Record handed to the cutscene runner as its spawn argument
    u8              unknown[8]; // Role unproven; zero, with no recovered access
} RoomCutsceneRecStorage;
STATIC_ASSERT_SIZEOF(RoomCutsceneRecStorage, 0x20);

/// A room's `gRoomCutsceneSoundTask` when that symbol is eight bytes.
///
/// `task` is the sound task the cutscene runner starts beside the scene,
/// polls and kills on a skip. The four bytes after it are zero in both rooms
/// with this extent. Their role is unproven.
typedef struct {
    Task* task;       // Sound task started beside the scene; NULL while none runs
    u8    unknown[4]; // Role unproven; zero, with no recovered access
} RoomCutsceneSoundTaskStorage;
STATIC_ASSERT_SIZEOF(RoomCutsceneSoundTaskStorage, 8);

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

/// Sixteen-byte storage for a room's latched staged event.
///
/// Accepting a message replaces only `event`. The staged task reads it to
/// run the CAP command, optional fade and stage sound before changing rooms.
/// The four trailing bytes are initially zero and are not part of that copy;
/// their role is unproven.
typedef struct {
    RoomLatchedEvent event;      // Latched event; copied as a complete twelve-byte record
    u8               unknown[4]; // Uninterpreted trailing bytes; initially zero, role unproven
} RoomLatchedEventStorage;
STATIC_ASSERT_SIZEOF(RoomLatchedEventStorage, 0x10);

/// World-coordinate working values for constructing a water surface's quad strips.
///
/// All values are signed world units. A drawer reserves one block on the
/// scratchpad stack for its call, reuses it for each surface, then releases it.
/// The subdivided axis stores its per-segment step; the other axis stores the
/// span across the strip. Values are narrowed to signed halfwords before
/// vertex arithmetic. `yOffset` is overwritten for each displaced vertex.
typedef struct {
    s16 y;       // Undisplaced surface height
    s16 dx;      // X span of one quad (segment step or span across the strip)
    s16 yOffset; // Added to y for the current vertex (0 flat, signed sine displacement for waves)
    s16 dz;      // Z span of one quad (segment step or span across the strip)
    s16 x;       // Surface's starting X, including any drawer-specific offset
    s16 z;       // Surface's starting Z
} WaterQuadScratch;
STATIC_ASSERT_SIZEOF(WaterQuadScratch, 0xC);

/// End marker in a water-surface descriptor table's signed count or list-marker field.
///
/// Each table must include a final descriptor with this value within its bounds.
/// Walkers stop before reading that descriptor's geometry or using its count
/// for subdivision. The marker fits both the 32-bit `RoomWaterSurface::segmentCount`
/// and the signed 16-bit marker used by alternate surface formats. Fixed strip
/// drawers accept a count of 0 for drawable entries.
enum { WATER_SURFACE_LIST_END = -1 };

/// A rectangular water patch in world coordinates, with height supplied by its drawer.
///
/// Tables include a final entry with `segmentCount == WATER_SURFACE_LIST_END`;
/// the other fields of that entry are not read. Variable strip drawers require
/// a positive `segmentCount` and divide either `width` or `depth` by it using
/// integer division. The drawer selects the subdivision axis. Fixed strip
/// drawers ignore the count except for the terminator; their entries store 0.
typedef struct {
    s16 x;            // Starting X in world units, before any drawer-specific offset
    s16 z;            // Starting Z in world units
    s16 width;        // Extent along +X in world units
    s16 depth;        // Extent along +Z in world units
    s32 segmentCount; // Subdivisions (>0 variable, 0 unused in fixed drawers, -1 list end)
} RoomWaterSurface;
STATIC_ASSERT_SIZEOF(RoomWaterSurface, 0xC);

/// A rectangular water patch in world coordinates that carries no subdivision count.
///
/// The compact form of `RoomWaterSurface`: the same rectangle, with height
/// supplied by its drawer, followed by a signed halfword that only marks the
/// end of the table. Each drawer subdivides `width` and `depth` by its own
/// constants, using integer division. Tables include a final entry with
/// `listMarker == WATER_SURFACE_LIST_END`; the other fields of that entry are
/// not read.
typedef struct {
    s16 x;          // Starting X in world units
    s16 z;          // Starting Z in world units
    s16 width;      // Extent along +X in world units
    s16 depth;      // Extent along +Z in world units
    s16 listMarker; // List terminator (0 drawable entry, -1 list end)
} RoomCompactWaterSurface;
STATIC_ASSERT_SIZEOF(RoomCompactWaterSurface, 0xA);

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

/// Scratch-stack block of a room's own glow-beam drawer: the beam's two end
/// points in world space, followed by the block their projection fills.
///
/// These drawers queue the same capsule of gouraud wedges as the shared
/// two-point glow drawers, but work both ends out themselves from effect
/// coordinates, so the points live in the block rather than arriving as
/// arguments. `point0` is projected first and fills the `0` half of `pair`;
/// `point1` fills the `1` half. A drawer that places the second end relative
/// to the first rotates that offset in place in `point1` before adding
/// `point0` to it.
typedef struct {
    SVECTOR                 point0; // World position of the first end
    SVECTOR                 point1; // World position of the second end; holds its local offset while that is rotated
    OverlayPointPairScratch pair;   // Both ends' screen positions and depths, the GTE flag word and the on-screen radii
} RoomBeamScratch;
STATIC_ASSERT_SIZEOF(RoomBeamScratch, 0x2C);

/// Scratch block a room's two-radius disc drawer takes from the scratch stack.
///
/// One perspective transform of a world point through `gGfxViewCoord.workm`
/// writes the screen position and the GTE flag word. A negative flag word
/// means the transform reported an error, and the drawer links nothing.
/// Otherwise it stores the ordering-table depth and two on-screen radii, a
/// caller size divided by that depth. The outer radius spans the wedge disc,
/// and the tips of the cross drawn over it sit at that radius and at twice it.
/// The inner radius is one eighth of the outer scale and places the shoulders
/// either side of each ray. The flag word follows the radii.
/// `GlowCentreRadiiScratch` keeps the same words with the flag between the
/// depth and the radii, so the records stay separate. `sx` and `sy` are
/// written by one screen-XY store, so they stay adjacent.
///
/// Reserve the complete block and release it in scratch-stack order after
/// drawing.
typedef struct {
    s32 otz;         // Ordering-table depth of the centre; also the divisor for both radii
    s32 outerRadius; // On-screen radius of the disc; cross-ray tips sit at this and at twice it
    s32 innerRadius; // On-screen distance from the centre to a cross ray's shoulders
    s32 flag;        // GTE flag word; negative means the transform reported an error
    u16 sx;          // Projected centre, x; first half of the screen-XY word
    u16 sy;          // Projected centre, y; second half of that word
} RoomDiscScratch;
STATIC_ASSERT_SIZEOF(RoomDiscScratch, 0x14);

#endif // INCLUDE_ROOMS_ROOM_H
