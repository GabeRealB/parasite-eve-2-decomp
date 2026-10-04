/* Part of the Dryfield main street library; see main_street.h. */

/// Message handler for the room. Messages 0x19, 1 and 0xF answer in the copy's
/// `room` from story nibbles. Messages 0xB and 0xC run the room's own event
/// gate: unless the event's nibble is already set, the message and event are
/// latched, the nibble is set and the room's event task is spawned. Messages
/// 0xD and 0xE go through the event gate `roomEventGate`
/// and, when it fires, swap collected bits
/// 0x10F / 0x112 for 0x113. Anything else is not consumed.
s32 mainStreetResolveMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent ev;
    s32              ret;

    *out = *msg;
    if (msg->areaId == 0x19) {
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(GAME_FLAG_DRIVEWAY_PROGRESS) >= 2) {
                    out->room = 2;
                } else {
                    out->room = 1;
                }
            }
        } else if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = GameFlag_GetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
        }
    }
    if (msg->areaId == 1 && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(GAME_FLAG_NIGHT_GAS_STATION_PROGRESS) == 0) {
            out->room = 1;
        } else if (GameFlag_GetNibble(GAME_FLAG_STORY_CHAPTER) >= 4) {
            out->room = 4;
        } else {
            out->room = GameFlag_GetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 2;
        }
    }
    if (msg->areaId == 0xF && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = GameFlag_GetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) + 1;
    }
    if (msg->areaId == 0x19 && GameFlag_GetNibble(GAME_FLAG_NIGHT_MOTEL_BALCONY_SCENE_SEEN) != 0) {
        if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SetNibbleIf(msg->flagId, 2);
            Gp_RunCapCmd1(0x13);
            return 2;
        }
        return 2;
    }
    if (msg->areaId == 0xB) {
        ev.capCmd                            = 3;
        ev.stageSnd                          = 0x52020005;
        ev.flagId                            = GAME_FLAG_MAIN_STREET_TO_MOTEL_ROOM_1_SCENE;
        ev.fade                              = 0;
        gMainStreetEventSpawned.eventStarted = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->queryOnly == ROOM_EVENT_EXECUTE) {
                gRoomEventStagedMsg = *out;
                ROOM_EVENT_LATCHED  = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&gMainStreetEventTaskDesc, 0, 0, 0);
                gMainStreetEventSpawned.eventStarted = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->areaId == 0xC) {
        ev.capCmd                            = 4;
        ev.stageSnd                          = 0x52020005;
        ev.flagId                            = GAME_FLAG_MAIN_STREET_TO_MOTEL_ROOM_2_SCENE;
        ev.fade                              = 0;
        gMainStreetEventSpawned.eventStarted = 0;
        if (GameFlag_GetNibble(ev.flagId) == 0 || ev.flagId == 0) {
            if (out->queryOnly == ROOM_EVENT_EXECUTE) {
                gRoomEventStagedMsg = *out;
                ROOM_EVENT_LATCHED  = ev;
                if (ev.flagId != 0) {
                    GameFlag_SetNibble(ev.flagId, 1);
                }
                Task_SpawnFromTable(&gMainStreetEventTaskDesc, 0, 0, 0);
                gMainStreetEventSpawned.eventStarted = 1;
                return 2;
            }
            return 2;
        }
        return 1;
    } else if (msg->areaId == 0xD) {
        req.capCmd        = 0xA;
        req.missingCapCmd = 5;
        req.firstSnd      = Gp_PackStageSndId(SOUND_MAIN_STREET_MOTEL_DOOR_UNLOCK);
        req.secondSnd     = Gp_PackStageSndId(SOUND_MAIN_STREET_MOTEL_DOOR_OPEN);
        req.flagId        = GAME_FLAG_MOTEL_ROOM_3_DOOR_UNLOCKED;
        req.collectedBit  = 0x13;
        ret               = roomEventGate(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (ROOM_EVENT_ACTIVE != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->queryOnly == ROOM_EVENT_EXECUTE && GameFlag_GetNibble(GAME_FLAG_093) == 0) {
            Gp_SetNibbleIf(msg->flagId, 0);
        }
        return ret;
    } else if (msg->areaId == 0xE) {
        req.capCmd        = 0xB;
        req.missingCapCmd = 6;
        req.firstSnd      = Gp_PackStageSndId(SOUND_MAIN_STREET_MOTEL_DOOR_UNLOCK);
        req.secondSnd     = Gp_PackStageSndId(SOUND_MAIN_STREET_MOTEL_DOOR_OPEN);
        req.flagId        = GAME_FLAG_MOTEL_ROOM_4_DOOR_UNLOCKED;
        req.collectedBit  = 0x13;
        ret               = roomEventGate(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (ROOM_EVENT_ACTIVE != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
        if (msg->queryOnly == ROOM_EVENT_EXECUTE && GameFlag_GetNibble(GAME_FLAG_094) == 0) {
            Gp_SetNibbleIf(msg->flagId, 0);
        }
        return ret;
    }
    return 1;
}
