#ifndef STAGES_AKROPOLIS_EVENT_FLAGS_H
#define STAGES_AKROPOLIS_EVENT_FLAGS_H

// Values of `EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS`.
enum {
    AKROPOLIS_CAFETERIA_ROUTE_NOT_STARTED       = 0,
    AKROPOLIS_CAFETERIA_ROUTE_ENTERED_AKROPOLIS = 1, // The east elevator hall's opening has played
    AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED     = 2,
    AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED    = 3,
    AKROPOLIS_CAFETERIA_ROUTE_FOUNTAIN_UNLOCKED = 5,
};

// Values of `EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS`.
enum {
    AKROPOLIS_SANCTUARY_ROUTE_OBSERVATORY_LOCK_FOUND = 1,
    AKROPOLIS_SANCTUARY_ROUTE_ESCALATOR_UNLOCKED     = 2, // By the security room's blue key
    AKROPOLIS_SANCTUARY_ROUTE_OBSERVATORY_ENTERED    = 3, // The forked road's scene to the observatory has played
    AKROPOLIS_SANCTUARY_ROUTE_PROMENADE_ENTERED      = 4,
    AKROPOLIS_SANCTUARY_ROUTE_SANCTUARY_REACHED      = 5,
};

// Values of `EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS`.
enum {
    AKROPOLIS_BRIDGE_SANCTUARY_ENTERED = 2,
    AKROPOLIS_BRIDGE_RAISED            = 3,
};

// Values of `EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE`.
enum {
    AKROPOLIS_SANCTUARY_BOMB_SCENE_PLAYED    = 1,
    AKROPOLIS_SANCTUARY_BOMB_SCENE_PASSED_BY = 2,
};

// Values of `EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_STATE`.
enum {
    AKROPOLIS_PATIO_CAFETERIA_DOOR_LOCKED   = 1, // The player has found it locked
    AKROPOLIS_PATIO_CAFETERIA_DOOR_UNLOCKED = 2, // The east elevator hall scene has unlocked it
    AKROPOLIS_PATIO_CAFETERIA_DOOR_OPENED   = 3, // Opened: the warp to the cafeteria has started
};

// Values of `EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS`.
enum {
    AKROPOLIS_PATIO_CUTSCENE_NOT_STARTED = 0,
    AKROPOLIS_PATIO_CUTSCENE_START       = 1, // Leaving the west elevator hall for the square the first time
    AKROPOLIS_PATIO_CUTSCENE_PLAYING     = 3,
    AKROPOLIS_PATIO_CUTSCENE_DONE        = 4,
};

// Values of `EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT`.
enum {
    AKROPOLIS_CAFETERIA_BOSS_REPORT_QUEUED = 1, // The boss scene has played; the report to MIST is pending
    AKROPOLIS_CAFETERIA_BOSS_REPORT_MADE   = 4, // The report has been made from the square's telephone
};

// Values of `EVENT_FLAG_STORY_DIALOGUE_INDEX`: the next call from Baldwin.
enum {
    AKROPOLIS_STORY_DIALOGUE_ARRIVAL           = 1, // "This is Baldwin."
    AKROPOLIS_STORY_DIALOGUE_LATE              = 2, // The first call was missed
    AKROPOLIS_STORY_DIALOGUE_CIVILIAN_EVAC     = 3, // After the east elevator hall
    AKROPOLIS_STORY_DIALOGUE_NEW_SPECIES       = 4, // After the cafeteria boss
    AKROPOLIS_STORY_DIALOGUE_ARMORED_ASSAILANT = 5, // After the sanctuary
    AKROPOLIS_STORY_DIALOGUE_HELICOPTER        = 6, // After the fire escape or the roof garden
    AKROPOLIS_STORY_DIALOGUE_STILL_HERE        = 7, // Back from the landing pad
};

// Values of the shooting gallery's prizes, `EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_FIRST` + the course.
enum {
    MIST_SHOOTING_GALLERY_PRIZE_BP_ONLY = 1, // The course earned BP, not its prize
    MIST_SHOOTING_GALLERY_PRIZE_WAITING = 2, // The prize waits at the parking lot, with Jodie
    MIST_SHOOTING_GALLERY_PRIZE_CLAIMED = 3,
};

// Bits of `EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS`.
enum {
    AKROPOLIS_SECURITY_ROOM_PANEL_KEY_BLUE_INSERTED = 1 << 0,
    AKROPOLIS_SECURITY_ROOM_PANEL_KEY_RED_INSERTED  = 1 << 1,
};

#endif // STAGES_AKROPOLIS_EVENT_FLAGS_H
