#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_H

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
#define MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT 13

/// Work block of the shooting gallery's controller task, which runs one course from its countdown through its target
/// waves to the result panel.
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
extern Task* gMistShootingGalleryControllerTask;

/// Number of entries in the gallery's movie-presentation task table.
enum { MIST_SHOOTING_GALLERY_MOVIE_TASK_COUNT = 3 };

/// Post-clear presentation recipes: slot 0 session, slot 1 movie, slot 2 mode splash.
extern TaskDesc gMistShootingGalleryMovieTaskDescs[MIST_SHOOTING_GALLERY_MOVIE_TASK_COUNT];

/// Indices in `gMistShootingGalleryTaskDescs`; these are descriptor slots, not states.
enum {
    MIST_SHOOTING_GALLERY_TASK_COURSE_CONTROLLER = 0,
    MIST_SHOOTING_GALLERY_TASK_RED_FLASH         = 1,
    MIST_SHOOTING_GALLERY_TASK_COUNT             = 2,
};

/// Course-controller and red-screen-flash task descriptors for the loaded gallery.
extern TaskDesc gMistShootingGalleryTaskDescs[MIST_SHOOTING_GALLERY_TASK_COUNT];

/// Number of retained placement-variant slots, including absent slots.
enum { MIST_SHOOTING_GALLERY_AREA_VARIANT_COUNT = 12 };

/// Actor placements and loading/spawn recipes indexed by `GameLocationKey.variant`.
extern AreaVariant gMistShootingGalleryAreaVariants[MIST_SHOOTING_GALLERY_AREA_VARIANT_COUNT];

/// RESULT-panel recipe used by gameplay after a gallery course ends.
extern UiObjectDesc gMistShootingGalleryResultPanelDesc;

/// Gallery weapon-picker recipe exported to the inventory menu's root session.
extern UiObjectDesc gMistShootingGalleryWeaponSelectPanelDesc;

/// Extents of the gallery's room directory and 1-based warp endpoint table.
enum {
    MIST_SHOOTING_GALLERY_ROOM_COUNT = 1,
    MIST_SHOOTING_GALLERY_WARP_COUNT = 7,
};

/// Collision resources for gallery room 1, indexed by room ID minus one.
extern WorldCollisionRoomResources gMistShootingGalleryRoomCollisionResources[MIST_SHOOTING_GALLERY_ROOM_COUNT];

/// Logical-view mapping for the gallery's single room.
extern u8* gMistShootingGalleryViewMaps[1];

/// Logical-view count searched by the gallery's reverse view lookup.
extern ViewCount gMistShootingGalleryViewCounts[2];

/// Light collection and per-view ambient minima for the gallery's single room.
extern WorldCoordRoomLighting gMistShootingGalleryRoomLightingTable[1];

/// Gallery warp endpoints 1..7, indexed by warp ID minus one without a terminator.
extern DirectionWarpEntry gMistShootingGalleryWarpEntries[MIST_SHOOTING_GALLERY_WARP_COUNT];

/// Extent of the gallery's mapped camera and sprite-view resource arrays.
enum { MIST_SHOOTING_GALLERY_MAPPED_VIEW_COUNT = 18 };

/// Cameras for mapped gallery views 1..18, stored at mapped index minus one.
extern ViewCamera gMistShootingGalleryViewCameras[MIST_SHOOTING_GALLERY_MAPPED_VIEW_COUNT];

/// Sprite resources for mapped gallery views 1..18, stored at index minus one.
extern SpriteView gMistShootingGallerySpriteViews[MIST_SHOOTING_GALLERY_MAPPED_VIEW_COUNT];

/// Collision and footstep properties indexed by the gallery's surface class (0..7).
extern WorldCollisionSurfaceProperties* gMistShootingGallerySurfacePropertiesByClass[8];

/// Restores the training barrier and optionally lowers it out of the firing lane.
void mistShootingGallerySetTrainingBarrierLowered(s32 lowered);

/// Queues the gallery jukebox display session and returns 1.
s32 mistShootingGalleryOpenJukebox(s32 unused);

/// Selects the gallery's default or alternate room-light collection.
void mistShootingGallerySelectRoomLights(s16 useAlternate);

/// Spawns an uncounted fixed demonstration target at world XYZ (6000, 0, 3000).
void mistShootingGallerySpawnDemoTarget(void);

/// Latches the gallery action point for the active course script.
void mistShootingGallerySignalAction(void);

/// Issues the chosen training course's items and level-one Parasite Energy spells.
void mistShootingGalleryPrepareTrainingLoadout(s32 courseLevel);

/// Queues the gallery weapon picker through the inventory menu and returns 1.
s32 mistShootingGalleryOpenWeaponMenu(s32 unused);

/// Opens run-mode selection with scaled EXP/BP carryover after a cleared run.
void mistShootingGalleryOpenCarryoverModeMenu(void);

/// Draws the gallery's fixed light glows for the mapped view each frame.
void mistShootingGalleryDrawLightGlowsTask(Task* unused);

/// Animates the tracer and blue screen tint of a gallery target's attack.
void mistShootingGalleryTracerTask(Task* task);

/// Dispatches the gallery room task's initialization, running and exit states.
void mistShootingGalleryRoomTask(Task* task);

/// A gallery target's model, kind 0.
extern TmdSource gMistShootingGalleryCreatureTarget1Model;

/// A gallery target's model, kind 1.
extern TmdSource gMistShootingGalleryCreatureTarget2Model;

/// A gallery target's model, kind 2.
extern TmdSource gMistShootingGalleryCreatureTarget3Model;

/// A gallery target's model, kind 3.
extern TmdSource gMistShootingGalleryCreatureTarget4Model;

/// A gallery target's model, kind 4.
extern TmdSource gMistShootingGalleryCreatureTarget5Model;

/// A gallery target's model, kind 5.
extern TmdSource gMistShootingGalleryCreatureTarget6Model;

/// A gallery target's model, kind 6.
extern TmdSource gMistShootingGalleryCreatureTarget7Model;

/// A gallery target's model, kind 7.
extern TmdSource gMistShootingGalleryCreatureTarget8Model;

/// A gallery target's model, kind 8.
extern TmdSource gMistShootingGalleryCreatureTarget9Model;

/// A gallery target's model, kind 9.
extern TmdSource gMistShootingGalleryCreatureTarget10Model;

/// The target mount's model, the post every target hangs or stands on.
extern TmdSource gMistShootingGalleryMountModel;

/// A gallery target's model, kind 14: the left of the dark target.
extern TmdSource gMistShootingGalleryHalfTarget1Model;

/// A gallery target's model, kind 15: the left of the dark target.
extern TmdSource gMistShootingGalleryHalfTarget2Model;

/// A gallery target's model, kind 16, which no spawn reaches: the right of the dark target.
extern TmdSource gMistShootingGalleryHalfTarget3Model;

/// A gallery target's model, kind 17, which no spawn reaches: the right of the dark target.
extern TmdSource gMistShootingGalleryHalfTarget4Model;

/// A gallery target's model, kind 10.
extern TmdSource gMistShootingGalleryBullseye1Model;

/// A gallery target's model, kind 11.
extern TmdSource gMistShootingGalleryBullseye2Model;

/// A gallery target's model, kind 12.
extern TmdSource gMistShootingGalleryBullseye3Model;

/// The demonstration target's model, kind 13, Pierce's demonstration.
extern TmdSource gMistShootingGalleryDemonstrationTargetModel;

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_H
