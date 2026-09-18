#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"
#include "actors/actors_shared_80132808.h"

/// Same body as `ActorsShared80132808`: re-aim one joint by `yaw` about Y in
/// world space and write the result back in its parent's frame.
void func_actor_341700_8016AC64(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    ActorsShared80132808_Accumulate(coord, rotation, &Gfx_ViewCoord);
    func_8004BFF8(yaw, rotation);
    out = ActorsShared80132808_Localize(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016AF70);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B2B8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B804);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016B9A8);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_27", func_actor_341700_8016C0F4);

extern void func_actor_341700_8016D2B8(GpEnemy* arg0, Task* arg1);
extern void func_actor_341700_8016D2E8(GpEnemy* arg0, Task* arg1);
extern void func_actor_341700_8016C0F4(GpEnemy* arg0, Task* arg1);

/// Three state handlers, indexed by `Actor341700SubWork::field_0`; copied onto
/// the stack before dispatch, as `D_actor_341700_80161F0C` is.
const GpEnemyTaskFuncTable3 D_actor_341700_80162058 = { {
    func_actor_341700_8016D2B8,
    func_actor_341700_8016D2E8,
    func_actor_341700_8016C0F4,
} };

/// Per-frame callback of the `func_actor_341700_8016D130` task. It colours the
/// model from the world position of its *second* attach coordinate and then,
/// unless `D_801153F4` hides the model, runs the handler `Actor341700SubWork::
/// field_0` names.
///
/// `case 0` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's tree tests `case 1` at the
/// root; dropping the case makes `case 2` the root and the emitted branches
/// come out with the wrong polarity and a stray low-bound test.
void func_actor_341700_8016CC9C(GpEnemy* arg0, Task* arg1)
{
    VECTOR                block;
    Actor341700SubWork*   work = (Actor341700SubWork*)arg1->work;
    GpEnemyTaskFuncTable3 sp   = D_actor_341700_80162058;

    ((TmdObject*)arg1->extra)->field_8[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->field_8[1]);
    block.vx = ((TmdObject*)arg1->extra)->field_8[1].workm.t[0];
    block.vy = ((TmdObject*)arg1->extra)->field_8[1].workm.t[1];
    block.vz = ((TmdObject*)arg1->extra)->field_8[1].workm.t[2];
    Gp_UpdateActorColor(arg0, &block, 0, 0);
    switch (D_801153F4) {
        case 2:
            ((TmdObject*)arg1->extra)->field_C |= 0x80;
            return;
        case 1:
            return;
        case 0:
        default:
            if (work->field_2 != work->field_0) {
                work->field_4 = 1;
            } else {
                work->field_4 = 0;
            }
            work->field_2 = work->field_0;
            sp.funcs[work->field_0](arg0, arg1);
            if (gGameSession->viewReady != 0) {
                ((TmdObject*)arg1->extra)->field_8->flg = 0;
            }
            return;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_341700/actor_341700_27", ActorsShared80135df4Table);

s32 func_actor_341700_8016CE28(Actor341700* arg0, s32 arg1, s32 arg2)
{
    TmdObject* obj = arg0->field_2C;

    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            break;
        case 1:
            obj->field_C = 0;
            Tmd_AllocBuffers(obj);
            break;
        case 2:
            obj->field_C |= 4;
            break;
        case 3:
            obj->field_C = 4;
            break;
    }
    return 0;
}

/// The `0x2704` command handler, reached through the task's `Task::msgTable`
/// table (`D_actor_341700_80175F5C`): the three leading bytes of `cmd` are
/// copied over `Actor341700SubWork::field_18` .. `field_1A` and the second
/// halfword, when the opcode matches, picks the state the work block moves to.
///
/// `case 2` is folded into `default` on purpose. The two bodies are the same,
/// so the case list keeps three nodes and GCC's decision tree balances around
/// `case 1`; dropping `case 2` makes `case 0` the root and the emitted branches
/// come out in a different order.
s32 func_actor_341700_8016CEB4(Task* task, s32 arg1, Actor341700Cmd* cmd)
{
    Actor341700SubWork* work = (Actor341700SubWork*)task->work;

    work->field_18 = cmd->bytes.field_0;
    work->field_19 = cmd->bytes.field_1;
    work->field_1A = cmd->bytes.field_2;

    if (cmd->halfs[0] == 0x2704) {
        switch (cmd->halfs[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 1:
                ((TmdObject*)task->extra)->field_8->flg = 0;
                work->field_0                           = 2;
                break;
            case 2:
            default:
                work->field_0 = 0;
                task->state   = 1;
                break;
        }
    }
    return 1;
}
