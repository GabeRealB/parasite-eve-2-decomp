#include "common.h"

#include "actors/actors_shared_801351d4.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>

extern s32 D_80115728;
extern s32 Gp_LcgState;

/// Set nonzero while the game is paused / in a menu; the actor's tick handler
/// is skipped in that state.
extern u8  D_801153F4;
extern s32 D_80115754;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_102400_80131E84(GsCOORDINATE2* arg0, s32 arg1);
s32  func_800E1B24(s32 arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102400_80131E24;
extern GpU16Pair             D_actor_102400_801363C4;

/// Spawn/setup handler (state 0): allocates the work block, attaches the
/// model 0x15E above the parent through the scratch stack, links the three
/// display nodes, arms the death timer and moves the task to state 1.
void func_actor_102400_801345B0(GpEnemy* arg0, Task* arg1)
{
    ActorsShared801351d4Parent*  parentWork;
    Task*                        parent;
    ActorsShared801351d4Work*    work;
    ActorsShared801351d4Scratch* scratch;
    void*                        head;
    GsCOORDINATE2*               objCoord;
    GsCOORDINATE2*               objCoord2;
    GsCOORDINATE2*               objCoord3;
    SVECTOR*                     offset;
    GsCOORDINATE2*               coord;
    GsCOORDINATE2*               parentCoord;

    head                = *(void**)0x1F8003FC;
    scratch             = (ActorsShared801351d4Scratch*)((u8*)head - 0x18);
    *(void**)0x1F8003FC = scratch;
    offset              = &scratch->offset;
    parent              = arg1->parent;
    coord               = ((TmdObject*)arg1->extra)->coords;
    parentCoord         = ((TmdObject*)parent->extra)->coords;
    parentWork          = (ActorsShared801351d4Parent*)parent->work;
    work                = memCalloc(0xB4, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->work         = (TaskIdMap*)work;
    scratch->offset.vx = 0;
    scratch->offset.vy = -0x15E;
    scratch->offset.vz = 0;
    gte_SetRotMatrix(&parentCoord->coord);
    gte_ldv0(offset);
    __asm__ volatile("nop; nop; .word 0x4A486012");
    gte_stlvnl(&scratch->result);
    coord->sub        = &gGfxViewCoord;
    coord->coord      = parentCoord->coord;
    coord->coord.t[0] = parentCoord->coord.t[0] + scratch->result.vx;
    coord->coord.t[1] = parentCoord->coord.t[1] + scratch->result.vy;
    coord->coord.t[2] = parentCoord->coord.t[2] + scratch->result.vz;
    coord->flg        = 0;
    work->field_A8    = parentCoord->coord.m[0][2];
    work->field_AA    = parentCoord->coord.m[1][2];
    work->field_AC    = parentCoord->coord.m[2][2];

    objCoord             = ((TmdObject*)arg1->extra)->coords;
    work->obj_0.ctx.recs = &work->rec_40;
    work->obj_0.pos.vx   = 0;
    work->obj_0.pos.vy   = 0;
    work->obj_0.pos.vz   = 0;
    work->obj_0.coord    = objCoord;
    work->obj_0.key      = Gp_PackPair(&D_actor_102400_801363C4, (parentWork->field_14E * 2) | 1);
    work->obj_0.radius   = 0xC8;
    work->obj_0.flags    = 1;
    Gp_LinkObj(3, &work->obj_0);
    Gp_InitRec18Table(&work->rec_40, 1, 0);
    work->obj_0.flags    |= 0x8000;
    objCoord2             = ((TmdObject*)arg1->extra)->coords;
    work->obj_20.ctx.recs = &work->rec_40;
    work->obj_20.pos.vx   = 0;
    work->obj_20.pos.vy   = 0;
    work->obj_20.pos.vz   = 0;
    work->obj_20.coord    = objCoord2;
    if (parentWork->field_14E == 0) {
        work->obj_20.key = 0x22D2D;
    } else {
        work->obj_20.key = 0x22E2E;
    }
    work->obj_20.radius = 0xC8;
    work->obj_20.flags  = 1;
    Gp_LinkObj(1, &work->obj_20);

    work->pose_78.field_C  = -0xD2;
    work->pose_78.field_10 = 1;
    work->pose_78.field_12 = 1;
    work->pose_78.field_0  = 0;
    work->pose_78.field_2  = 0;
    work->pose_78.field_4  = 0;
    work->pose_78.field_8  = 0;
    work->pose_78.field_A  = 0;
    work->pose_78.field_14 = &work->field_90;
    work->obj_20.flags    |= 0x8000;
    objCoord3              = ((TmdObject*)arg1->extra)->coords;
    work->obj_58.ctx.d4rec = &work->pose_78;
    work->obj_58.pos.vx    = 0;
    work->obj_58.pos.vy    = 0;
    work->obj_58.pos.vz    = 0;
    work->obj_58.key       = 0;
    work->obj_58.radius    = 0;
    work->obj_58.flags     = 3;
    work->obj_58.coord     = objCoord3;
    Gp_LinkObj(3, &work->obj_58);
    Gp_InitRec18Table(&work->field_90, 1, 0);
    work->field_B0      = 0x5A;
    work->obj_58.flags |= 0x4400;
    Task_DetachFromParent(arg1);
    arg1->state        = 1;
    *(u8**)0x1F8003FC += 0x18;
}

/// Per-frame tick of the state-1 handler: it walks the enemy along the step
/// held in the work block's `field_A8` / `field_AC`, and once the death timer
/// `field_B0` runs out (or the collision record `field_90` names an occupied
/// room slot) spawns the death effect and moves the task to state 2.
///
/// `arg0` is the enemy the dispatcher passes first; the task itself is `arg1`
/// (`GpEnemyTaskFunc`).
void func_actor_102400_80134910(GpEnemy* arg0, Task* arg1)
{
    ActorsShared801351d4Work* work;
    GsCOORDINATE2*            coord;
    GpRoomParamRec*           param;
    s32                       rec;
    s32                       spawn;
    u16                       timer;

    coord = ((TmdObject*)arg1->extra)->coords;
    work  = (ActorsShared801351d4Work*)arg1->work;
    spawn = 0;
    switch (D_801153F4) {
        case 1:
            func_actor_102400_80131E84(coord, 0x100);
            return;
        case 2:
            return;
        case 0:
        default:
            coord->coord.t[0] += (work->field_A8 * 0x19) >> 9;
            coord->coord.t[2] += (work->field_AC * 0x19) >> 9;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_actor_102400_80131E84(coord, 0x100);
            rec = work->field_90.key;
            if ((rec != 0) &&
                (Gp_RoomParamTables[gGameSession->at4.loc.stage - 1][gGameSession->at4.loc.area - 1]
                                   [func_800E1B24(rec)]
                                       ->field_1 == 0)) {
                spawn = 1;
            }
            Gp_ClearRec18Occupied(&work->field_90);
            timer          = work->field_B0 - 1;
            work->field_B0 = timer;
            if (((timer << 0x10) <= 0) || (work->rec_40.flags & 1) || (spawn != 0)) {
                Gp_SpawnEff(D_80115754, coord, 0, NULL);
                arg1->state    = 2;
                work->field_B2 = 0;
            }
            break;
    }
}

void func_actor_102400_80134AC4(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->field_4 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}

void func_actor_102400_80134BD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102400_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
