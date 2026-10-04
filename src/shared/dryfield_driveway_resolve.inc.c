/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Event gate for the driveway. Every message is answered by editing the copy
/// in `out`; the two that matter are message 0x17, which reports whether the
/// road flag is clear and otherwise stages the pending request at
/// `gRoomEventLatched` for `roomEventStagedTask`
/// to replay as a CAP command, and message 0x20, which reports the gate flag and
/// spawns the cutscene task at `gDrivewayCutsceneTasks`.
s32 drivewayResolveEvent(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomLatchedEvent  req;
    RoomLatchedEvent* p;
    s32               fl;

    *out = *in;
    if (in->areaId == 0x17 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        fl        = gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0;
        out->room = fl ? 1 : 2;
    }
    if (in->areaId == 0x20 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        fl        = gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0;
        out->room = fl ? 2 : 1;
        if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
            out->room = out->room + 2;
        }
    }
    if (in->areaId == 2 && gameFlagGetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(6);
            Gp_SetNibbleIf(in->flagId, 2);
        }
        return 2;
    }
    if (in->areaId == 0x20) {
        if (gameFlagGetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) != 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    if (gGameSession->location.loc.variant == 1) {
                        if (gameFlagGetNibble(GAME_FLAG_050) == 0) {
                            Task_SpawnFromTable(gDrivewayCutsceneTasks, 1, 0, 0);
                            return 0;
                        }
                    }
                }
                if (gGameSession->location.loc.variant == 1 && gSceneCombatState.signals.bytes.battlePhase == gGameSession->location.loc.variant) {
                    return 0;
                }
                Gp_RunCapCmd1(1);
                return 0;
            }
            return 0;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(GAME_FLAG_COMPANION_2_SCHEDULE) == 1) {
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 2);
        }
    }
    if (in->areaId == 0x17) {
        if (gameFlagGetNibble(GAME_FLAG_030) == 1) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(2);
                return 2;
            }
            return 2;
        }
        req.capCmd            = 9;
        req.stageSnd          = 0x52190003;
        req.flagId            = GAME_FLAG_DRIVEWAY_TO_FACTORY_SCENE;
        req.fade              = 0;
        p                     = &req;
        gDrivewayEventSpawned = 0;
        if (gameFlagGetNibble(p->flagId) == 0 || p->flagId == 0) {
            if (out->queryOnly == ROOM_EVENT_EXECUTE) {
                gRoomEventStagedMsg = *out;
                gRoomEventLatched   = req;
                if (p->flagId != 0) {
                    gameFlagSetNibble(p->flagId, 1);
                }
                Task_SpawnFromTable(&gRoomEventStagedTaskDesc, 0, 0, 0);
                gDrivewayEventSpawned = 1;
                return 2;
            }
            return 2;
        }
    }
    return 1;
}
