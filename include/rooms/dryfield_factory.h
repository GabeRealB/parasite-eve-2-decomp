#ifndef INCLUDE_ROOMS_DRYFIELD_FACTORY_H
#define INCLUDE_ROOMS_DRYFIELD_FACTORY_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Collision resources shared by the factory's two daytime rooms.
///
/// Room IDs 1 and 2 select the same live lift grid, view boundaries and action
/// triggers, with no occluders. The map directory borrows this room-indexed
/// array; it has two entries and no terminator. Keep the room overlay loaded
/// until the active grid and both mutable trigger lists are retired.
extern WorldCollisionRoomResources gDryfieldFactoryRoomCollisionResources[2];

/// Logical-view maps for the daytime factory's two room variants.
///
/// Indexed by room minus one, then logical view minus one. Room 1 borrows
/// gameplay's identity map; room 2 remaps logical views 1..19 after the barrier
/// is cleared. The mapped byte selects camera, image and sprite resources.
/// The map overlay borrows this directory while the factory remains loaded.
extern u8* gDryfieldFactoryRoomViewMaps[2];

/// Lighting resources for daytime factory room selectors 1 and 2.
///
/// Both borrow the same point-light collection and per-logical-view ambient
/// minima. Coordinate and lighting lookups index by room minus one; the
/// descriptors and their resources require the factory overlay to stay loaded.
extern WorldCoordRoomLighting gDryfieldFactoryRoomLighting[2];

/// Reverse-view lookup limits for daytime factory room selectors 1 and 2.
///
/// Both search nineteen logical-view bytes. These signed element counts
/// exclude room 2's final stored zero and do not bound mapped resource arrays.
/// The map overlay borrows the two entries while the factory remains loaded.
extern ViewCount gDryfieldFactoryRoomViewCounts[2];

/// Daytime factory endpoints for the driveway, breezeway and garage, in that order.
///
/// Indexed by the one-based warp ID minus one.
/// Entries carry whole world-coordinate actor placements, 4096-unit yaw,
/// arrival/departure/refusal sound IDs, logical arrival views and map markers.
/// The third endpoint requests a departure fade. Records have no terminator;
/// the stage map borrows them while this factory overlay remains loaded.
extern DirectionWarpEntry gDryfieldFactoryWarpEntries[3];

/// Eight indexed daytime factory spawn recipes, without a terminator.
///
/// Slots 0..7 create the power, lamp, barrier-dialogue, barrier-transition,
/// lift-model, barrier-collision, hatch-scene and hatch-model tasks. All run at
/// priority `TASK_PRIORITY_CONTROL`; scene spawns carry CAP commands in spawnArg1, the lift borrows
/// the panel-task slot in spawnArg2, and the hatch borrows its lift parent there.
/// Runtime selection may reach this table from either compiled factory room;
/// its callbacks and borrowed TMD sources require the daytime overlay loaded.
extern TaskDesc gFactoryDaySpawnTable[8];

/// Single priority-192, bodyless operator-panel spawn recipe for the daytime factory.
///
/// Entry 0 starts the seven-state panel dispatcher. Runtime stage selection
/// may borrow it from either compiled room; the daytime factory code must
/// remain loaded while its panel and cursor tasks run. No terminator is stored.
extern TaskDesc gFactoryDayPanelDesc;

/// Number of mapped camera and sprite views in the daytime factory.
enum { DRYFIELD_FACTORY_MAPPED_VIEW_COUNT = 19 };

/// Camera transforms and perspective distances for the daytime factory's mapped views.
///
/// Indexed by mapped view 1..19 minus one, independently of the logical view.
/// Both room variants borrow this array through the daytime map directory.
/// Camera translations use world coordinates; rotations use 4096 for 1.0,
/// and screen distances are pixels. The factory overlay must stay loaded until
/// a queued camera application has copied its selected record.
extern ViewCamera gDryfieldFactoryViewCameras[DRYFIELD_FACTORY_MAPPED_VIEW_COUNT];

/// Sprite resources for the daytime factory's mapped views.
///
/// Indexed by mapped view 1..19 minus one through the daytime map directory.
/// All views preserve the decoded background and have no clipping areas;
/// views 1, 4, 5, 10 and 12 have empty batch lists. Sources and batches are
/// writable room-owned storage; the power scene toggles view 11's batch 1.
/// Keep the map and factory overlays loaded while using these records or
/// their referenced lists, including during cached sprite drawing.
extern SpriteView gDryfieldFactorySpriteViews[DRYFIELD_FACTORY_MAPPED_VIEW_COUNT];

/// Two-lane controller-vibration commands for a jammed daytime factory lift turn.
///
/// The binary motor plays segment 2 then stops. The variable motor plays
/// segments 0 and 1 then stops. Both factory instances select this script in
/// daytime Dryfield; the interpreter borrows it without writing it until the
/// script task ends, so the daytime factory overlay must remain loaded.
extern PadScriptCmd gDryfieldFactoryLiftJoltCommands[3];

/// Timed controller-vibration segments used by the daytime lift's jam jolt.
///
/// Segment 0 holds variable intensity 255 for 8 eligible script frames;
/// segment 1 ramps 150 toward 80 for 20; segment 2 holds the binary motor for
/// 5. Intensities are bytes (0..255); the fourth stored byte is unread by the
/// interpreter and its role is unproven. Borrowed for the jolt task's lifetime.
extern PadScriptVibrationSegment gDryfieldFactoryLiftJoltSegments[3];

/// Collision policies and footstep cues indexed by the daytime factory's surface class (0..7).
///
/// Classes 0 and 4..7 share a solid, silent policy. Class 1 passes probes and
/// disables weapon impacts, while retaining pushback. Classes 2 and 3 are solid
/// and select different footstep cue bases. The map overlay borrows this table;
/// its records remain valid only while the daytime factory overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldFactorySurfaceClasses[8];

/// Live collision mesh shared by the daytime factory's two room variants.
///
/// The 2-by-4 XZ grid uses 4000-unit cells and biases (444, 222) in room
/// coordinates. It owns 72 faces indexing 28 normals and 170 vertices; the
/// eight cell lists borrow a 288-halfword pool and end with
/// `WORLD_COLLISION_GRID_CELL_END`. Activation binds the view coordinate.
/// The barrier rewrites normals/faces 0..1 and vertices 0..7; the lift rewrites
/// normals/faces 2..5 and vertices 8..15. The overlay owns all storage, which
/// must remain loaded while the grid is active or a factory task edits it.
extern WorldCollisionGrid gDryfieldFactoryCollisionGrid;

/// Sets the power-controlled sprite visibility in the daytime factory's mapped view 9.
///
/// The low byte of `visible` selects hidden (0) or visible (nonzero); higher
/// bits are ignored. Only the daytime Dryfield stage changes sprite records.
/// Requires a live session and, in that stage, the loaded factory sprite tables,
/// sprite variant 1, mapped view 9 and writable batch 1. The batch holds one
/// sprite; its hidden flag controls subsequent drawing. The loaded overlays own
/// the sprite tables.
void dryfieldFactorySetView9SpriteVisible(s32 visible);

/// Sets the power-scene sprite visibility in the daytime factory's mapped view 11.
///
/// The low byte of `visible` selects hidden (0) or visible (nonzero); higher
/// bits are ignored. Only the daytime Dryfield stage changes sprite records.
/// Requires a live session and, in that stage, the factory area with its loaded
/// sprite tables, sprite variant 1, mapped view 11 and writable batch 1 containing
/// one sprite.
/// The hidden flag controls subsequent drawing; the loaded map and room
/// overlays own the tables and must remain loaded during the call.
void dryfieldFactorySetView11SpriteVisible(s32 visible);

/// Draws the daytime factory's power and lamp glows for the current logical view.
///
/// Per-frame effect callback for room views 1..19. Power enables one fixed-world
/// glow; lamp progress 1 or 2 selects one of two positions and colours, so at
/// most two glows are queued. Other lamp progress values draw no lamp glow.
/// Requires the composed view, initialized scratch stack and current packet
/// arena and ordering table. Packets live until this frame's GPU work completes.
/// The callback leaves task state unchanged and does not use `task`.
void dryfieldFactoryDrawGlowsTask(Task* task);

/// Dispatches the daytime factory room task's setup, idle and teardown states.
///
/// Requires a live bodyless task with `Task::state` in 0..2; the three handlers
/// are copied onto the stack and indexed without a bounds check. Setup installs
/// the room's message table, owns a panel-task slot at `Task::work`, and starts
/// the lift and barrier tasks. Idle keeps the room available for messages;
/// teardown releases its work. The factory overlay must remain loaded while
/// the task can run or receive messages.
void dryfieldFactoryEntryTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_FACTORY_H
