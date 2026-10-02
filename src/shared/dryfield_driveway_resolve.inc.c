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
        fl        = GameFlag_GetNibble(0x47) == 0;
        out->room = fl ? 1 : 2;
    }
    if (in->areaId == 0x20 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        fl        = GameFlag_GetNibble(0x51) == 0;
        out->room = fl ? 2 : 1;
        if (GameFlag_GetNibble(0x53) != 0) {
            out->room = out->room + 2;
        }
    }
    if (in->areaId == 2 && GameFlag_GetNibble(0x61) != 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(6);
            Gp_SetNibbleIf(in->flagId, 2);
        }
        return 2;
    }
    if (in->areaId == 0x20) {
        if (GameFlag_GetNibble(0x3A) != 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    if (gGameSession->location.loc.variant == 1) {
                        if (GameFlag_GetNibble(0x50) == 0) {
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
        if (in->queryOnly == ROOM_EVENT_EXECUTE && GameFlag_GetNibble(0x4B) == 1) {
            GameFlag_SetNibble(0x4B, 2);
        }
    }
    if (in->areaId == 0x17) {
        if (GameFlag_GetNibble(0x30) == 1) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(2);
                return 2;
            }
            return 2;
        }
        req.capCmd            = 9;
        req.stageSnd          = 0x52190003;
        req.flagId            = 0x11C;
        req.fade              = 0;
        p                     = &req;
        gDrivewayEventSpawned = 0;
        if (GameFlag_GetNibble(p->flagId) == 0 || p->flagId == 0) {
            if (out->queryOnly == ROOM_EVENT_EXECUTE) {
                gRoomEventStagedMsg = *out;
                gRoomEventLatched   = req;
                if (p->flagId != 0) {
                    GameFlag_SetNibble(p->flagId, 1);
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
