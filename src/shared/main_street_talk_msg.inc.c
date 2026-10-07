/* Part of the Dryfield main street library; see main_street.h. */

/// Acts only on `arg2 == 1`. Once nibble 0x7B has reached 2 it updates Ice Bag
/// melting, stores saved object state 1 at slot 0x1B unless collected bit 0x119 is held,
/// spawns CAP entry 1 and the task above; before that it spawns CAP entry
/// 0x14 instead.
s32 mainStreetTalkMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 1) {
        if (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS) >= 2) {
            inventoryMeltIceBagIfExpired();
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                areaSetCurrentObjectState(0x1B, 1);
            }
            capSpawnEventIfIdle(1, CAP_EVENT_PAUSE_ACTORS);
            taskSpawnFromTable(&gMainStreetPlayTimeTaskDesc, 0, 0, 0);
        } else {
            capSpawnEventIfIdle(0x14, CAP_EVENT_PAUSE_ACTORS);
        }
    }
    return 0;
}
