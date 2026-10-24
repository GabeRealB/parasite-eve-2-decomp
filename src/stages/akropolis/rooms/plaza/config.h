#ifndef STAGES_AKROPOLIS_ROOMS_PLAZA_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_PLAZA_CONFIG_H

// Configuration of the Akropolis plaza.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the plaza's area bank.
#define AKROPOLIS_PLAZA_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_PLAZA, (cue))

/// Entries of the plaza's area bank.
enum {
    AKROPOLIS_PLAZA_SOUND_OFFICER_AMBIENCE  = 1,  // Under the traversal's officer shots
    AKROPOLIS_PLAZA_SOUND_ENTRY_AMBIENCE    = 2,  // The traversal's opening, faded out early
    AKROPOLIS_PLAZA_SOUND_FINAL_SCENE_CUE   = 3,  // The final scene's cue
    AKROPOLIS_PLAZA_SOUND_APPROACH_AMBIENCE = 4,  // The traversal's approach
    AKROPOLIS_PLAZA_SOUND_WALK_AMBIENCE     = 5,  // The whole traversal
    AKROPOLIS_PLAZA_SOUND_OFFICERS_WALK_IN  = 11, // The officers walk in, in the final scene
};

#endif
