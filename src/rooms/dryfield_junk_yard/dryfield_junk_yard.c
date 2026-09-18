#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_junk_yard.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern s32 D_dryfield_junk_yard_8017DD20;
extern s32 D_dryfield_junk_yard_8017DD88;
extern s32 D_dryfield_junk_yard_8017DDD8;
extern s32 D_dryfield_junk_yard_8017DDEC;
extern s32 D_dryfield_junk_yard_8017DE00;
extern s32 D_dryfield_junk_yard_8017DE18;
extern s32 D_dryfield_junk_yard_8017DE30;
extern s32 D_dryfield_junk_yard_8017DE48;
extern s32 D_dryfield_junk_yard_8017E028;
extern s32 D_dryfield_junk_yard_8017E490;
extern s32 D_dryfield_junk_yard_8017E658;

void func_dryfield_junk_yard_8017D658(Task* task);

/// State 0 of the yard's pickup-model task: hides the mesh with flag 4
/// (`field_C` 0x84) once the item's 2-bit flag reads 2, otherwise shows it
/// (`field_C` 0) and clears the animation byte, then advances the task.
void func_dryfield_junk_yard_8017D5F4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj          = (GpItemObj8*)task->spawnArg2;
    tmd          = (TmdObject*)task->extra;
    flag         = Gp_GetCurBit2Flag(obj->field_8);
    tmd->field_C = 0;
    if (flag == 2) {
        tmd->field_C = 0x84;
    } else {
        tmd->field_E = 0;
    }
    func_dryfield_junk_yard_8017D658(task);
}

/// Per-frame draw for the pickup model: after refreshing the model's world
/// matrix, draws the ground-effect quad at its position.
///
/// Skipped outright once the model is marked for deferred kill (`field_C` bit
/// 0x80, what `Task_Kill` ORs in for a spawnType-1 task) or before its aux
/// buffers exist (`field_18`, allocated by `Tmd_AllocBuffers`), so a model
/// still streaming in draws nothing. `0x1A0` and `0xC0` are the same width and
/// height the room's other ground quads pass.
void func_dryfield_junk_yard_8017D658(Task* task)
{
    DjyGroundQuadScratch* scratch;
    GsCOORDINATE2*        coord;
    TmdObject*            tmd;

    tmd   = (TmdObject*)task->extra;
    coord = tmd->field_8;
    if ((tmd->field_C & 0x80) == 0 && tmd->field_18 != 0) {
        scratch                             = *(DjyGroundQuadScratch**)0x1F8003FC - 1;
        *(DjyGroundQuadScratch**)0x1F8003FC = scratch;
        Gp_UpdateCoord(coord);
        scratch->pos.vx = coord->workm.t[0];
        scratch->pos.vy = coord->workm.t[1];
        scratch->pos.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&scratch->pos, 0x1A0, 0xC0);
        *(DjyGroundQuadScratch**)0x1F8003FC += 1;
    }
}

void func_dryfield_junk_yard_8017D708(Task* arg0)
{
    arg0->field_24 = &D_dryfield_junk_yard_8017DD20;
    Game_SetPtrSlot(arg0, 7);
    if (Game_GetPtrSlot(0xA) != 0) {
        if (GameFlag_GetNibble(0x38) == 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE00, 0);
            Gp_AllyAnimId(&D_dryfield_junk_yard_8017DD88);
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DD88, 0);
        }
        if ((GameFlag_GetNibble(0x39) == 0) && (GameFlag_GetNibble(0x28) >= 2)) {
            GameFlag_SetNibble(0x39, 1);
            func_800E8634((s32)&D_dryfield_junk_yard_8017E490, 0, (s32)&D_dryfield_junk_yard_8017E658);
        } else if (gGameSession->loc.warp == 2) {
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE30, 0);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// State 2 of the yard's room task: the pickup sequence's script driver. Every
/// case writes its own `task->state + 1; return;`; jump2's cross jumping folds
/// those five identical tails into the one increment block the ROM shows, and
/// folds cases 4 and 6's identical `Gp_DispatchMsg(..., 0, 0)` into one call.
///
/// Cases 3, 5 and 7 hand slot 0xA a message (`0x3EE`, `0x3E8`, `0x3E8`) with a
/// script pointer; cases 4 and 6 send the bare `0x3F0` / `0x3ED` and stop the
/// sequence when the driver answers nonzero. Case 1 gates the whole thing on
/// `gGameSession->eventState` (the field `func_dryfield_junk_yard_8017D708` tests
/// as `field_8`), and cases 7/2 fall through to `Task_Kill`.
void func_dryfield_junk_yard_8017D848(Task* task)
{
    switch (task->state) {
        case 0:
            func_800E8634((s32)&D_dryfield_junk_yard_8017DE48, 0, (s32)&D_dryfield_junk_yard_8017E028);
            task->state = task->state + 1;
            return;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 3:
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3EE, (s32)&D_dryfield_junk_yard_8017DE18, 0);
            task->state = task->state + 1;
            return;
        case 4:
            if (Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3F0, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 5:
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DDD8, 0);
            task->state = task->state + 1;
            return;
        case 6:
            if (Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3ED, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 7:
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DDEC, 0);
            /* fallthrough */
        case 2:
            Task_Kill(task);
            return;
    }
}
