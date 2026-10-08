/* Part of the Dryfield main street library; see main_street.h. */

/// Starts the main-street talk event and checks the completed Ice Bag handover.
///
/// Handles `ROOM_MESSAGE_COMMAND`, command 1; other arguments are unused.
/// After handover progress reaches 2, checks melting, updates saved object
/// state when the bag is absent, and requests the talk CAP event and completion
/// task. Before that progress, requests the alternate CAP event. Both require
/// the room's CAP resources to remain loaded. Always returns 0.
static s32 _mainStreetTalkMessage(Task* task, s32 messageId, s32 command, s32 unusedSecondArg)
{
    enum {
        MAIN_STREET_COMMAND_TALK        = 1,
        MAIN_STREET_ICE_BAG_OBJECT_SLOT = 27,
        MAIN_STREET_CAP_AFTER_HANDOVER  = 1,
        MAIN_STREET_CAP_BEFORE_HANDOVER = 20,
    };
    if (command == MAIN_STREET_COMMAND_TALK) {
        if (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= 2) {
            inventoryMeltIceBagIfExpired();
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                areaSetCurrentObjectState(MAIN_STREET_ICE_BAG_OBJECT_SLOT, 1);
            }
            capSpawnEventIfIdle(MAIN_STREET_CAP_AFTER_HANDOVER, CAP_EVENT_PAUSE_ACTORS);
            taskSpawnFromTable(&gMainStreetPlayTimeTaskDesc, 0, 0, 0);
        } else {
            capSpawnEventIfIdle(MAIN_STREET_CAP_BEFORE_HANDOVER, CAP_EVENT_PAUSE_ACTORS);
        }
    }
    return 0;
}
