#include "common.h"

#include "gameplay/3CD8.h"

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", D_dryfield_night_gas_station_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017D660);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", D_dryfield_night_gas_station_8017D610);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017DE2C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017E228);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017E524);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017E844);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017E9F8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017ECF0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017ED4C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017EDBC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017EEB0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017EFA0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F0A4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F188);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F250);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F318);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F3E0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F41C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F544);

/// Maps a cap (cutscene) script event key to the stage sound it should play in
/// the night gas station, then enqueues it as a type-6 sound event. Event key
/// 0x83 only plays if a cap script is still reporting an event key. Keys with
/// no sound are ignored. Always returns 0.
s32 func_dryfield_night_gas_station_8017F6B8(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 5:
            id = 0x52010005;
            goto play;
        case 7:
            id = 0x52010007;
            goto play;
        case 0xA:
            id = 0x5201000A;
            goto play;
        case 0xD:
            id = 0x5201000D;
            goto play;
        case 0x11:
            id = 0x52010011;
            goto play;
        case 0x13:
            id = 0x52010013;
            goto play;
        case 0x6D:
        case 0x82:
            id = 0x5201000B;
            goto play;
        case 0x73:
            id = 0x5201000E;
            goto play;
        case 0x83:
            if (Gp_GetCapEventKey() == 0) {
                break;
            }
            id = 0x52010012;
        play:
            Gp_EnqueueStageSnd6(id, 0, 0);
            break;
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F7E0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F89C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F990);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017F9E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017FA6C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017FAEC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017FB64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017FB70);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017FBD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_8017FD80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_801802EC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180604);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180720);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180740);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180760);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180780);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_801807A0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_801807D4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180828);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180920);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180940);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180974);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180998);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180A00);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180A34);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180A60);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180B04);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180B38);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180B5C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180BEC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180C20);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180C3C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180D1C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180DC8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80180E9C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_801812B4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80181AF8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80181D80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80182024);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80182450);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_801827E4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_80182CD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_gas_station/dryfield_night_gas_station", func_dryfield_night_gas_station_801830CC);
