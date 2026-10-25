#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_FACTORY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_FACTORY_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Eight nighttime factory recipes, indexed 0..7 with no terminator.
///
/// Slots are power, lamp, CAP, barrier transition, lift model, barrier collision,
/// hatch scene and hatch model. All use priority `TASK_PRIORITY_CONTROL` and zero unused metadata.
/// Both factory overlays select this table outside daytime Dryfield. Spawning
/// reads recipes synchronously; keep the night factory callbacks and borrowed
/// lift/hatch model resources loaded through task teardown.
extern TaskDesc gDryfieldNightFactoryTaskDescs[8];

/// Two-lane controller-vibration commands for a jammed nighttime factory lift turn.
///
/// The binary motor plays segment 2 then stops; the variable motor plays
/// segments 0 and 1 then stops. Both factory instances select this script outside
/// daytime Dryfield and borrow it until script teardown. Keep the night overlay loaded.
extern PadScriptCmd gDryfieldNightFactoryLiftJoltCommands[3];

/// Timed vibration segments for the nighttime lift's jam jolt.
///
/// Segment 0 holds intensity 255 for eight eligible frames; segment 1 ramps
/// 150 toward 80 over twenty; segment 2 holds the binary motor for five.
/// Intensities are bytes (0..255). The fourth byte is unread metadata with
/// unproven meaning. The script borrows this table through teardown.
extern PadScriptVibrationSegment gDryfieldNightFactoryLiftJoltSegments[3];

/// Bodyless priority-192 recipe for the nighttime factory's operator panel.
///
/// Room setup borrows its address for the selected panel descriptor; spawning
/// reads the single recipe at index zero with unused payload words. There is
/// no terminator. Keep the night factory loaded until the panel exits.
extern TaskDesc gDryfieldNightFactoryPanelTaskDesc;

/// Dryfield night factory placement/resource directory indexed by `GameLocationKey.variant` 0..10.
///
/// All eleven slots are absent (NULL, NULL), so this room publishes no actor
/// placement or resource lists. The indexed slot itself is still returned by
/// the area lookup. Keep the room overlay loaded; there is no bounds check.
extern AreaVariant gDryfieldNightFactoryAreaVariants[11];

/// Logical-to-mapped view directories for night factory room selectors 1 and 2.
///
/// Both borrow `gViewIdentityMap`: logical views 1..19 select the same 1-based
/// camera, image and sprite indices. The map overlay borrows this two-pointer
/// directory while the night factory overlay remains loaded.
extern u8* gDryfieldNightFactoryViewMaps[2];

/// Model-lighting resources for night factory room selectors 1 and 2.
///
/// Both borrow the same point-light collection and minimum ambient colours
/// for logical views 1..19. The records and their room-owned resources must
/// remain loaded while coordinate setup or lighting queries use them.
extern WorldCoordRoomLighting gDryfieldNightFactoryRoomLighting[2];

/// Live collision mesh shared by the night factory's two room selectors.
///
/// The 2-by-4 XZ grid uses 4000-unit cells and biases (444, 222) in room
/// coordinates. Its 72 faces index 28 normals and 170 vertices; the eight
/// cell lists borrow a 288-halfword pool and end with
/// `WORLD_COLLISION_GRID_CELL_END`. Activation binds the view coordinate.
/// The barrier rewrites normals/faces 0..1 and vertices 0..7; the lift rewrites
/// normals/faces 2..5 and vertices 8..15. The night overlay owns all storage,
/// which must remain loaded while collision queries or factory tasks use it.
extern WorldCollisionGrid gDryfieldNightFactoryCollisionGrid;

/// Collision resources for room selectors 1..2, indexed by room minus one.
///
/// Both room slots borrow the same grid, fourteen view boundaries and nineteen action
/// triggers; neither has sight occluders.
///
/// The stage map borrows this directory while the room overlay is loaded. Keep the grid
/// and its pools live while active, and the mutable trigger/occluder lists live until
/// unlinked. This directory has no terminator or bounds check.
extern WorldCollisionRoomResources gDryfieldNightFactoryRoomCollisionResources[2];

/// Nighttime factory logical-view search counts for rooms 1 and 2.
///
/// Both rooms search nineteen identity-map entries, selecting mapped views 1..19.
/// Indexed by room - 1; signed halfword values count logical map elements,
/// independently of map capacity and mapped resource extents. No terminator.
/// Borrow the array only while this room overlay is loaded.
extern ViewCount gDryfieldNightFactoryViewCounts[2];

/// Warp endpoint records for slots 1..3, indexed by warp minus one.
///
/// Slots 1..3 serve the Driveway, Breezeway and Garage connections, with arrival views
/// 2, 7 and 6. All mark their doors; slot 2 has a blocked sound and slot 3 fades on
/// permitted departure.
///
/// Positions use signed whole world coordinates; yaw uses 4096 units per turn. The two
/// byte spans have unproven roles and remain zero. The stage map borrows this array;
/// keep the room overlay loaded through departure or arrival setup. Consumers copy
/// complete entries and actor spawns borrow their transforms synchronously. There is
/// no terminator or bounds check.
extern DirectionWarpEntry gDryfieldNightFactoryWarpEntries[3];

/// Nineteen cameras shared by the nighttime factory's two room slots.
///
/// Both use mapped views 1..19, selecting records at view minus one. Rotation
/// uses ONE per unit, translation is the negated world-space camera origin
/// and perspective distance is pixels. The camera directory borrows this
/// mutable array while the factory overlay is loaded.
extern ViewCamera gDryfieldNightFactoryViewCameras[19];

/// Dryfield night factory's sprite resources, indexed by mapped view minus one.
///
/// Views 1, 5, 10..14 and 17 have no sources. Views 18 and 19 share
/// one source pool and batch list; both logical rooms use this directory.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gDryfieldNightFactorySpriteViews[19];

/// Surface-class directory for the nighttime factory, indexed by class 0..7.
///
/// Class 1 passes probes and ignores weapon impacts; all others block probes
/// and allow impacts. Every class applies grid pushback. Only classes 2 and 3
/// supply footstep cues. The stage map borrows this directory and its records
/// only while this overlay is loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightFactorySurfacePropertiesByClass[8];

/// Night factory instance of the mapped view 9 power-sprite visibility setter.
///
/// The low byte of `visible` selects hidden (0) or visible (nonzero); higher
/// bits are ignored. This instance also acts only in the daytime Dryfield stage;
/// calls in the night stage leave sprite records unchanged. Requires a live
/// session and, in the daytime stage, the loaded factory sprite tables, sprite
/// variant 1, mapped view 9 and writable batch 1 containing the power sprite.
/// The hidden flag controls subsequent drawing. The loaded overlays own the
/// sprite tables.
void dryfieldNightFactorySetView9SpriteVisible(s32 visible);

/// Night factory instance of the mapped view 11 power-scene sprite visibility setter.
///
/// The low byte of `visible` selects hidden (0) or visible (nonzero); higher
/// bits are ignored. This instance also acts only in the daytime Dryfield stage;
/// calls in the night stage leave sprite records unchanged. Requires a live
/// session and, in the daytime stage, the factory area with its loaded sprite
/// tables, sprite variant 1, mapped view 11 and writable batch 1 containing one sprite.
/// The hidden flag controls subsequent drawing; the loaded map and room
/// overlays own the tables and must remain loaded during the call.
void dryfieldNightFactorySetView11SpriteVisible(s32 visible);

/// Draws the nighttime factory's power and lamp glows for the current logical view.
///
/// Per-frame effect callback for room views 1..19. Power enables one fixed-world
/// glow; lamp progress 1 or 2 selects one of two positions and colours, so at
/// most two glows are queued. Other lamp progress values draw no lamp glow.
/// `task` must have a live coordinate body and writable coordinate parent chain;
/// its cached transform is refreshed before drawing, without changing task state.
/// Requires the composed view, initialized scratch stack and current packet
/// arena and ordering table. Packets live until this frame's GPU work completes.
void dryfieldNightFactoryDrawGlowsTask(Task* task);

/// Dispatches the nighttime factory room task's setup, idle and teardown states.
///
/// Requires a live bodyless task with `Task::state` in 0..2; the three handlers
/// are copied onto the stack and indexed without a bounds check. Setup installs
/// the room's message table, owns a panel-task slot at `Task::work`, and starts
/// the lift and barrier tasks. Idle keeps the room available for messages;
/// teardown releases its work. The factory overlay must remain loaded while
/// the task can run or receive messages.
void dryfieldNightFactoryEntryTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_FACTORY_H
