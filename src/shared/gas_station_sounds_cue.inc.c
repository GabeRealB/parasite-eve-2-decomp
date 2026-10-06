/* Part of the gas station sounds library; see gas_station_sounds.h. */

/// Maps a cap (cutscene) script event key to the stage sound it should play in
/// the gas station, then enqueues it as a type-6 sound event. Event key 0x83
/// only plays if a cap script is still reporting an event key. Keys with no
/// sound are ignored. Always returns 0.
s32 gasStationCueSoundMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 5:
            Gp_EnqueueStageSnd6(0x52010005, 0, 0);
            break;
        case 7:
            Gp_EnqueueStageSnd6(0x52010007, 0, 0);
            break;
        case 0xA:
            Gp_EnqueueStageSnd6(0x5201000A, 0, 0);
            break;
        case 0xD:
            Gp_EnqueueStageSnd6(0x5201000D, 0, 0);
            break;
        case 0x11:
            Gp_EnqueueStageSnd6(0x52010011, 0, 0);
            break;
        case 0x13:
            Gp_EnqueueStageSnd6(0x52010013, 0, 0);
            break;
        case 0x6D:
        case 0x82:
            Gp_EnqueueStageSnd6(0x5201000B, 0, 0);
            break;
        case 0x73:
            Gp_EnqueueStageSnd6(0x5201000E, 0, 0);
            break;
        case 0x83:
            if (capGetVariantKey() == 0) {
                break;
            }
            Gp_EnqueueStageSnd6(0x52010012, 0, 0);
            break;
    }
    return 0;
}
