/* Part of the back street library; see back_street.h. */

/// Message handler for the back street's two events. Copies the incoming
/// record to the outgoing one and answers by editing `room` of the copy; a
/// non-zero `queryOnly` suppresses the side effects.
///
/// On stage 2 (`gGameSession->location.loc.stage`), message 7 answers 1 while event
/// nibble 0x3C is clear and the stage byte, read once into a local, when it is
/// set. Message 9 with nibble 0x3F clear runs CAP command 2 on stage 2 (9
/// otherwise), sets nibble 2 of the record's flag index and returns 0. Any
/// other case, on stage 2, enqueues the type-7 event the ambience task uses to
/// stop sound 0x52050006, and returns 1.
s32 backStreetEventMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 s1;

    *out = *in;
    s1   = gGameSession->location.loc.stage;
    if (s1 == 2) {
        if (in->areaId == 7) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gameFlagGetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN) == 0) {
                    out->room = 1;
                } else {
                    out->room = s1;
                }
            }
        }
    }
    if ((in->areaId == 9) && (gameFlagGetNibble(GAME_FLAG_DILAPIDATED_HOUSE_DOOR_UNLOCKED) == 0)) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            s32 cmd = 9;

            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                cmd = 2;
            }
            Gp_RunCapCmd1(cmd);
            Gp_SetNibbleIf(in->flagId, 2);
        }
        return 0;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            SndEvt_EnqueueType7(SOUND_BACK_STREET_AMBIENCE, 0xF);
        }
    }
    return 1;
}
