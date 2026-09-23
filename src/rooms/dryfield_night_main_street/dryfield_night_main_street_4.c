#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Advances the shared LCG and yields the high half of the new state.
#define DRYFIELD_NIGHT_MAIN_STREET_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

extern s32     D_80070F70;
extern s32     D_80115720;
extern s32     D_80115728;
extern s32     D_8011573C;
extern s32     D_80115744;
extern u16     D_dryfield_night_main_street_80182178[];
extern SVECTOR D_dryfield_night_main_street_801821A8[];
extern SVECTOR D_dryfield_night_main_street_801821B8;
extern SVECTOR D_dryfield_night_main_street_801821C8;
extern SVECTOR D_dryfield_night_main_street_801821D8;
extern SVECTOR D_dryfield_night_main_street_801821E8;
extern s32     D_dryfield_night_main_street_80182230[];
extern u32     Gp_LcgState;

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_main_street/dryfield_night_main_street_4", func_dryfield_night_main_street_8017E118);

/// Per-frame room task. On its first run it stores the ids 0x60286-0x60289 in
/// four gameplay globals. Each run it draws the anchors whose view mask in
/// `D_...80182230` contains the current view (entries 2 and 3 are cleared once
/// nibble 0x7F is set) and publishes the view's `roomEffectMode`. In views 8
/// and 0x13 it spawns 0x30 randomly placed 0x601B2 effects on entering the
/// view, and one more on each run with bit 0 of `D_80070F70` set while it
/// stays. `spawnArg1` holds the view seen on the previous run.
void func_dryfield_night_main_street_8017E484(Task* task)
{
    s32 mask;
    s32 i;

    mask = 1 << (Gp_GetViewIndex() & 0xFF);
    if (task->state == 0) {
        D_80115728  = 0x60286;
        D_80115744  = 0x60287;
        D_8011573C  = 0x60288;
        D_80115720  = 0x60289;
        task->state = 1;
    }
    if (GameFlag_GetNibble(0x7F) != 0) {
        D_dryfield_night_main_street_80182230[3] = 0;
        D_dryfield_night_main_street_80182230[2] = 0;
    }
    if (mask & D_dryfield_night_main_street_80182230[0]) {
        Room_Draw08(D_dryfield_night_main_street_801821A8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[2]) {
        Room_Draw08(&D_dryfield_night_main_street_801821B8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[4]) {
        Room_Draw08(&D_dryfield_night_main_street_801821C8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[6]) {
        Room_Draw08(&D_dryfield_night_main_street_801821D8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[8]) {
        Room_Draw08(&D_dryfield_night_main_street_801821E8, 0x180);
    }
    for (i = 10; i < 16; i++) {
        if (mask & D_dryfield_night_main_street_80182230[i]) {
            Room_Draw17(&D_dryfield_night_main_street_801821A8[i], 1, 0x380);
        }
    }
    Gp_State1C->roomEffectMode = D_dryfield_night_main_street_80182178[(Gp_GetViewIndex() & 0xFF) - 1];
    if ((Gp_GetViewIndex() & 0xFF) == 8 || (Gp_GetViewIndex() & 0xFF) == 0x13) {
        if (task->spawnArg1 != (Gp_GetViewIndex() & 0xFF)) {
            for (i = 0; i < 0x30; i++) {
                D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
                D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
                D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
                Gp_SpawnEff(0x601B2, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) + 0x103100,
                            &D_dryfield_night_main_street_801821A8[16]);
            }
        } else if (D_80070F70 & 1) {
            D_dryfield_night_main_street_801821A8[16].vx = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 300 - 0x4A1;
            D_dryfield_night_main_street_801821A8[16].vy = DRYFIELD_NIGHT_MAIN_STREET_RAND() % 600 - 0x4E7;
            D_dryfield_night_main_street_801821A8[16].vz = 0x2927 - DRYFIELD_NIGHT_MAIN_STREET_RAND() % 700;
            Gp_SpawnEff(0x601B2, NULL, (DRYFIELD_NIGHT_MAIN_STREET_RAND() & 0x10FF) | 0x82100,
                        &D_dryfield_night_main_street_801821A8[16]);
        }
    }
    task->spawnArg1 = Gp_GetViewIndex() & 0xFF;
}
