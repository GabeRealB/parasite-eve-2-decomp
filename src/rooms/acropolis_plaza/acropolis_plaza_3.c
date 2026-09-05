#include "common.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/task.h"
#include "rooms/acropolis_plaza.h"

/// Two four-vertex quads facing each other across the plaza's scene object:
/// one at x - 0xBB8, one at x + 0x7D0, each spanning y .. y + 0x3E8 and
/// z - 0x1000 .. z + 0x3000. The second quad's vertices run in the opposite
/// z order, flipping its winding.
extern SVECTOR D_acropolis_plaza_80198B90[8];

/// Rebuilds the eight box vertices in `D_acropolis_plaza_80198B90` around the
/// scene work's world position.
void func_acropolis_plaza_8017DD90(Task* arg0)
{
    AcropolisPlazaSceneWork* work = (AcropolisPlazaSceneWork*)arg0->idMap;
    s32                      x    = work->pos.vx;
    s32                      y    = work->pos.vy;
    s32                      z    = work->pos.vz;
    s16                      near = x - 0xBB8;
    s16                      top;
    s16                      left;
    s16                      right;
    s16                      far;

    D_acropolis_plaza_80198B90[0].vx = near;
    D_acropolis_plaza_80198B90[1].vx = near;
    D_acropolis_plaza_80198B90[2].vx = near;
    D_acropolis_plaza_80198B90[3].vx = near;

    top   = y + 0x3E8;
    left  = z - 0x1000;
    right = z + 0x3000;
    far   = x + 0x7D0;

    D_acropolis_plaza_80198B90[0].vy = y;
    D_acropolis_plaza_80198B90[1].vy = y;
    D_acropolis_plaza_80198B90[2].vy = top;
    D_acropolis_plaza_80198B90[3].vy = top;

    D_acropolis_plaza_80198B90[0].vz = left;
    D_acropolis_plaza_80198B90[1].vz = right;
    D_acropolis_plaza_80198B90[2].vz = left;
    D_acropolis_plaza_80198B90[3].vz = right;

    D_acropolis_plaza_80198B90[4].vx = far;
    D_acropolis_plaza_80198B90[5].vx = far;
    D_acropolis_plaza_80198B90[6].vx = far;
    D_acropolis_plaza_80198B90[7].vx = far;

    D_acropolis_plaza_80198B90[4].vy = y;
    D_acropolis_plaza_80198B90[5].vy = y;
    D_acropolis_plaza_80198B90[6].vy = top;
    D_acropolis_plaza_80198B90[7].vy = top;

    D_acropolis_plaza_80198B90[4].vz = right;
    D_acropolis_plaza_80198B90[5].vz = left;
    D_acropolis_plaza_80198B90[6].vz = right;
    D_acropolis_plaza_80198B90[7].vz = left;
}

/// The plaza's view tables, one per camera set. Each entry is a *pair* of
/// `GpViewRec`s -- the two shots the stream alternates between -- indexed by
/// `CdCmd_Queue.field_1EE - 1`, so a table row is 0x48 bytes.
extern GpViewRec D_acropolis_plaza_801838B8[][2];
extern GpViewRec D_acropolis_plaza_8018A938[][2];
extern GpViewRec D_acropolis_plaza_8018CAFC[][2];
extern GpViewRec D_acropolis_plaza_8018F530[][2];
extern GpViewRec D_acropolis_plaza_8018F9B4[][2];

/// Applies the plaza camera for view set `arg0`.
///
/// Sets 0..3 all share the opening table and pick within a row directly:
/// while `field_1FA` is clear the row's first shot is used, otherwise
/// `field_1F2` steps forward or back from it depending on `field_1F0`.
/// Sets 4..7 (and any out-of-range value, which leaves the table whatever the
/// caller left in place) instead index the table flat, one shot per step, and
/// clamp the backwards walk at the start of the table.
void func_acropolis_plaza_8017DE24(s32 arg0)
{
    CdCmdQueue* q = &CdCmd_Queue;
    GpViewRec(*tbl)[2];
    GpViewRec* view;
    s16        idx;

    switch ((u16)arg0) {
        case 0:
        case 1:
        case 2:
        case 3:
            tbl = D_acropolis_plaza_801838B8;
            if (q->field_1FA == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair];
            } else if (q->field_1F0 == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair] + q->field_1F2;
            } else {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair] - q->field_1F2;
            }
            break;
        case 4:
            tbl = D_acropolis_plaza_8018A938;
            goto common;
        case 5:
            tbl = D_acropolis_plaza_8018CAFC;
            goto common;
        case 6:
            tbl = D_acropolis_plaza_8018F530;
            goto common;
        case 7:
            tbl = D_acropolis_plaza_8018F9B4;
        default:
        common:
            if (q->field_1FA == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair];
            } else {
                if (q->field_1F0 == 0) {
                    idx = ((q->field_1EE - 1) * 2) + q->field_1F2 + 1;
                } else {
                    idx = ((q->field_1EE - 1) * 2) - q->field_1F2 - 1;
                    if (idx < 0) {
                        idx = 0;
                    }
                }
                view = *tbl + idx;
            }
            break;
    }
    Gp_ApplyView(view);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DFE0);
