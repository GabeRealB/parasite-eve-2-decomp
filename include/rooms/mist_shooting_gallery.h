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

/// Number of target kinds the gallery scores: the low nibble of a target's
/// spawn argument, indexing both `MistShootingGalleryWork::kills` and the RESULT
/// panel's point table.
#define MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT 13

/// Work block of the shooting gallery's controller task, which runs one course
/// from its countdown through its target waves to the result panel.
///
/// The controller allocates it zeroed and publishes its task as
/// `D_mist_shooting_gallery_8018E0C4`, through which the targets it spawns and
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

extern Task* D_mist_shooting_gallery_8018E0C4;

extern TaskDesc D_mist_shooting_gallery_80185384[3];

extern TaskDesc D_mist_shooting_gallery_801856B8[2];

extern AreaVariant D_mist_shooting_gallery_8018DF74[12];

extern UiObjectDesc D_mist_shooting_gallery_80185000;

extern UiObjectDesc D_mist_shooting_gallery_80184F70;

// mist_shooting_gallery
extern WorldCollisionRoomResources D_mist_shooting_gallery_801853A8[];

extern u8* D_mist_shooting_gallery_801853B8[];

extern ViewCount D_mist_shooting_gallery_801853BC[];

/// Light collection and per-view ambient minima for the gallery's single room.
///
/// Indexed by the 1-based room ID minus one; only room 1 is valid. The mutable
/// `lights` pointer initially selects the default collection and may switch to
/// the alternate collection. Both share `ambientTable` for views 1..18.
/// The table and its borrowed data are valid only while the room overlay is loaded.
extern WorldCoordRoomLighting gMistShootingGalleryRoomLightingTable[1];

extern DirectionWarpEntry D_mist_shooting_gallery_801853C8[];

extern ViewCamera D_mist_shooting_gallery_8018998C[];

extern SpriteView D_mist_shooting_gallery_8018BD10[];

extern WorldCollisionSurfaceProperties* D_mist_shooting_gallery_8018E09C[];

void func_mist_shooting_gallery_80180390(s32 arg0);

s32 func_mist_shooting_gallery_80180B34(s32 unused);

void func_mist_shooting_gallery_801811C0(s16 arg0);

void func_mist_shooting_gallery_801848B4(void);

void func_mist_shooting_gallery_80184954(void);

void func_mist_shooting_gallery_8017DCAC(s32 mode);

s32 func_mist_shooting_gallery_8017F95C(s32 unused);

void func_mist_shooting_gallery_8017FBD8(void);

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

void func_mist_shooting_gallery_8018018C(Task* task);

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
