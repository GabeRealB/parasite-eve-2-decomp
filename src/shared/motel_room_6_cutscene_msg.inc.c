/* Part of the motel room 6 library; see motel_room_6.h. */

#include "main/event_flags.h"

#ifndef MOTEL_ROOM_6_HANDLE_OTHER_COMMAND
#error "Bind MOTEL_ROOM_6_HANDLE_OTHER_COMMAND to the carrier's non-cutscene command handler"
#endif

/// Configures and requests the motel-room-6 story cutscene for command 22.
///
/// Handles `ROOM_MESSAGE_COMMAND`; other commands are forwarded with all four
/// arguments through `MOTEL_ROOM_6_HANDLE_OTHER_COMMAND`, ignoring its reply.
/// That binding must name a declared four-argument handler; the day instance
/// returns void. The room owns the cutscene record and must leave it unchanged
/// until the runner completes. Chapters 0..6 select the CAP file, VRAM page and
/// post-scene command. Other chapters retain file/page and use command 0.
/// Does not reset the record's texture-page Y. Always returns 0.
static s32 _motelRoom6CutsceneMessage(Task* task, s32 messageId, s32 command, s32 secondArg)
{
    enum {
        MOTEL_ROOM_6_COMMAND_CUTSCENE    = 22,
        MOTEL_ROOM_6_CUTSCENE_VIEW       = 12,
        MOTEL_ROOM_6_CUTSCENE_CAP_SLOT   = 1,
        MOTEL_ROOM_6_CAP_DAY_FOLLOW_UP   = 4,
        MOTEL_ROOM_6_CAP_LATER_FOLLOW_UP = 2,
        MOTEL_ROOM_6_CAP_TPAGE_RIGHT_X   = 960,
        MOTEL_ROOM_6_CAP_TPAGE_LEFT_X    = 896,
        MOTEL_ROOM_6_FIRST_CAP_FILE      = 1,
    };
    s32 followUpCommand;

    // Retain untouched record fields; the runner borrows this singleton.
    followUpCommand = 0;
    if (command == MOTEL_ROOM_6_COMMAND_CUTSCENE) {
        _gMotelRoom6CutsceneRec.view    = MOTEL_ROOM_6_CUTSCENE_VIEW;
        _gMotelRoom6CutsceneRec.capSlot = MOTEL_ROOM_6_CUTSCENE_CAP_SLOT;
        switch (gameFlagGetNibble(EVENT_FLAG_STORY_CHAPTER)) {
            case 0 ... 3:
                if (gGameSession->location.stage == GAME_STAGE_DRYFIELD) {
                    followUpCommand                   = MOTEL_ROOM_6_CAP_DAY_FOLLOW_UP;
                    _gMotelRoom6CutsceneRec.capTPageX = MOTEL_ROOM_6_CAP_TPAGE_RIGHT_X;
                    _gMotelRoom6CutsceneRec.capFile   = MOTEL_ROOM_6_FIRST_CAP_FILE;
                } else {
                    followUpCommand                   = MOTEL_ROOM_6_CAP_LATER_FOLLOW_UP;
                    _gMotelRoom6CutsceneRec.capTPageX = MOTEL_ROOM_6_CAP_TPAGE_LEFT_X;
                    _gMotelRoom6CutsceneRec.capFile   = MOTEL_ROOM_6_FIRST_CAP_FILE;
                }
                break;
            case 4 ... 6:
                followUpCommand                   = MOTEL_ROOM_6_CAP_LATER_FOLLOW_UP;
                _gMotelRoom6CutsceneRec.capTPageX = MOTEL_ROOM_6_CAP_TPAGE_RIGHT_X;
                _gMotelRoom6CutsceneRec.capFile   = followUpCommand;
                break;
        }
        _gMotelRoom6CutsceneRec.skipScene       = 0;
        _gMotelRoom6CutsceneRec.startSound      = sndScriptResolveStageId(SOUND_MOTEL_ROOM_6_SCENE_START);
        _gMotelRoom6CutsceneRec.endSound        = sndScriptResolveStageId(SOUND_MOTEL_ROOM_6_SCENE_END);
        _gMotelRoom6CutsceneRec.sceneSound      = sndScriptResolveStageId(SOUND_MOTEL_ROOM_6_SCENE_TRACK);
        _gMotelRoom6CutsceneRec.afterSceneSound = sndScriptResolveStageId(SOUND_MOTEL_ROOM_6_SCENE_COMPLETE);
        taskSpawnFromTable(_gRoomCutsceneTaskDescs, ROOM_CUTSCENE_TASK_SCENE, followUpCommand, &_gMotelRoom6CutsceneRec);
    } else {
        MOTEL_ROOM_6_HANDLE_OTHER_COMMAND(task, messageId, command, secondArg);
    }
    return 0;
}
