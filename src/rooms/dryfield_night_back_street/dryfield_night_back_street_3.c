#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/dryfield_night_back_street.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_dryfield_night_back_street_8018036C[];
extern SVECTOR D_dryfield_night_back_street_8018037C[];
extern SVECTOR D_dryfield_night_back_street_8018038C[];

/// The room's light points. Two shafts come first, each a pair of ends at
/// 0x8018034C and 0x8018035C, then glow points in pairs: those of camera view
/// 2 at 0x8018036C, of view 3 at 0x8018037C and of views 4 and 5 at
/// 0x8018038C. The code names only the glow pairs, so the shafts are reached
/// as elements -4 and -2 of the view-2 array.
///
/// Per-frame room task. On its first run it stores the effect ids 0x6000A,
/// 0x60097 and 0x600E4 in three gameplay globals. Each run it sets
/// `roomEffectMode` to 2 and draws the lights of the current camera view
/// (`gGameSession->at4.loc.view`): view 2 draws a sprite on each of its two
/// glow points with `func_dryfield_night_back_street_8017E108` and a shaft
/// between each pair of shaft ends with
/// `func_dryfield_night_back_street_8017D920`; view 3 adds its own two glows
/// to view 2's set; views 4 and 5 draw their shared two glows; every other
/// view draws nothing.
void func_dryfield_night_back_street_8017D7E0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758 = 0x6000A;
        D_8011572C = 0x60097;
        D_80115750 = 0x600E4;
    }
    Gp_State1C->roomEffectMode = 2;
    switch (gGameSession->at4.loc.view) {
        case 3:
            func_dryfield_night_back_street_8017E108(&D_dryfield_night_back_street_8018037C[0], 1, 0x300);
            func_dryfield_night_back_street_8017E108(&D_dryfield_night_back_street_8018037C[1], 1, 0x300);
            /* fallthrough */
        case 2:
            func_dryfield_night_back_street_8017E108(&D_dryfield_night_back_street_8018036C[0], 0, 0x300);
            func_dryfield_night_back_street_8017E108(&D_dryfield_night_back_street_8018036C[1], 0, 0x300);
            func_dryfield_night_back_street_8017D920(&D_dryfield_night_back_street_8018036C[-4], 0x100);
            func_dryfield_night_back_street_8017D920(&D_dryfield_night_back_street_8018036C[-2], 0x100);
            break;
        case 4:
        case 5:
            func_dryfield_night_back_street_8017E108(&D_dryfield_night_back_street_8018038C[0], 1, 0x300);
            func_dryfield_night_back_street_8017E108(&D_dryfield_night_back_street_8018038C[1], 1, 0x300);
            break;
    }
}
