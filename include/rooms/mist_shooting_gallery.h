#ifndef INCLUDE_ROOMS_MIST_SHOOTING_GALLERY_H
#define INCLUDE_ROOMS_MIST_SHOOTING_GALLERY_H

#include "common.h"

#include "main/tmd_types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/ui_types.h"

/// Number of scored gallery target kinds, indexed 0..12.
///
/// The low spawn-argument nibble indexes `MistShootingGalleryWork::kills` and
/// the RESULT panel's score rows. Civilian kind 9 is included and deducts points;
/// the uncounted demonstration kind 13 is excluded from these arrays.
#define MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT 13

/// Work block of the shooting gallery's controller task, which runs one course
/// from its countdown through its target waves to the result panel.
///
/// The controller allocates it zeroed and publishes its task as
/// `gMistShootingGalleryControllerTask`, through which the targets it spawns and
/// the RESULT panel reach it. Times are in frames at 30 per second. The block
/// lives as long as the controller task.
typedef struct {
    u16 scriptFrame;                                    // Frames into the wave script; records keyed to it spawn, saturating at 0xFFF0
    u16 timeLeft;                                       // Course time remaining; counts down only while combat actors run
    u16 phase;                                          // Step of the course script, then of the closing sequence
    u16 resumePhase;                                    // `phase` to return to after an interrupting caption sequence
    s16 spawnIndex;                                     // Next record of the course's wave script
    s16 timer;                                          // General countdown for the current phase
    s16 clockX;                                         // Screen X of the countdown clock, sliding in from the left
    u8  liveTargets;                                    // Spawned targets still in play; scripts wait for 0 between waves
    u8  kills[MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT]; // Targets destroyed, per target kind
    u8  course;                                         // Course being run (0..4), from the task's spawn argument; selects script, time and scoring
    u8  targetLockMask;                                 // Player-actor slots locking onto the current target (`worldTargetGetActorLockMask`)
    u8  interrupted;                                    // Set once the course's single interruption (out of ammo, abort, lethal hit) has fired
    u8  actionTriggered;                                // Raised when the player uses the gallery's action point; scripts clear it
    u8  captionStep;                                    // Next caption of the course's caption script to show
    u8  resumeCaptionStep;                              // `captionStep` to return to after an interrupting caption sequence
    u8  lethalHit;                                      // Raised by a target attack the player could not survive; ends the course
} MistShootingGalleryWork;
STATIC_ASSERT_SIZEOF(MistShootingGalleryWork, 0x24);

/// Borrowed task of the most recently initialized gallery course controller.
///
/// Published after successful work allocation. Its `work` is a
/// `MistShootingGalleryWork` owned by the task; targets, result panels and score
/// queries require it to remain live. The pointer is not cleared on teardown
/// and must not be used afterwards. The gallery and target overlays stay loaded
/// throughout the course, including its result display.
extern Task* gMistShootingGalleryControllerTask;

/// Number of entries in the gallery's movie-presentation task table.
enum { MIST_SHOOTING_GALLERY_MOVIE_TASK_COUNT = 3 };

/// Post-clear presentation recipes: slot 0 session, slot 1 movie, slot 2 mode splash.
///
/// All three tasks are bodyless at priority 192 and take unused zero spawn
/// arguments. Actor 215100 starts slot 0; it queues the splash after a clear,
/// then the movie through the display task list. There is no terminator.
/// Spawning reads each recipe synchronously; keep the gallery and presentation
/// resources loaded until playback restores the game loop.
extern TaskDesc gMistShootingGalleryMovieTaskDescs[MIST_SHOOTING_GALLERY_MOVIE_TASK_COUNT];

/// Indices in `gMistShootingGalleryTaskDescs`; these are descriptor slots, not states.
enum {
    MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER = 0,
    MIST_SHOOTING_GALLERY_TASK_RED_FLASH         = 1,
    MIST_SHOOTING_GALLERY_TASK_COUNT             = 2,
};

/// Course-controller and red-screen-flash task descriptors for the loaded gallery.
///
/// Both tasks have no body and run at priority 192. The controller takes course
/// 0..4 in the low nibble of its first spawn argument; the flash ignores the
/// incoming arguments and owns no work. Spawning reads a descriptor immediately;
/// its callback code must remain loaded until the task exits.
extern TaskDesc gMistShootingGalleryTaskDescs[MIST_SHOOTING_GALLERY_TASK_COUNT];

/// Number of retained placement-variant slots, including absent slots.
enum { MIST_SHOOTING_GALLERY_AREA_VARIANT_COUNT = 12 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
///
/// Slots 1 are present; every other slot is absent (NULL, NULL).
/// Supply an index in 0..11; this directory has no terminator or bounds check.
/// The placement selector is independent of room and view selectors.
/// Placements belong to the stage map, recipes to this room. Keep both overlays
/// loaded while loading or using either table. Referenced actor overlays must
/// be loaded for spawning and model-policy updates.
extern AreaVariant gMistShootingGalleryAreaVariants[MIST_SHOOTING_GALLERY_AREA_VARIANT_COUNT];

/// RESULT-panel recipe used by gameplay after a gallery course ends.
///
/// Initial bounds (-104, -48, 208, 64) are screen-centered pixels, with OT base
/// 32 and a bodyless priority-192 task. The handler fits the height to nonzero
/// kill-count rows and reads the live gallery controller; it ignores the first
/// spawn argument. Spawning copies this recipe. Keep the gallery overlay and
/// controller work live through the result and any BONUS child panel.
extern UiObjectDesc gMistShootingGalleryResultPanelDesc;

/// Gallery weapon-picker recipe exported to the inventory menu's root session.
///
/// Initial bounds are (-8, 0, 144, 64) screen-centered pixels, with OT base 32
/// and a bodyless priority-192 task. Its handler fits and centers the panel to
/// the owned weapons. Spawning copies the recipe; keep the gallery and callback
/// loaded until the picker and its help children close.
extern UiObjectDesc gMistShootingGalleryWeaponSelectPanelDesc;

/// Extents of the gallery's room directory and 1-based warp endpoint table.
enum {
    MIST_SHOOTING_GALLERY_ROOM_COUNT = 1,
    MIST_SHOOTING_GALLERY_WARP_COUNT = 7,
};

/// Collision resources for gallery room 1, indexed by room ID minus one.
///
/// Borrows the live grid, view boundaries and interaction triggers; no occluder
/// list is supplied. The Acropolis map's gallery area slot borrows this array.
/// Setup binds and links the writable resources, including the LAST-marked
/// trigger records. Keep the gallery loaded until the active lists are cleared.
extern WorldCollisionRoomResources gMistShootingGalleryRoomCollisionResources[MIST_SHOOTING_GALLERY_ROOM_COUNT];

/// Logical-view mapping for the gallery's single room.
///
/// Indexed by the 1-based room ID minus one; only room 1 is valid. Its borrowed
/// `gViewIdentityMap` maps logical views 1..18 to the same camera/image indices.
/// The Acropolis map directory borrows this array while the gallery is loaded;
/// the gameplay-owned byte map has its own larger capacity and no terminator.
extern u8* gMistShootingGalleryViewMaps[1];

/// Logical-view count searched by the gallery's reverse view lookup.
///
/// Indexed by the 1-based room ID minus one; room 1 has 18 views. The retained
/// second element is zero, not a terminator or a second supported room. Counts
/// are signed halfword element counts, independent of the identity map's capacity.
/// The Acropolis map directory borrows this array while the gallery is loaded.
extern ViewCount gMistShootingGalleryViewCounts[2];

/// Light collection and per-view ambient minima for the gallery's single room.
///
/// Indexed by the 1-based room ID minus one; only room 1 is valid. The mutable
/// `lights` pointer initially selects the default collection and may switch to
/// the alternate collection. Both share `ambientTable` for views 1..18.
/// The table and its borrowed data are valid only while the room overlay is loaded.
extern WorldCoordRoomLighting gMistShootingGalleryRoomLightingTable[1];

/// Gallery warp endpoints 1..7, indexed by warp ID minus one without a terminator.
///
/// The Acropolis map's gallery area slot borrows this table while the room is
/// loaded. Player and companion placements coincide; XYZ are room units and
/// yaw uses 4096 per turn. Initial views are 2, 4, 14, 5, 5, 7 and 15.
/// Endpoints 3 and 7 have no sounds; none has blocked sounds, flags or map markers.
/// Entry 7 is the cleared-run carryover arrival. Consumers copy whole records,
/// retaining both byte spans whose roles remain unproven.
extern DirectionWarpEntry gMistShootingGalleryWarpEntries[MIST_SHOOTING_GALLERY_WARP_COUNT];

/// Extent of the gallery's mapped camera and sprite-view resource arrays.
enum { MIST_SHOOTING_GALLERY_MAPPED_VIEW_COUNT = 18 };

/// Cameras for mapped gallery views 1..18, stored at mapped index minus one.
///
/// No terminator. Rotation coefficients have 12 fractional bits; translation
/// is the negated camera origin in world units, and projection distance is in
/// pixels. The Acropolis map directory borrows this array while the gallery is
/// loaded; queued camera application requires the selected record to stay live.
extern ViewCamera gMistShootingGalleryViewCameras[MIST_SHOOTING_GALLERY_MAPPED_VIEW_COUNT];

/// Sprite resources for mapped gallery views 1..18, stored at index minus one.
///
/// No terminator. Each view borrows a `SPRITE_BATCH_END`-terminated list and optional
/// source elements; all draw-area pointers are NULL. Empty views retain their
/// batch address in `sources.empty`, which is never read as source elements.
/// The Acropolis map directory and renderer borrow the writable records and
/// their arrays only while the gallery overlay remains loaded.
extern SpriteView gMistShootingGallerySpriteViews[MIST_SHOOTING_GALLERY_MAPPED_VIEW_COUNT];

/// Collision and footstep properties indexed by the gallery's surface class (0..7).
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Classes 1 and 2 select distinct footstep cue pairs; classes 0 and 3..7 share
/// a record without footstep cues. The table and borrowed records remain valid
/// only while the gallery overlay is loaded.
extern WorldCollisionSurfaceProperties* gMistShootingGallerySurfacePropertiesByClass[8];

/// Restores the training barrier and optionally lowers it out of the firing lane.
///
/// Zero restores its original position; nonzero adds 4000 to Y in room units.
/// Requires the loaded, writable room collision grid. The face uses normal 3
/// and vertices 8..11; the retained offset pass also adds Y to vertices 12..15.
void mistShootingGallerySetTrainingBarrierLowered(s32 lowered);

/// Queues the gallery jukebox display session and returns 1.
///
/// `unused` is ignored. Requires the gallery overlay and serialized display-mode
/// requests; the queued session retains its descriptor while the overlay is loaded.
s32 mistShootingGalleryOpenJukebox(s32 unused);

/// Selects the gallery's default or alternate room-light collection.
///
/// Zero selects the default collection; every nonzero signed halfword selects
/// the alternate collection. Requires the loaded gallery overlay. The shared
/// per-view ambient table stays selected independently.
void mistShootingGallerySelectRoomLights(s16 useAlternate);

/// Spawns an uncounted fixed demonstration target at world XYZ (6000, 0, 3000).
///
/// Requires loaded gallery and actor 107600 overlays and a live scene root.
/// Allocation failure leaves no target. Kind 13 lies beyond the actor's thirteen
/// serialized HP entries; its live HP backing remains unproven.
void mistShootingGallerySpawnDemoTarget(void);

/// Latches the gallery action point for the active course script.
///
/// Requires the published controller and its live initialized work. Repeated
/// signals coalesce into one byte; the course script clears it after handling.
void mistShootingGallerySignalAction(void);

/// Issues the chosen training course's items and level-one Parasite Energy spells.
///
/// `courseLevel` is 1..5. All courses start with Pyrokinesis; 1..2 add no items,
/// 3 adds a GPS, 4 adds a GPS and two attached Colas plus Combustion/Energy Shot,
/// and 5 adds a GPS, three Recovery1s and two MP Boost1s plus those spells.
/// The caller prepares the carried inventory; this routine adds to it and resets
/// only the twelve spell levels. It then restores HP/MP and all weapon supplies.
void mistShootingGalleryPrepareTrainingLoadout(s32 courseLevel);

/// Queues the gallery weapon picker through the inventory menu and returns 1.
///
/// `unused` is ignored. Requires the gallery overlay and serialized display-mode
/// requests. The picker offers the owned entries of the gallery's weapon table.
s32 mistShootingGalleryOpenWeaponMenu(s32 unused);

/// Opens run-mode selection with scaled EXP/BP carryover after a cleared run.
///
/// Called on gallery entry; queues the session only when the live save has a
/// clear and the entry warp is 7. Requires the gallery overlay to remain loaded
/// through the display session.
void mistShootingGalleryOpenCarryoverModeMenu(void);

/// Draws the gallery's fixed light glows for the mapped view each frame.
///
/// Gameplay effect slot 0x19F dispatches this callback. `unused` is ignored;
/// selected world-point pairs draw capsules, and single points draw discs.
/// Other views draw nothing. Radii use the glow drawers' depth-scaled units,
/// and colours are packed RGB nibbles with alternating-frame flicker.
/// Requires this room overlay, the current view transform, initialized scratch
/// storage and sufficient frame packet space; packets live until GPU completion.
void mistShootingGalleryDrawLightGlowsTask(Task* unused);

/// Animates the tracer and blue screen tint of a gallery target's attack.
///
/// Effect-bank slot 0x1BD supplies a coordinate body and an owned `EffectWork`
/// in `task->spawnArg2.pointer`, with a borrowed parent coordinate that must
/// outlive the effect. The endpoint uses the body's cached world translation,
/// narrowed to signed 16-bit coordinates; spin uses 4096 units per turn.
/// Running ticks flicker the sprite and beam and fade the tint by 8 levels;
/// held ticks redraw at a larger size without advancing age or brightness.
/// Completion releases the counted effect work and kills the task.
void mistShootingGalleryTracerTask(Task* task);

/// Dispatches the gallery room task's initialization, running and exit states.
///
/// `task->state` must be 0..2. The room descriptor in the Acropolis map package
/// starts at state 0; room state handlers and their actor dependencies require
/// both gallery and actor 215100 overlays to remain loaded for the task's lifetime.
void mistShootingGalleryRoomTask(Task* task);

extern TmdSource gMistShootingGalleryModel093FC;

extern TmdSource gMistShootingGalleryModel095EC;

extern TmdSource gMistShootingGalleryModel097DC;

extern TmdSource gMistShootingGalleryModel099CC;

extern TmdSource gMistShootingGalleryModel09BBC;

extern TmdSource gMistShootingGalleryModel09DAC;

extern TmdSource gMistShootingGalleryModel09F9C;

extern TmdSource gMistShootingGalleryModel0A18C;

extern TmdSource gMistShootingGalleryModel0A37C;

extern TmdSource gMistShootingGalleryModel0A56C;

extern TmdSource gMistShootingGalleryModel0A81C;

extern TmdSource gMistShootingGalleryModel0AB30;

extern TmdSource gMistShootingGalleryModel0AC94;

extern TmdSource gMistShootingGalleryModel0ADF8;

extern TmdSource gMistShootingGalleryModel0AF5C;

extern TmdSource gMistShootingGalleryModel0B0D0;

extern TmdSource gMistShootingGalleryModel0B2C0;

extern TmdSource gMistShootingGalleryModel0B4B0;

extern TmdSource gMistShootingGalleryModel0B6A0;

#endif // INCLUDE_ROOMS_MIST_SHOOTING_GALLERY_H
