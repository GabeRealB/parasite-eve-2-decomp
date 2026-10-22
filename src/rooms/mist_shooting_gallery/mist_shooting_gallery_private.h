#ifndef SRC_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H
#define SRC_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/room.h"

/// Live gallery room collision mesh and its 7-by-4 XZ cell index.
///
/// Cells span 4000 room-coordinate units from origin (-12000, -7000).
/// The 57 faces index 22 normals and 136 vertices. Story-barrier edits use
/// faces/normals 0..2 and vertices 0..7; training-barrier edits use face/normal 3
/// and vertices 8..11, with the retained offset pass also moving vertices 12..15.
/// Activation binds the view transform. The descriptor and writable pools belong
/// to the loaded gallery overlay; borrowed pointers must not outlive it.
extern WorldCollisionGrid gMistShootingGalleryCollisionGrid;

/// Directed view-boundary quads for the gallery's mapped views 1..18.
///
/// Source/destination pairs occupy 28 records, including the LAST-marked final
/// boundary. Origins, corner offsets and radii use game units; normals use
/// 4096 per unit. Room binding supplies coordinates and mutable list links.
/// Storage must stay loaded until the active boundary list is cleared.
extern WorldCollisionTrigger gMistShootingGalleryViewBoundaryTriggers[28];

/// Room-action IDs shared by the gallery trigger data and room-message handler.
enum {
    MIST_SHOOTING_GALLERY_ACTION_OPEN_COURSE_MENU = 1,
    MIST_SHOOTING_GALLERY_ACTION_TALK_TO_PIERCE   = 2,
    MIST_SHOOTING_GALLERY_ACTION_TRAINING_POINT   = 3,
    MIST_SHOOTING_GALLERY_ACTION_STORY_EVENT      = 4,
};

/// Gallery interaction quads for warps, CAP commands and room-action points.
///
/// All 21 records are live, including the LAST-marked final quad. Room binding
/// supplies coordinates, links and enabled state; collision and action dispatch
/// update hit latches. Positions/radii use game units and normals use 4096 per
/// unit. The loaded gallery owns the storage until its action list is cleared.
extern WorldCollisionTrigger gMistShootingGalleryActionTriggers[21];

/// Default gallery room lighting: fourteen point lights contributing in every view.
///
/// Selected initially by the room table and restored by the lighting selector's
/// zero option. There are no directional or cone lights. The loaded room overlay
/// owns the collection and its mutable light array; borrowed pointers must not
/// outlive the overlay. Coordinate updates and shading queries modify the lights.
extern WorldCoordRoomLights gMistShootingGalleryDefaultRoomLights;

/// Alternate gallery shading collection: one directional, 18 point and 15 cone lights.
///
/// Selected by the nonzero lighting option, independently of the shared ambient
/// table. The gallery owns the record and mutable arrays: coordinate updates
/// compose their transforms and shading queries write attenuation. Borrowed
/// pointers remain valid only while the room overlay is loaded.
extern WorldCoordRoomLights gMistShootingGalleryAlternateRoomLights;

/// Number of per-view colour entries in the gallery's ambient-light table.
///
/// Excludes entry zero, which stores this count; valid view indices are 1..18.
enum { MIST_SHOOTING_GALLERY_AMBIENT_VIEW_COUNT = 18 };

/// Minimum ambient model-shading colours for the gallery's 18 views.
///
/// Entry zero stores the view count; entries 1..18 use the room's 1-based view
/// index. RGB components use 16 units per 8-bit colour level; stored brightness
/// in the fourth component is unused by lighting. Both room light collections
/// share this table. Borrowed pointers are valid only while the overlay is loaded.
extern WorldCoordRoomAmbientEntry gMistShootingGalleryViewAmbientTable[MIST_SHOOTING_GALLERY_AMBIENT_VIEW_COUNT + 1];

/// Unspent EXP captured before the carryover menu changes the selected run mode.
///
/// Captured only after SELECT is created; STATUS previews its scaled value and
/// confirmation writes the same conversion to the player. Valid for that display
/// session while the gallery is loaded; stored separately from the changing live balance.
extern s32 gMistShootingGalleryUnscaledExp;

/// Spendable BP captured before the carryover menu changes the selected run mode.
///
/// Captured alongside `gMistShootingGalleryUnscaledExp`; STATUS and confirmation
/// scale this unchanged snapshot for the selected mode during the display session.
/// It belongs to the loaded gallery overlay, independently of the live BP balance.
extern s32 gMistShootingGalleryUnscaledBp;

/// Returns the bonus BP earned by a score in the active gallery course.
///
/// Requires the live gallery controller and its initialized work. Returns
/// 0, 100, 200 or 300 BP, without crediting rewards or changing course flags.
/// Minimum scores for 100/200/300 BP by zero-based course are:
/// 0: 8000/9000/10000; 1: 16000/16800/17400; 2: 39000/46000/50000;
/// 3: 52000/56000/60000; 4: 50000/53000/55000. Other courses return 0.
s32 mistShootingGalleryGetBonusBp(s32 score);

/// Tests whether bonus BP also qualifies for the active course's parking prize.
///
/// Requires live controller work. Courses 0..2 require at least 200 BP; courses
/// 3..4 require at least 300 BP. Returns 0 or 1 without changing flags or rewards.
s32 mistShootingGalleryQualifiesForPrize(s32 bonusBp);

// The room data's record types: its wave scripts and target scores are written from its room instance.

/// One record of a course's wave script: a gallery target to spawn once the
/// script clock reaches `frame`, or a marker that controls the script.
///
/// A course walks its script in order. Consecutive records carrying the same
/// `frame` spawn together, and since the clock only counts up, frames do not
/// decrease along a script. A course may also spawn its leading records one at
/// a time by index, ignoring `frame`. A marker record uses only `frame`; its
/// other fields are zero.
///
/// The two argument halves form the 32-bit spawn argument of the gallery
/// target actor, kept as halfwords because the word would sit unaligned:
///
/// - bits 0-3: target kind (below `MIST_SHOOTING_GALLERY_TARGET_KIND_COUNT`),
///   which picks the target's model, hit points and score
/// - bits 12-15: behaviour of the target's mount (0 follows its path, 1 starts
///   turned half a revolution and runs the alternate path state, 2 is the
///   uncounted demonstration target, which no script requests)
/// - bits 16-23: movement path the mount follows
/// - bits 24-27: seconds the mount holds on a path whose first waypoint has no speed
/// - bit 28: the mount keeps rotating
/// - bit 29: forwarded to the target itself
/// - bit 30: spawn even when the player is within 0x400 of the position
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
/// (0 Replay, 1 Bounty, 2 Scavenger, 3 Nightmare).
///
/// The gallery keeps two: the mode names its mode list draws as rows, and the
/// descriptions its help panel shows for the mode in force. Both are constants
/// that their reader copies whole onto its stack before indexing, and the
/// struct is what lets a file-scope table be copied by assignment.
typedef struct {
    const char* byMode[4]; // Text for run mode 0..3
} _MistShootingGalleryModeTexts;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryModeTexts, 0x10);

/// A rating a row of the gallery's DATA panel can show: how far its gauge is
/// filled and the word printed beside it.
///
/// The panel rates the run mode in force on four rows - mission level,
/// condition, enemy level and supply level - and each row has a table of four
/// of these, one for every run mode in the order of the save's `gameMode`
/// (0 Replay, 1 Bounty, 2 Scavenger, 3 Nightmare).
typedef struct {
    s32         level; // Marks drawn on the row's gauge (1 lowest .. 5 full)
    const char* name;  // Word for the rating ("EASY", "GOOD", "VERY POOR", ...)
} _MistShootingGalleryRating;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryRating, 0x8);

/// Score and label for one gallery target kind, as the RESULT panel prints it.
///
/// The table of these is indexed by target kind, the same index as
/// `MistShootingGalleryWork::kills`. `points` is added once for each kill of
/// that kind, and is negative when the kill is penalised.
typedef struct {
    s32         points; // Points added for each kill of this kind; negative deducts
    const char* name;   // Name printed for the kind ("Bacterium", "Woman", "Red Target", ...)
} _MistShootingGalleryTargetScore;
STATIC_ASSERT_SIZEOF(_MistShootingGalleryTargetScore, 0x8);

#endif // SRC_ROOMS_MIST_SHOOTING_GALLERY_MIST_SHOOTING_GALLERY_PRIVATE_H
