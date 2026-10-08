#include "rooms/dryfield_night_motel_balcony.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "dryfield_night_motel_balcony_private.h"

#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/sprites.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

extern u8** D_dryfield_night_motel_balcony_80182C3C[];

/// Sets the five breakaway scenery batches in view 18 to the same hidden byte.
///
/// Borrows the writable batch table (indices 6..10 must exist).
static inline void _dryfieldNightMotelBalconySetBreakawaySprites(SpriteBatch* batches, u8 hidden)
{
    batches[6].hidden  = hidden;
    batches[7].hidden  = hidden;
    batches[8].hidden  = hidden;
    batches[9].hidden  = hidden;
    batches[10].hidden = hidden;
}

/// Applies a terminated stream of byte pairs to writable area sprite batches.
///
/// (view, 255) selects a zero-based view; following (batch, hidden) pairs write
/// exact visibility bytes. A pair starting with 255 ends the stream. Requires
/// a nonempty stream beginning with a view selection and valid indices; every
/// view selection must be followed by a batch write. Borrows all data for this call.
/// The initial view is read before the terminator test, so an empty stream is invalid.
static inline void _dryfieldNightMotelBalconyApplySectionSpriteCommands(const SpriteView* areaViews, const u8* command)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_COMMAND_MARKER = 0xFF,
        DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_COMMAND_BYTES  = 2,
    };
    SpriteBatch* batches;

    batches = areaViews[command[0]].batches;
    if (command[0] != DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_COMMAND_MARKER) {
        do {
            if (command[1] == DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_COMMAND_MARKER) {
                batches  = areaViews[command[0]].batches;
                command += DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_COMMAND_BYTES;
            }
            batches[command[0]].hidden = command[1];
            command                   += DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_COMMAND_BYTES;
        } while (command[0] != DRYFIELD_NIGHT_MOTEL_BALCONY_SPRITE_COMMAND_MARKER);
    }
}

u8 D_dryfield_night_motel_balcony_80182858[68] = {
    8,
    255,
    6,
    0,
    9,
    255,
    1,
    0,
    11,
    0,
    10,
    255,
    4,
    0,
    6,
    0,
    19,
    255,
    1,
    0,
    2,
    0,
    22,
    255,
    3,
    0,
    1,
    0,
    24,
    255,
    4,
    0,
    10,
    0,
    25,
    255,
    2,
    0,
    11,
    0,
    29,
    255,
    1,
    0,
    7,
    0,
    30,
    255,
    1,
    0,
    7,
    0,
    31,
    255,
    3,
    0,
    2,
    0,
    34,
    255,
    7,
    0,
    11,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_8018289C[68] = {
    8,
    255,
    6,
    0,
    9,
    255,
    1,
    1,
    11,
    0,
    10,
    255,
    4,
    1,
    6,
    0,
    19,
    255,
    1,
    1,
    2,
    0,
    22,
    255,
    3,
    1,
    1,
    0,
    24,
    255,
    4,
    1,
    10,
    0,
    25,
    255,
    2,
    1,
    11,
    0,
    29,
    255,
    1,
    1,
    7,
    0,
    30,
    255,
    1,
    1,
    7,
    0,
    31,
    255,
    3,
    1,
    2,
    0,
    34,
    255,
    7,
    1,
    11,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_801828E0[68] = {
    8,
    255,
    6,
    1,
    9,
    255,
    1,
    1,
    11,
    1,
    10,
    255,
    4,
    1,
    6,
    1,
    19,
    255,
    1,
    1,
    2,
    1,
    22,
    255,
    3,
    1,
    1,
    1,
    24,
    255,
    4,
    1,
    10,
    1,
    25,
    255,
    2,
    1,
    11,
    1,
    29,
    255,
    1,
    1,
    7,
    1,
    30,
    255,
    1,
    1,
    7,
    1,
    31,
    255,
    3,
    1,
    2,
    1,
    34,
    255,
    7,
    1,
    11,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_motel_balcony_80182924[3] = {
    D_dryfield_night_motel_balcony_80182858,
    D_dryfield_night_motel_balcony_8018289C,
    D_dryfield_night_motel_balcony_801828E0,
};

u8 D_dryfield_night_motel_balcony_80182930[40] = {
    8,
    255,
    13,
    0,
    17,
    0,
    9,
    255,
    8,
    0,
    22,
    255,
    4,
    0,
    2,
    0,
    29,
    255,
    5,
    0,
    6,
    0,
    8,
    0,
    34,
    255,
    5,
    0,
    10,
    0,
    36,
    255,
    1,
    0,
    4,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_80182958[40] = {
    8,
    255,
    13,
    1,
    17,
    0,
    9,
    255,
    8,
    0,
    22,
    255,
    4,
    1,
    2,
    0,
    29,
    255,
    5,
    1,
    6,
    1,
    8,
    0,
    34,
    255,
    5,
    1,
    10,
    0,
    36,
    255,
    1,
    1,
    4,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_80182980[40] = {
    8,
    255,
    13,
    1,
    17,
    1,
    9,
    255,
    8,
    1,
    22,
    255,
    4,
    1,
    2,
    1,
    29,
    255,
    5,
    1,
    6,
    1,
    8,
    1,
    34,
    255,
    5,
    1,
    10,
    1,
    36,
    255,
    1,
    1,
    4,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_motel_balcony_801829A8[3] = {
    D_dryfield_night_motel_balcony_80182930,
    D_dryfield_night_motel_balcony_80182958,
    D_dryfield_night_motel_balcony_80182980,
};

u8 D_dryfield_night_motel_balcony_801829B4[32] = {
    6,
    255,
    3,
    0,
    7,
    255,
    4,
    0,
    8,
    255,
    11,
    0,
    12,
    0,
    22,
    255,
    8,
    0,
    25,
    255,
    8,
    0,
    9,
    0,
    13,
    255,
    5,
    0,
    6,
    0,
    255,
    255,
};

u8 D_dryfield_night_motel_balcony_801829D4[32] = {
    6,
    255,
    3,
    1,
    7,
    255,
    4,
    1,
    8,
    255,
    11,
    1,
    12,
    1,
    22,
    255,
    8,
    1,
    25,
    255,
    8,
    1,
    9,
    1,
    13,
    255,
    5,
    1,
    6,
    1,
    255,
    255,
};

u8* D_dryfield_night_motel_balcony_801829F4[2] = {
    D_dryfield_night_motel_balcony_801829B4,
    D_dryfield_night_motel_balcony_801829D4,
};

u8 D_dryfield_night_motel_balcony_801829FC[44] = {
    8,
    255,
    4,
    0,
    9,
    255,
    2,
    0,
    10,
    255,
    1,
    0,
    24,
    255,
    1,
    0,
    5,
    0,
    25,
    255,
    3,
    0,
    11,
    255,
    1,
    0,
    30,
    255,
    5,
    0,
    31,
    255,
    4,
    0,
    34,
    255,
    9,
    0,
    35,
    255,
    5,
    0,
    255,
    255,
};

u8 D_dryfield_night_motel_balcony_80182A28[44] = {
    8,
    255,
    4,
    1,
    9,
    255,
    2,
    1,
    10,
    255,
    1,
    1,
    24,
    255,
    1,
    1,
    5,
    1,
    25,
    255,
    3,
    1,
    11,
    255,
    1,
    1,
    30,
    255,
    5,
    1,
    31,
    255,
    4,
    1,
    34,
    255,
    9,
    1,
    35,
    255,
    5,
    1,
    255,
    255,
};

u8* D_dryfield_night_motel_balcony_80182A54[2] = {
    D_dryfield_night_motel_balcony_801829FC,
    D_dryfield_night_motel_balcony_80182A28,
};

u8 D_dryfield_night_motel_balcony_80182A5C[52] = {
    8,
    255,
    5,
    0,
    9,
    255,
    3,
    0,
    10,
    255,
    2,
    0,
    24,
    255,
    2,
    0,
    6,
    0,
    25,
    255,
    4,
    0,
    11,
    255,
    2,
    0,
    29,
    255,
    2,
    0,
    30,
    255,
    3,
    0,
    4,
    0,
    31,
    255,
    5,
    0,
    34,
    255,
    8,
    0,
    35,
    255,
    4,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_80182A90[52] = {
    8,
    255,
    5,
    1,
    9,
    255,
    3,
    1,
    10,
    255,
    2,
    1,
    24,
    255,
    2,
    1,
    6,
    1,
    25,
    255,
    4,
    1,
    11,
    255,
    2,
    1,
    29,
    255,
    2,
    1,
    30,
    255,
    3,
    1,
    4,
    0,
    31,
    255,
    5,
    1,
    34,
    255,
    8,
    1,
    35,
    255,
    4,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_motel_balcony_80182AC4[2] = {
    D_dryfield_night_motel_balcony_80182A5C,
    D_dryfield_night_motel_balcony_80182A90,
};

u8 D_dryfield_night_motel_balcony_80182ACC[56] = {
    8,
    255,
    7,
    0,
    8,
    0,
    9,
    255,
    4,
    0,
    10,
    255,
    3,
    0,
    22,
    255,
    5,
    0,
    24,
    255,
    3,
    0,
    7,
    0,
    25,
    255,
    5,
    0,
    11,
    255,
    3,
    0,
    13,
    255,
    4,
    0,
    29,
    255,
    3,
    0,
    4,
    0,
    30,
    255,
    2,
    0,
    31,
    255,
    6,
    0,
    37,
    255,
    2,
    0,
    255,
    255,
};

u8 D_dryfield_night_motel_balcony_80182B04[56] = {
    8,
    255,
    7,
    1,
    8,
    1,
    9,
    255,
    4,
    1,
    10,
    255,
    3,
    1,
    22,
    255,
    5,
    1,
    24,
    255,
    3,
    1,
    7,
    1,
    25,
    255,
    5,
    1,
    11,
    255,
    3,
    1,
    13,
    255,
    4,
    1,
    29,
    255,
    3,
    1,
    4,
    1,
    30,
    255,
    2,
    1,
    31,
    255,
    6,
    1,
    37,
    255,
    2,
    1,
    255,
    255,
};

u8* D_dryfield_night_motel_balcony_80182B3C[2] = {
    D_dryfield_night_motel_balcony_80182ACC,
    D_dryfield_night_motel_balcony_80182B04,
};

u8 D_dryfield_night_motel_balcony_80182B44[44] = {
    8,
    255,
    9,
    0,
    9,
    255,
    5,
    0,
    22,
    255,
    6,
    0,
    25,
    255,
    6,
    0,
    11,
    255,
    4,
    0,
    13,
    255,
    3,
    0,
    34,
    255,
    6,
    0,
    35,
    255,
    3,
    0,
    36,
    255,
    3,
    0,
    37,
    255,
    4,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_80182B70[44] = {
    8,
    255,
    9,
    1,
    9,
    255,
    5,
    1,
    22,
    255,
    6,
    1,
    25,
    255,
    6,
    1,
    11,
    255,
    4,
    1,
    13,
    255,
    3,
    1,
    34,
    255,
    6,
    1,
    35,
    255,
    3,
    1,
    36,
    255,
    3,
    1,
    37,
    255,
    4,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_motel_balcony_80182B9C[2] = {
    D_dryfield_night_motel_balcony_80182B44,
    D_dryfield_night_motel_balcony_80182B70,
};

u8 D_dryfield_night_motel_balcony_80182BA4[40] = {
    6,
    255,
    1,
    0,
    7,
    255,
    3,
    0,
    8,
    255,
    10,
    0,
    9,
    255,
    6,
    0,
    22,
    255,
    7,
    0,
    25,
    255,
    7,
    0,
    11,
    255,
    5,
    0,
    13,
    255,
    2,
    0,
    37,
    255,
    3,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_80182BCC[40] = {
    6,
    255,
    1,
    1,
    7,
    255,
    3,
    1,
    8,
    255,
    10,
    1,
    9,
    255,
    6,
    1,
    22,
    255,
    7,
    1,
    25,
    255,
    7,
    1,
    11,
    255,
    5,
    1,
    13,
    255,
    2,
    1,
    37,
    255,
    3,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_motel_balcony_80182BF4[2] = {
    D_dryfield_night_motel_balcony_80182BA4,
    D_dryfield_night_motel_balcony_80182BCC,
};

u8 D_dryfield_night_motel_balcony_80182BFC[28] = {
    9,
    255,
    7,
    0,
    13,
    255,
    1,
    0,
    28,
    255,
    7,
    0,
    34,
    255,
    4,
    0,
    36,
    255,
    2,
    0,
    37,
    255,
    5,
    0,
    255,
    255,
    0,
    0,
};

u8 D_dryfield_night_motel_balcony_80182C18[28] = {
    9,
    255,
    7,
    1,
    13,
    255,
    1,
    1,
    28,
    255,
    7,
    10,
    34,
    255,
    4,
    1,
    36,
    255,
    2,
    1,
    37,
    255,
    5,
    1,
    255,
    255,
    0,
    0,
};

u8* D_dryfield_night_motel_balcony_80182C34[2] = {
    D_dryfield_night_motel_balcony_80182BFC,
    D_dryfield_night_motel_balcony_80182C18,
};

u8** D_dryfield_night_motel_balcony_80182C3C[9] = {
    D_dryfield_night_motel_balcony_80182924,
    D_dryfield_night_motel_balcony_801829A8,
    D_dryfield_night_motel_balcony_801829F4,
    D_dryfield_night_motel_balcony_80182A54,
    D_dryfield_night_motel_balcony_80182AC4,
    D_dryfield_night_motel_balcony_80182B3C,
    D_dryfield_night_motel_balcony_80182B9C,
    D_dryfield_night_motel_balcony_80182BF4,
    D_dryfield_night_motel_balcony_80182C34,
};

SVECTOR D_dryfield_night_motel_balcony_80182C60[2] = {
    { -930, -2870, 0x271A, 0 },
    { -2130, -2870, 0x271A, 0 },
};

SVECTOR D_dryfield_night_motel_balcony_80182C70 = { -6010, -2870, 3050, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182C78 = { -6010, -2870, 1850, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182C80 = { -6010, -2870, -2940, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182C88 = { -6010, -2870, -4150, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182C90 = { -160, -3100, 8360, 0 };

void dryfieldNightMotelBalconyBeginMoviesTask(Task* task)
{
    displaySpawnTaskFromTable(D_dryfield_night_motel_balcony_80182834, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    taskKill(task);
}

void dryfieldNightMotelBalconyUpdateSceneSprites(u8 sceneStep)
{
    const GameSession*     session  = gGameSession;
    const GameLocationKey* location = &session->location.loc;
    const SpriteView*      areaViews;
    SpriteBatch*           batches;

    areaViews = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];

    switch (location->view) {
        case 17:
            batches = areaViews[16].batches;
            if (sceneStep == 0) {
                batches[2].hidden = 1;
            } else {
                batches[3].hidden = 1;
            }
            break;
        case 18:
            batches = areaViews[17].batches;
            _dryfieldNightMotelBalconySetBreakawaySprites(batches, 0);
            break;
        case 19:
            batches = areaViews[18].batches;
            if (sceneStep == 0) {
                batches[2].hidden = 0;
            } else {
                batches[1].hidden = 0;
            }
            // The view-19 cue also updates the alternate view-22 sprites.
        case 22:
            batches = areaViews[21].batches;
            if (sceneStep == 1) {
                batches[1].hidden = 0;
                batches[2].hidden = 1;
            } else if (sceneStep == 2) {
                batches[1].hidden = 1;
                batches[2].hidden = 0;
            } else {
                batches[2].hidden = 1;
                batches[1].hidden = 1;
            }
            break;
    }
}

void dryfieldNightMotelBalconySetSectionState(s16 sectionIndex, s16 sectionState)
{
    const GameLocationKey* location;
    const SpriteView*      areaViews;
    const u8*              command;

    command   = D_dryfield_night_motel_balcony_80182C3C[sectionIndex][sectionState];
    location  = &gGameSession->location.loc;
    areaViews = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    _dryfieldNightMotelBalconyApplySectionSpriteCommands(areaViews, command);
    // Persist the same state used to choose the visibility stream.
    switch (sectionIndex) {
        case 0:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_0_STATE, sectionState);
            break;
        case 1:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_1_STATE, sectionState);
            break;
        case 2:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_2_STATE, sectionState);
            break;
        case 3:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_3_STATE, sectionState);
            break;
        case 4:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_4_STATE, sectionState);
            break;
        case 5:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_5_STATE, sectionState);
            break;
        case 6:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_6_STATE, sectionState);
            break;
        case 7:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_7_STATE, sectionState);
            break;
        case 8:
            gameFlagSetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_8_STATE, sectionState);
            break;
    }
}

void dryfieldNightMotelBalconyRestoreSectionStates(void)
{
    dryfieldNightMotelBalconySetSectionState(0, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_0_STATE));
    dryfieldNightMotelBalconySetSectionState(1, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_1_STATE));
    dryfieldNightMotelBalconySetSectionState(2, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_2_STATE));
    dryfieldNightMotelBalconySetSectionState(3, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_3_STATE));
    dryfieldNightMotelBalconySetSectionState(4, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_4_STATE));
    dryfieldNightMotelBalconySetSectionState(5, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_5_STATE));
    dryfieldNightMotelBalconySetSectionState(6, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_6_STATE));
    dryfieldNightMotelBalconySetSectionState(7, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_7_STATE));
    dryfieldNightMotelBalconySetSectionState(8, gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SECTION_8_STATE));
}

void dryfieldNightMotelBalconyResetSceneSprites(void)
{
    const GameSession*     session  = gGameSession;
    const GameLocationKey* location = &session->location.loc;
    const SpriteView*      areaViews;
    SpriteBatch*           batches;

    areaViews = gSpriteAreaTables[location->stage - 1][session->spriteVariant - 1].areaViews[location->area - 1];

    batches           = areaViews[16].batches;
    batches[2].hidden = 0;
    batches[3].hidden = 0;

    batches = areaViews[17].batches;
    _dryfieldNightMotelBalconySetBreakawaySprites(batches, 1);

    batches           = areaViews[18].batches;
    batches[1].hidden = 1;
    batches[2].hidden = 1;

    batches           = areaViews[21].batches;
    batches[1].hidden = 1;
    batches[2].hidden = 1;
}
