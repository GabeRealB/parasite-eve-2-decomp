#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_night_main_street.h"

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
extern u8**    D_dryfield_night_main_street_80182168;
extern u8**    D_dryfield_night_main_street_8018216C;
extern u8**    D_dryfield_night_main_street_80182170;
extern u8**    D_dryfield_night_main_street_80182174;

/// Applies the patch list `table[GameFlag_GetNibble(nibble)]` to the current
/// area's view sprite commands. The list is a stream of byte pairs ended by a
/// 0xFF first byte: `(view, 0xFF)` selects that view's command list, and any
/// other `(cmd, value)` stores `value` in that command's `field_4`. The list
/// starts on the view named by its first byte.
#define DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(table, nibble)      \
    {                                                                   \
        GpAreaKey* sess;                                                \
        GpSprtRec* rec;                                                 \
        GpSprtCmd* cmd;                                                 \
        u8*        p;                                                   \
        s16        idx;                                                 \
        u8**       tbl;                                                 \
                                                                        \
        idx  = GameFlag_GetNibble(nibble);                              \
        tbl  = table;                                                   \
        p    = tbl[idx];                                                \
        sess = &gGameSession->at4.loc;                                  \
        rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1]; \
        cmd  = rec[p[0]].field_4;                                       \
        if (p[0] != 0xFF) {                                             \
            do {                                                        \
                if (p[1] == 0xFF) {                                     \
                    cmd = rec[p[0]].field_4;                            \
                    p  += 2;                                            \
                }                                                       \
                cmd[p[0]].field_4 = p[1];                               \
                p                += 2;                                  \
            } while (p[0] != 0xFF);                                     \
        }                                                               \
    }

/// Applies the sprite-command patch lists selected by game-flag nibbles 0x88,
/// 0x89, 0x8A and 0x8C, one table of lists per nibble.
void func_dryfield_night_main_street_8017E118(void)
{
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182168, 0x88);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_8018216C, 0x89);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182170, 0x8A);
    DRYFIELD_NIGHT_MAIN_STREET_APPLY_SPRT_PATCH(D_dryfield_night_main_street_80182174, 0x8C);
}

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
        func_dryfield_night_main_street_8017E940(D_dryfield_night_main_street_801821A8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[2]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821B8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[4]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821C8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[6]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821D8, 0x180);
    }
    if (mask & D_dryfield_night_main_street_80182230[8]) {
        func_dryfield_night_main_street_8017E940(&D_dryfield_night_main_street_801821E8, 0x180);
    }
    for (i = 10; i < 16; i++) {
        if (mask & D_dryfield_night_main_street_80182230[i]) {
            func_dryfield_night_main_street_8017F128(&D_dryfield_night_main_street_801821A8[i], 1, 0x380);
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
