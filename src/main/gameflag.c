#include "main/gameflag.h"

#include "types.h"

#include "main/pad.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"

#include "gameplay/captions.h"
#include "gameplay/evs_scripts.h"

/// Bit layout of the two game flags stored in each payload byte.
enum {
    GAME_FLAG_BITS_PER_NIBBLE  = 4,
    GAME_FLAG_NIBBLE_MASK      = (1 << GAME_FLAG_BITS_PER_NIBBLE) - 1,
    GAME_FLAG_HIGH_NIBBLE_MASK = GAME_FLAG_NIBBLE_MASK << GAME_FLAG_BITS_PER_NIBBLE
};

TaskDesc D_80067734[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL },
    { { { TASK_BODY_NONE, 0x20 } }, taskKill },
    { { { TASK_BODY_NONE, 0x80 } }, func_800E7570 },
    { { { TASK_BODY_NONE, 0x20 } }, func_800E8830 },
    { { { TASK_BODY_NONE, 0x80 } }, capHudSlideTask },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0x20 } }, taskKill },
    { { { TASK_BODY_NONE, 0x80 } }, Gp_EndWaitTask },
    { { { TASK_BODY_NONE, 0x20 } }, evsScreenShakeTask },
    { { { TASK_BODY_NONE, 0x20 } }, evsMusicVolumeFadeTask },
    { { { TASK_BODY_NONE, 0x20 } }, evsSoundAttenuationFadeTask },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL },
    { { { TASK_BODY_NONE, 0xC0 } }, NULL },
    { { { TASK_BODY_NONE, 0x20 } }, func_800E4028 },
    { { { TASK_DESC_END, 0x20 } }, NULL },
};

void gameFlagSetNibble(s32 flagId, s32 value)
{
    s32 byteIndex = flagId / 2;

    // Preserve the neighboring flag in the same saved byte.
    if (flagId & 1) {
        GameFlagNibbleBank* liveBank = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE];
        liveBank->payload.packedFlags[byteIndex] =
            (liveBank->payload.packedFlags[byteIndex] & GAME_FLAG_HIGH_NIBBLE_MASK) | (value & GAME_FLAG_NIBBLE_MASK);
    } else {
        GameFlagNibbleBank* liveBank = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE];
        liveBank->payload.packedFlags[byteIndex] =
            (liveBank->payload.packedFlags[byteIndex] & GAME_FLAG_NIBBLE_MASK) | (value << GAME_FLAG_BITS_PER_NIBBLE);
    }
}

s32 gameFlagGetNibble(s32 flagId)
{
    s32 byteIndex = flagId / 2;

    if (flagId & 1) {
        const GameFlagNibbleBank* liveBank = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE];
        return liveBank->payload.packedFlags[byteIndex] & GAME_FLAG_NIBBLE_MASK;
    } else {
        const GameFlagNibbleBank* liveBank = &gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE];
        return liveBank->payload.packedFlags[byteIndex] >> GAME_FLAG_BITS_PER_NIBBLE;
    }
}

s32 padIsStartPressed(void)
{
    return padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START);
}
