#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011573C;
extern s32 D_80115744;

/// The passage's per-view emitter placements, one `SVECTOR` per position, 8
/// bytes apart. All four names address the same 24-entry run: `ED8` is `EC8[2]`,
/// `EE8` is `EC8[4]` and `F08` is `EC8[8]`, so view 2's last two positions are
/// `F08[8]` / `F08[9]` and view 4's last is `F08[15]`.
extern SVECTOR D_mine_secret_passage_80180EC8[];
extern SVECTOR D_mine_secret_passage_80180ED8[];
extern SVECTOR D_mine_secret_passage_80180EE8[];
extern SVECTOR D_mine_secret_passage_80180F08[];

/// Publishes this passage's four emitter ids on the task's first tick - the
/// `D_8011573C` / `D_80115744` / `D_80115728` / `D_80115720` slots take
/// 0x60240-0x60243 in that order, the same slot order every other room uses -
/// then draws the emitters the current camera view shows: a run of placements
/// out of one of the passage's arrays, each drawn as a `Room_Draw01` wedge pair
/// (half-extent 0x200-0x280, colour 0x444 except view 6's 0x44) or a
/// `Room_Draw13` disc (half-extent 0x200 or 0x400, colour 0x421 or 0x444).
/// Views 3 and 7 both end on `ED8[20]`, so the compiler merges their last two
/// calls into one shared tail, which in turn ends on the single `Room_Draw13`
/// every other view finishes with.
void func_mine_secret_passage_8017D9D4(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60242;
        D_80115744  = 0x60241;
        D_8011573C  = 0x60240;
        D_80115720  = 0x60243;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_secret_passage_80180EC8;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw13(&p[16], 0x200, 0x421);
            Room_Draw13(&p[17], 0x200, 0x421);
            break;
        }
        case 3: {
            SVECTOR* p = D_mine_secret_passage_80180ED8;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw13(&p[14], 0x200, 0x421);
            Room_Draw13(&p[15], 0x200, 0x421);
            Room_Draw13(&p[16], 0x200, 0x421);
            Room_Draw13(&p[17], 0x200, 0x421);
            Room_Draw13(&p[20], 0x200, 0x421);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_secret_passage_80180ED8;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw13(&p[16], 0x200, 0x421);
            Room_Draw13(&p[18], 0x200, 0x421);
            Room_Draw13(&p[19], 0x200, 0x421);
            Room_Draw13(&p[20], 0x200, 0x421);
            Room_Draw13(&p[21], 0x200, 0x421);
            break;
        }
        case 5: {
            SVECTOR* p = D_mine_secret_passage_80180EE8;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw13(&p[6], 0x400, 0x444);
            Room_Draw13(&p[7], 0x400, 0x444);
            Room_Draw13(&p[8], 0x400, 0x444);
            Room_Draw13(&p[9], 0x400, 0x444);
            Room_Draw13(&p[10], 0x200, 0x421);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_secret_passage_80180EE8;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw01(&p[2], 0x200, 0x444);
            Room_Draw01(&p[4], 0x280, 0x44);
            Room_Draw13(&p[9], 0x400, 0x444);
            Room_Draw13(&p[10], 0x200, 0x421);
            Room_Draw13(&p[11], 0x200, 0x421);
            break;
        }
        case 7: {
            SVECTOR* p = D_mine_secret_passage_80180ED8;
            Room_Draw01(&p[0], 0x200, 0x444);
            Room_Draw13(&p[16], 0x200, 0x421);
            Room_Draw13(&p[18], 0x200, 0x421);
            Room_Draw13(&p[20], 0x200, 0x421);
            break;
        }
        case 8: {
            SVECTOR* p = D_mine_secret_passage_80180F08;
            Room_Draw01(&p[0], 0x280, 0x44);
            break;
        }
    }
}
