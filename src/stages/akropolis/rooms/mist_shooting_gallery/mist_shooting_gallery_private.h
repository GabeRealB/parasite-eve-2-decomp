#ifndef STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H
#define STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/room.h"

/// Live gallery room collision mesh and its 7-by-4 XZ cell index.
extern WorldCollisionGrid gMistShootingGalleryCollisionGrid;

/// Directed view-boundary quads for the gallery's mapped views 1..18.
extern WorldCollisionTrigger gMistShootingGalleryViewBoundaryTriggers[28];

/// Room-action IDs shared by the gallery trigger data and room-message handler.
enum {
    MIST_SHOOTING_GALLERY_ACTION_OPEN_COURSE_MENU = 1,
    MIST_SHOOTING_GALLERY_ACTION_TALK_TO_PIERCE   = 2,
    MIST_SHOOTING_GALLERY_ACTION_TRAINING_POINT   = 3,
    MIST_SHOOTING_GALLERY_ACTION_STORY_EVENT      = 4,
};

/// Gallery interaction quads for warps, CAP commands and room-action points.
extern WorldCollisionTrigger gMistShootingGalleryActionTriggers[21];

/// Default gallery room lighting: fourteen point lights contributing in every view.
extern WorldCoordRoomLights gMistShootingGalleryDefaultRoomLights;

/// Alternate gallery shading collection: one directional, 18 point and 15 cone lights.
extern WorldCoordRoomLights gMistShootingGalleryAlternateRoomLights;

/// Number of per-view color entries in the gallery's ambient-light table.
enum { MIST_SHOOTING_GALLERY_AMBIENT_VIEW_COUNT = 18 };

/// Minimum ambient model-shading colors for the gallery's 18 views.
extern WorldCoordRoomAmbientEntry gMistShootingGalleryViewAmbientTable[MIST_SHOOTING_GALLERY_AMBIENT_VIEW_COUNT + 1];

/// Unspent EXP captured before the carryover menu changes the selected run mode.
extern s32 gMistShootingGalleryUnscaledExp;

/// Spendable BP captured before the carryover menu changes the selected run mode.
extern s32 gMistShootingGalleryUnscaledBp;

/// Returns the bonus BP earned by a score in the active gallery course.
s32 mistShootingGalleryGetBonusBp(s32 score);

/// Tests whether bonus BP also qualifies for the active course's parking prize.
s32 mistShootingGalleryQualifiesForPrize(s32 bonusBp);

// The room data's record types: its wave scripts and target scores are written from its room instance.

/// One record of a course's wave script: a gallery target to spawn once the script clock reaches `frame`, or a marker
/// that controls the script.
typedef struct {
    u16 frame;      // Script frame the target spawns on, or a `MIST_SHOOTING_GALLERY_SPAWN_*` marker
    s16 spawnArgLo; // Low half of the target's spawn argument: kind and mount behaviour
    s16 spawnArgHi; // High half of the target's spawn argument: path, hold time and flags
    s16 x;          // World position the target's mount is placed at
    s16 y;
    s16 z;
} _MistShootingGallerySpawn;
STATIC_ASSERT_SIZEOF(_MistShootingGallerySpawn, 0xC);

/// One string for each run mode, in the order of the save's `gameMode`
typedef struct {
    const char* byMode[4]; // Text for run mode 0..3
} _MistShootingGalleryModeTexts;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryModeTexts, 0x10);

/// A rating a row of the gallery's DATA panel can show: how far its gauge is filled and the word printed beside it.
typedef struct {
    s32         level; // Marks drawn on the row's gauge (1 lowest .. 5 full)
    const char* name;  // Word for the rating ("EASY", "GOOD", "VERY POOR", ...)
} _MistShootingGalleryRating;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryRating, 0x8);

/// Score and label for one gallery target kind, as the RESULT panel prints it.
typedef struct {
    s32         points; // Points added for each kill of this kind; negative deducts
    const char* name;   // Name printed for the kind ("Bacterium", "Woman", "Red Target", ...)
} _MistShootingGalleryTargetScore;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryTargetScore, 0x8);

#endif // STAGES_AKROPOLIS_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H
