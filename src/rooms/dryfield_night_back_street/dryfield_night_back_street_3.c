#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32     D_8011572C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_dryfield_night_back_street_8018036C[];
extern SVECTOR D_dryfield_night_back_street_8018037C[];
extern SVECTOR D_dryfield_night_back_street_8018038C[];

/// The room's night lights, two `SVECTOR`s apiece: the pair is a light's two
/// ends, which `Room_Draw08` sweeps a wedge between and `Room_Draw17` glows at.
/// All six sit in one 0x60 run - the two shafts at 0x8018034C and 0x8018035C,
/// the three glows the camera views select between at 0x8018036C (view 2),
/// 0x8018037C (view 3) and 0x8018038C (views 4 and 5), and the shared smoke
/// trail's spawn offsets at 0x8018039C, which `rooms_shared_8017f9e4` reaches
/// by name as `RoomsShared8017f9e4Pos` / `Pos2`. splat names the anchors the
/// code resolves directly, so `_8018036C` is the *third* light's first end and
/// the two shafts before it are that array's `[-4]` and `[-2]` elements.
///
/// Per-frame effect tick. The first time it runs it latches the room's three
/// effect ids, which the text actor's script events spawn (`Actor02000_Fn03690`
/// uses `D_8011572C`), then marks the effect state `Gp_State1C->roomEffectMode` and
/// lights the room for the camera view the stage-visit byte
/// `gGameSession->at4.loc.view` selects. Views other than 2, 3, 4 and 5 draw
/// nothing, and view 3's glows are additional to view 2's shafts.
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
            Room_Draw17(&D_dryfield_night_back_street_8018037C[0], 1, 0x300);
            Room_Draw17(&D_dryfield_night_back_street_8018037C[1], 1, 0x300);
            /* fallthrough */
        case 2:
            Room_Draw17(&D_dryfield_night_back_street_8018036C[0], 0, 0x300);
            Room_Draw17(&D_dryfield_night_back_street_8018036C[1], 0, 0x300);
            Room_Draw08(&D_dryfield_night_back_street_8018036C[-4], 0x100);
            Room_Draw08(&D_dryfield_night_back_street_8018036C[-2], 0x100);
            break;
        case 4:
        case 5:
            Room_Draw17(&D_dryfield_night_back_street_8018038C[0], 1, 0x300);
            Room_Draw17(&D_dryfield_night_back_street_8018038C[1], 1, 0x300);
            break;
    }
}
