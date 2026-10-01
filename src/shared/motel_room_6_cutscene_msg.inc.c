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
        gMotelRoom6CutsceneRec.field_0 = 0xC;
        gMotelRoom6CutsceneRec.field_1 = 1;
        switch (GameFlag_GetNibble(0x7A)) {
            case 0 ... 3:
                if (gGameSession->location.loc.stage == 2) {
                    count                           = 4;
                    gMotelRoom6CutsceneRec.field_14 = 0x3C0;
                    gMotelRoom6CutsceneRec.field_3  = 1;
                } else {
                    count                           = 2;
                    gMotelRoom6CutsceneRec.field_14 = 0x380;
                    gMotelRoom6CutsceneRec.field_3  = 1;
                }
                break;
            case 4 ... 6:
                count                           = 2;
                gMotelRoom6CutsceneRec.field_14 = 0x3C0;
                gMotelRoom6CutsceneRec.field_3  = count;
                break;
        }
        gMotelRoom6CutsceneRec.field_2  = 0;
        gMotelRoom6CutsceneRec.field_4  = Gp_PackStageSndId(0x521E0008);
        gMotelRoom6CutsceneRec.field_8  = Gp_PackStageSndId(0x521E000B);
        gMotelRoom6CutsceneRec.field_10 = Gp_PackStageSndId(0x521E0009);
        gMotelRoom6CutsceneRec.field_C  = Gp_PackStageSndId(0x521E000A);
        Task_SpawnFromTable(gRoomCutsceneTaskDescs, 0, count, &gMotelRoom6CutsceneRec);
    } else {
        motelRoom6ActionMsg(arg0, arg1, arg2, arg3);
    }
    return 0;
}
