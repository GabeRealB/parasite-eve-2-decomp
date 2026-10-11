#ifndef INCLUDE_ACTORS_ACTOR_443500_H
#define INCLUDE_ACTORS_ACTOR_443500_H

#include "gameplay/evs.h"

/// Opens Pierce's guided terminal tour in Shelter R47.
///
/// The 74 commands include END. Copies the opening clips into Aya's equipped
/// bank extension, stages both actors at the console and follows CAP command 4
/// with frame delays and CAP cue waits. Completion releases player control,
/// hides Pierce and restores weapons, ambient lighting, area music and room CAP.
/// Start with `gActor443500TourOpeningSkipScript` as the skip target.
/// Requires a live player, Pierce at placement index 0, both animation banks,
/// this package's CAP/animation resources and the R47 overlay. The interpreter
/// borrows the commands and callbacks through completion; copied clip pointers
/// borrow this overlay until their playback ends. Command delays count unfrozen
/// interpreter updates; blend durations count whole animation frames.
extern EvsCommand gActor443500TourOpeningScript[74];

/// Commits the opening tour's console placements when the scene is skipped.
///
/// The 16 commands include END. Fades out, places both actors at the console,
/// shows Aya, hides Pierce and releases player control. Restores weapons,
/// lighting and area music, cleans up CAP and restores room CAP before fading
/// back in. Does not install the
/// opening clip extension or request Pierce's clips. Keep both overlays and
/// their live actors loaded until the interpreter finishes.
extern EvsCommand gActor443500TourOpeningSkipScript[16];

/// Continues Pierce's tour after the guided console releases its cutscene hold.
///
/// The 137 commands include END. Installs Aya's follow-up bank extension, sets
/// saved scene event 15 and stages CAP command 5. Frame delays and CAP cues
/// coordinate both actors, open the timed map display, then resume the scene
/// with Aya and Pierce at the map terminal. Completion releases player control,
/// hides Pierce and restores weapons, lighting and room CAP. Start with
/// `gActor443500ConsoleFollowupSkipScript` as the skip target. Requires the live
/// actors, their banks, this overlay's CAP/clip data and the R47 terminal tasks.
/// Keep both overlays loaded through execution and all borrowed clip playback;
/// the initial fade task also borrows this overlay's code until it finishes.
extern EvsCommand gActor443500ConsoleFollowupScript[137];

/// Commits the console follow-up's map-terminal placements on a skip.
///
/// The 18 commands include END. Sets saved scene event 15 and places both
/// actors at the map terminal, showing Aya and hiding Pierce. Releases player
/// control and restores lighting and weapons.
/// After scene cleanup and a three-update wait, requests variant 1 of CAP
/// command 5, then waits three updates before restoring room CAP and fading in.
/// Does not install follow-up clips or open the timed map display. Keep both
/// overlays, their actors and CAP resources live through execution.
extern EvsCommand gActor443500ConsoleFollowupSkipScript[18];

/// Finishes Pierce's tour after the guided map terminal releases its hold.
///
/// The 73 commands include END. Reinstalls Aya's follow-up clips, places both
/// actors at the map terminal and coordinates their playback with CAP command
/// 6, frame delays and CAP cues. Completion releases player control, returns
/// Pierce to his fallback clip, restores lighting, weapons and room CAP,
/// applies Shelter saved-area updates and selects view 5. Its room caller pairs
/// it with the map follow-up skip stream. Keep the live actors, their banks,
/// this package's clip/CAP data and the R47 overlay loaded through execution
/// and borrowed clip playback.
extern EvsCommand gActor443500MapFollowupScript[73];

#endif // INCLUDE_ACTORS_ACTOR_443500_H
