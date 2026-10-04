/* Part of the motel room 6 library; see motel_room_6.h. */

/// Handler of message 0x13F0 in the room's message table. For event 0x16 it
/// fills in the cutscene script record - the cap file and fade chosen from
/// flag nibble 0x7A and the stage, and the scene's sound events - and spawns
/// the cutscene task on it. Any other event goes to
/// `motelRoom6ActionMsg`.
s32 motelRoom6CutsceneMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 count;

    count = 0;
    if (arg2 == 0x16) {
        gMotelRoom6CutsceneRec.view    = 0xC;
        gMotelRoom6CutsceneRec.capSlot = 1;
        switch (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER)) {
            case 0 ... 3:
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    count                            = 4;
                    gMotelRoom6CutsceneRec.capTPageX = 0x3C0;
                    gMotelRoom6CutsceneRec.capFile   = 1;
                } else {
                    count                            = 2;
                    gMotelRoom6CutsceneRec.capTPageX = 0x380;
                    gMotelRoom6CutsceneRec.capFile   = 1;
                }
                break;
            case 4 ... 6:
                count                            = 2;
                gMotelRoom6CutsceneRec.capTPageX = 0x3C0;
                gMotelRoom6CutsceneRec.capFile   = count;
                break;
        }
        gMotelRoom6CutsceneRec.skipScene       = 0;
        gMotelRoom6CutsceneRec.startSound      = Gp_PackStageSndId(SOUND_MOTEL_ROOM_6_SCENE_START);
        gMotelRoom6CutsceneRec.endSound        = Gp_PackStageSndId(SOUND_MOTEL_ROOM_6_SCENE_END);
        gMotelRoom6CutsceneRec.sceneSound      = Gp_PackStageSndId(SOUND_MOTEL_ROOM_6_SCENE_TRACK);
        gMotelRoom6CutsceneRec.afterSceneSound = Gp_PackStageSndId(SOUND_MOTEL_ROOM_6_SCENE_COMPLETE);
        Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, count, &gMotelRoom6CutsceneRec);
    } else {
        motelRoom6ActionMsg(arg0, arg1, arg2, arg3);
    }
    return 0;
}
