#ifndef INCLUDE_ACTORS_ACTOR_450800_H
#define INCLUDE_ACTORS_ACTOR_450800_H

#include "gameplay/evs.h"

/// First-entry nursery scene, synchronized to CAP sequence 1.
///
/// Installs the player and companion nursery clips, stages Kyle's dialogue,
/// enables the room effects and ambience, then restores view 3 and player control.
/// Start with `EVENT_SCRIPT_HUD_HIDE_RESTORE` and
/// `gActor450800NurseryIntroSkipScript`. Keep this package, the nursery room,
/// CAP resources and live player/companion/Kyle tasks loaded through playback;
/// installed animation data remains borrowed while its clips play.
extern EvsCommand gActor450800NurseryIntroScript[];

/// Skip path that restores the nursery intro's return poses, ambience and music.
///
/// Borrows the resources and installed animation banks of
/// `gActor450800NurseryIntroScript`. Cancels CAP playback, restores view 3,
/// shows Kyle and releases scripted player control under a primary fade.
extern EvsCommand gActor450800NurseryIntroSkipScript[];

/// Nursery re-entry setup while the saved nursery progress is 1.
///
/// Installs companion clips, restores the companion's return placement and
/// clip 52, and restarts Kyle's clip 1. Start with `EVENT_SCRIPT_HUD_KEEP`;
/// requires this package, the companion and placed Kyle actor to stay loaded.
/// The installed companion animation data remains borrowed during playback.
extern EvsCommand gActor450800NurseryReentryScript[];

/// Nursery re-entry setup after the room's command-10 scene advances progress.
///
/// Restores the companion as in `gActor450800NurseryReentryScript`, then places
/// Kyle at his dialogue position in clip 20. The room selects this for progress
/// other than 0 or 1. Uses the same resource lifetimes and `EVENT_SCRIPT_HUD_KEEP`.
extern EvsCommand gActor450800NurseryAdvancedReentryScript[];

/// CAP-13 nursery shooting scene, ending with a growth-room transition.
///
/// Selects the scene/audio stream, stages Kyle's gunfire and both Pawn GOLEMs,
/// then the second body. The transition is suppressed in demo scene 9.
/// Start with `EVENT_SCRIPT_HUD_HIDE_RESTORE` and
/// `gActor450800NurseryShootingSkipScript`. Keep this package, the nursery room,
/// CAP/audio resources and live player/companion/scene actors loaded while used;
/// animation and vibration resources remain borrowed by their playing tasks.
extern EvsCommand gActor450800NurseryShootingScript[];

/// Skip path for the nursery shooting scene and its growth-room transition.
///
/// Cancels CAP playback, requests the same transition (except in demo scene 9),
/// then restores view 3, return poses and player control under a primary fade.
/// Borrows the resources and installed banks of `gActor450800NurseryShootingScript`.
extern EvsCommand gActor450800NurseryShootingSkipScript[];

/// Starts the nursery interaction selected by progress and capped visit counts.
///
/// Requires this actor package and its event/CAP resources to be loaded.
/// Acts only in view 4. Progress 1 increments the package's count up to 3;
/// otherwise increments the saved scene count up to 3, choosing the first or
/// repeat scene. The first saved scene sets objective 0x32. Scripts hide the
/// HUD through playback and restore it afterwards.
void actor450800StartNurseryInteraction(void);

/// Starts the nursery's companion dialogue with scripted player and companion poses.
///
/// Requires this actor package and its CAP resources to be loaded, with the
/// player and companion tasks registered. The event script hides the HUD for
/// playback and restores it when finished.
void actor450800StartNurseryCompanionDialogue(void);

/// Starts Kyle Madigan's nursery scene pose and places him at its starting transform.
///
/// Requires this actor package to be loaded and placed scene actor 0 to have
/// a live model and animation rig. Both messages consume package-owned records
/// synchronously; the animation request precedes placement.
void actor450800PrepareNurseryKyleMadigan(void);

#endif // INCLUDE_ACTORS_ACTOR_450800_H
