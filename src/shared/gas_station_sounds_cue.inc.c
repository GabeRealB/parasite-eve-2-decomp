#include "main/sound_ids.h"

/* Part of the gas station sounds library; see gas_station_sounds.h. */

/// Maps gas-station CAP sound cues to sound scripts in the current stage.
///
/// Installed for `ROOM_MESSAGE_SOUND` in both gas-station rooms. Cue 0x83
/// queues entry 0x12 only when the current CAP variant key is nonzero;
/// unknown cues do nothing. Always returns 0; receiver, ID and the second
/// payload are unused.
static s32 _gasStationCueSoundMsg(Task* task, s32 messageId, s32 cueKey, s32 unusedArg)
{
    enum {
        GAS_STATION_SOUND_CUE_5   = 5,
        GAS_STATION_SOUND_CUE_7   = 7,
        GAS_STATION_SOUND_CUE_10  = 10,
        GAS_STATION_SOUND_CUE_13  = 13,
        GAS_STATION_SOUND_CUE_17  = 17,
        GAS_STATION_SOUND_CUE_19  = 19,
        GAS_STATION_SOUND_CUE_109 = 109,
        GAS_STATION_SOUND_CUE_115 = 115,
        GAS_STATION_SOUND_CUE_130 = 130,
        GAS_STATION_SOUND_CUE_131 = 131,
    };
    switch (cueKey) {
        case GAS_STATION_SOUND_CUE_5:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 5), 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_7:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 7), 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_10:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x0A), 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_13:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x0D), 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_17:
            sndEvtRequestStageScriptStart(SOUND_GAS_STATION_CUTSCENE_LOOP, 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_19:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x13), 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_109:
        case GAS_STATION_SOUND_CUE_130:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x0B), 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_115:
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x0E), 0, 0);
            break;
        case GAS_STATION_SOUND_CUE_131:
            if (capGetVariantKey() == 0) {
                break;
            }
            sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_GAS_STATION, 0x12), 0, 0);
            break;
    }
    return 0;
}
