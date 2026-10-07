/* Part of the room variants library; see room_variants.h. */

/// Answers the room message `in`, copying it to `out` first. For message 2 it
/// reports in `out->room` how far nibble 0x61 has advanced (3 once nibble
/// 0x7A reaches 4). Message 3 returns 2 when the session sits at stage 3,
/// place 1 with `gSceneCombatState` agreeing, and 0 while nibble 0x3B is clear;
/// message 2 returns 0 while nibble 0x45 reads 1. The cap commands and nibble
/// write that go with those answers run only when `in->queryOnly` is clear.
/// Every other case returns 1.
s32 roomVariantGasStationMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 n;
    s32 val;

    *out = *in;
    if (in->areaId == 2 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        n = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
        if (n >= 4) {
            val = 3;
        } else {
            val = gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
        out->room = val;
    }
    if (in->areaId == 3) {
        if ((gGameSession->location.loc.stage == in->areaId) && (gGameSession->location.loc.variant == 1) &&
            (gSceneCombatState.signals.bytes.battlePhase == gGameSession->location.loc.variant)) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(0x15);
            }
            return 2;
        }
        if (gameFlagGetNibble(GAME_FLAG_GENERAL_STORE_DOOR_UNLOCKED) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(7);
                gameFlagSetNibbleIfPresent(in->flagId, 2);
            }
            return 0;
        }
    }
    if (in->areaId == 2) {
        if (gameFlagGetNibble(GAME_FLAG_GAS_STATION_MAIN_STREET_BLOCKED) == 1) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                capRunCommandWithTransition(8);
            }
            return 0;
        }
    }
    return 1;
}
