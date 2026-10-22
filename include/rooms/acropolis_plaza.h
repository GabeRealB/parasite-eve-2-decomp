#ifndef INCLUDE_ROOMS_ACROPOLIS_PLAZA_H
#define INCLUDE_ROOMS_ACROPOLIS_PLAZA_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// Native animation sets shared with the companion actor overlay.
extern AnimationSet gAkropolisPlazaExtraAnimClip;

extern AnimationSet gAkropolisPlazaExtraAnim2Clip;

extern AnimationSet gAkropolisPlazaExtraAnim3Clip;

extern AnimationSet gAkropolisPlazaExtraAnim4Clip;

extern AnimationSet gAkropolisPlazaExtraAnim5Clip;

extern AnimationSet gAkropolisPlazaExtraAnim6Clip;

extern AnimationSet gAkropolisPlazaExtraAnim7Clip;

extern AnimationSet gAkropolisPlazaExtraAnim8Clip;

extern AnimationSet gAkropolisPlazaExtraAnim9Clip;

extern AnimationSet gAkropolisPlazaExtraAnim10Clip;

extern AnimationSet gAkropolisPlazaExtraAnim11Clip;

extern AnimationSet gAkropolisPlazaExtraAnim12Clip;

extern AnimationSet gAkropolisPlazaExtraAnim13Clip;

extern AnimationSet gAkropolisPlazaExtraAnim14Clip;

extern AnimationSet gAkropolisPlazaExtraAnim15Clip;

extern AnimationSet gAkropolisPlazaExtraAnim16Clip;

extern AnimationSet gAkropolisPlazaExtraAnim17Clip;

extern AnimationSet gAkropolisPlazaExtraAnim18Clip;

extern AnimationSet gAkropolisPlazaExtraAnim19Clip;

extern AnimationSet gAkropolisPlazaExtraAnim20Clip;

extern AnimationSet gAkropolisPlazaExtraAnim21Clip;

extern AnimationSet gAkropolisPlazaExtraAnim22Clip;

extern AnimationSet gAkropolisPlazaExtraAnim23Clip;

extern AnimationSet gAkropolisPlazaExtraAnim24Clip;

extern AnimationSet gAyaIdleClip;

extern AnimationSet gAkropolisPlazaExtraAnim25Clip;

extern AnimationSet gAkropolisPlazaExtraAnim26Clip;

extern AnimationSet gAkropolisPlazaExtraAnim27Clip;

extern AnimationSet gAkropolisPlazaExtraAnim28Clip;

extern AnimationSet gAkropolisPlazaExtraAnim29Clip;

extern AnimationSet gAkropolisPlazaExtraAnim30Clip;

extern AnimationSet gAkropolisPlazaExtraAnim31Clip;

extern AnimationSet gAkropolisPlazaExtraAnim32Clip;

extern AnimationSet gAkropolisPlazaExtraAnim33Clip;

extern AnimationSet gAkropolisPlazaExtraAnim34Clip;

/// Entry indices of the plaza's direct-spawn sequence task table.
enum {
    ACROPOLIS_PLAZA_TASK_SEQUENCE = 0,
    ACROPOLIS_PLAZA_TASK_STREAMED_SCENE,
    ACROPOLIS_PLAZA_TASK_STREAM_SCENE,
    ACROPOLIS_PLAZA_TASK_FINAL_SCENE,
    ACROPOLIS_PLAZA_TASK_FIRST_SCENE,
    ACROPOLIS_PLAZA_TASK_DISPLAY_SETUP,
    ACROPOLIS_PLAZA_TASK_REPEAT_SCENE,
    ACROPOLIS_PLAZA_TASK_FADE_TO_WHITE,
    ACROPOLIS_PLAZA_TASK_FADE_IN,
    ACROPOLIS_PLAZA_TASK_CAPTION,
    ACROPOLIS_PLAZA_TASK_MOVIE_DISPLAY,
    ACROPOLIS_PLAZA_TASK_LETTERBOX,
    ACROPOLIS_PLAZA_TASK_COUNT
};

/// Extents of the plaza's exported room, view, warp and surface tables.
enum {
    ACROPOLIS_PLAZA_ROOM_COUNT          = 1,
    ACROPOLIS_PLAZA_VIEW_COUNT          = 8,
    ACROPOLIS_PLAZA_WARP_COUNT          = 1,
    ACROPOLIS_PLAZA_SURFACE_CLASS_COUNT = 8
};

/// Direct-spawn recipes for the streamed plaza sequence and its presentation tasks.
///
/// Indexed by `ACROPOLIS_PLAZA_TASK_*`, with no terminator. Every entry starts
/// without a body at priority 192; callbacks allocate their own work as needed.
/// Gameplay starts entry 0 after a plaza session load. Spawn reads a recipe
/// synchronously, but the plaza overlay must remain loaded while its tasks run.
extern TaskDesc gAcropolisPlazaSequenceTaskDescriptors[ACROPOLIS_PLAZA_TASK_COUNT];

/// Collision resources for the plaza's single room.
///
/// Room ID 1 selects the mutable grid and action triggers, with no view-boundary
/// triggers or sight occluders. The map directory borrows this loaded-overlay
/// storage; keep it live until the grid is retired and the triggers unlinked.
extern WorldCollisionRoomResources gAcropolisPlazaRoomCollisionResources[ACROPOLIS_PLAZA_ROOM_COUNT];

/// Logical-view maps for the plaza's single room.
///
/// Room ID 1 borrows `gViewIdentityMap`: logical views 1..8 select the same
/// 1-based camera, image and sprite-view indices. There is no map terminator.
extern u8* gAcropolisPlazaViewMaps[ACROPOLIS_PLAZA_ROOM_COUNT];

/// Logical-view count for the plaza's single room, in view elements.
///
/// Room ID 1 admits eight entries for reverse lookup in the shared identity map.
extern ViewCount gAcropolisPlazaViewCounts[ACROPOLIS_PLAZA_ROOM_COUNT];

/// Lighting descriptors for the plaza's single room.
///
/// Room ID 1 selects the mutable point-light collection and default ambient
/// minima. The map directory borrows these records while the plaza is loaded.
extern WorldCoordRoomLighting gAcropolisPlazaRoomLightingTable[ACROPOLIS_PLAZA_ROOM_COUNT];

/// Static room cameras selected by the plaza's eight logical views.
///
/// Index by the identity-mapped 1-based view minus one. Each record contains
/// a world-to-view transform and a projection distance; the streamed sequence
/// uses its separate sampled camera paths. Keep the plaza loaded during use.
extern ViewCamera gAcropolisPlazaViewCameras[ACROPOLIS_PLAZA_VIEW_COUNT];

/// Image-presentation choices for the plaza's eight logical views.
///
/// Index by identity-mapped view minus one. All views have no source sprites
/// or clip lists: terminal first counts suppress image strips in views 2 and
/// 8 and retain them in views 1 and 3..7. Records and batches belong to the
/// loaded plaza overlay.
extern SpriteView gAcropolisPlazaSpriteViews[ACROPOLIS_PLAZA_VIEW_COUNT];

/// The plaza's single arrival/departure endpoint, selected by warp ID 1.
///
/// Player and companion share world position (23200, 0, 1800) and yaw 3072
/// in 4096 units per turn. Arrival selects view 2, with no transition sounds,
/// scripted-player/departure-fade flags or map marker. Borrow while loaded.
extern DirectionWarpEntry gAcropolisPlazaWarpEntries[ACROPOLIS_PLAZA_WARP_COUNT];

/// Collision and footstep records selected by room-local surface class 0..7.
///
/// Class 1 has its own footstep pairs; every other class shares the default.
/// All classes block probes, enable weapon impacts and apply grid pushback.
/// The map directory borrows these writable records while the plaza is loaded.
extern WorldCollisionSurfaceProperties* gAcropolisPlazaSurfacePropertiesByClass[ACROPOLIS_PLAZA_SURFACE_CLASS_COUNT];

/// Slots in the plaza's area-layout table, including the absent variants 0 and 2.
enum { ACROPOLIS_PLAZA_AREA_VARIANT_COUNT = 3 };

/// Area layouts for Acropolis Plaza, indexed directly by placement variant 0..2.
///
/// Only variant 1 is present: it pairs the map's two police-officer placements
/// with their actor files and controller recipes. Slots 0 and 2 are absent;
/// the table has no terminator. Gameplay borrows this table for Acropolis area 5.
/// Keep the plaza, map and actor overlays loaded during resource loading,
/// spawning and actor use; placements and recipes belong to those overlays.
extern AreaVariant gAcropolisPlazaAreaVariants[ACROPOLIS_PLAZA_AREA_VARIANT_COUNT];

/// Spawns the plaza's ambient glows, flares and siren beams on the first tick.
///
/// Requires a live single-coordinate body and the plaza's placement resources.
/// State 0 creates seven glows at placement indices 12..18, four flares at
/// 7..10 and six siren beams at 1..6, in the task coordinate's local frame.
/// Advances to state 1 and does nothing on later ticks; children use their own
/// effect lifetimes. Keep the overlay and placement records live through use.
void acropolisPlazaSpawnAmbientLightsTask(Task* task);

/// Draws a stationary flickering or pulsing additive glow at a placed light.
///
/// Bank-6 effect 0x096 requires a coordinate body and the current view, scratch
/// stack, primitive arena and depth ordering table. `spawnArg1.value` is the
/// placement slot: the plaza passes 12..18; below 16 the glow flickers yellow,
/// otherwise it pulses red. Each visible tick queues eight Gouraud wedges and
/// their blend commands. The callback keeps no pointer into scratch storage.
void acropolisPlazaLightGlowTask(Task* task);

/// Sweeps a siren beam, its point light and its near and far additive glows.
///
/// Bank-6 effect 0x098 requires a coordinate body and `effectSpawn`'s live
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` is the placement slot
/// (the plaza passes 1..6): below 5 is blue, otherwise red; its low three bits
/// select the transient point-light slot. Parity seeds opposite yaw phases.
/// `scale` is yaw in 4096 units per turn, decreasing by 128 per tick; `angle`
/// is the selected 512/2048-unit reach and `period` the point-light falloff
/// width. Requires current view, scratch, packet and ordering-table storage;
/// nothing in scratch survives the tick. Effect teardown owns the work block.
void acropolisPlazaSirenLightTask(Task* task);

/// Sweeps a pulsing four-ray flare and a flickering glow ahead of a placed light.
///
/// Bank-6 effect 0x099 requires a coordinate body and `effectSpawn`'s live
/// `EffectWork` in `spawnArg2.pointer`. `spawnArg1.value` is the placement slot
/// (the plaza passes 7..10): below 9 is blue, otherwise red. Parity seeds
/// opposite yaw phases; `scale` advances by -128 in 4096 units per turn.
/// `angle` gates the far glow with a 512/2048-unit reach selection; `period`
/// is untouched. Requires current view, scratch, packet and ordering-table
/// storage. Scratch is released each tick; effect teardown owns the work block.
void acropolisPlazaLightFlareTask(Task* task);

/// Services the plaza's queued play, reset and resume movie commands.
///
/// Called by the resident CD dispatcher while this overlay and the selected
/// movie/workspaces are loaded. Reads the queue head: play/reset use its signed
/// 16-bit big-endian sector offset; resume uses the saved absolute sector.
/// A reset also clears the movie input/output buffer selectors before initialization.
/// Polls CD readiness and playback, retiring the request only on completion.
/// Other opcodes and unrecognized steps are left untouched.
void acropolisPlazaPollStreamCommands(void);

#endif // INCLUDE_ROOMS_ACROPOLIS_PLAZA_H
