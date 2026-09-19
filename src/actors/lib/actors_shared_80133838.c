#include "common.h"

#include "actors/actors_shared_80133838.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

extern u8 D_801153F4;

void func_8017E524(s32 arg0);
void func_8017FD88(s32 arg0);

/// Teardown tick of an enemy whose block carries this body. It runs only while
/// the pause flag `D_801153F4` is clear, so the despawn countdown does not
/// advance during a cutscene or menu.
///
/// The first tick releases the enemy: the list node at +0x10 of the enemy comes
/// off, the work block itself comes off its own object list, the enemy's
/// `recs` slot is dropped, the parent's block supplies the sound id, and
/// one of the two per-enemy death flags is raised -- which of them depending on
/// the block's `field_46`. From then on the same counter destroys the enemy
/// once it reaches 0x3D.
///
/// Shared: `actor_105300` and `actor_105400` carry the same body byte for byte
/// and reference nothing overlay-local, so one object serves both.
void ActorsShared80133838(GpEnemy* arg0, Task* arg1)
{
    ActorsShared80133838Work* work;
    ActorsShared80133838Work* parentWork;
    u16                       timer;

    work       = (ActorsShared80133838Work*)arg1->work;
    parentWork = ((ActorsShared80133838Task*)arg1->parent)->field_1C;
    if (D_801153F4 == 0) {
        timer          = work->field_42 + 1;
        work->field_42 = timer;
        if ((s16)timer == 1) {
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj((GpObj*)work);
            arg0->recs = 0;
            SndEvt_EnqueueType7(parentWork->field_31C, 1);
            if (work->field_46 == 0) {
                func_8017FD88(0);
                GameFlag_SetNibble(0x147, 1);
            } else {
                func_8017E524(0);
                GameFlag_SetNibble(0x148, 1);
            }
        }
        if ((s16)work->field_42 >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
    }
}
