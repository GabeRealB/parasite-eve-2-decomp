#include "common.h"

#include "actors/actor_323400.h"
#include "gameplay/3CD8.h"
#include "main/sound.h"

INCLUDE_ASM("actors/nonmatchings/actor_323400/actor_323400_2", func_actor_323400_80163448);

INCLUDE_ASM("actors/nonmatchings/actor_323400/actor_323400_2", func_actor_323400_80163B58);

INCLUDE_ASM("actors/nonmatchings/actor_323400/actor_323400_2", func_actor_323400_80163FC8);

void func_actor_323400_801641C4(GpEnemy* enemy, Task* task)
{
    Actor323400Work* work;
    TmdObject*       obj;
    s32              id;
    s32              pan;
    SVECTOR          ofs2;
    SVECTOR          ofs;

    work = (Actor323400Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323400_80163B58(task);
        return;
    }
    switch (++work->field_6) {
        case 9: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002400, p);
            }
            break;
        }
        case 10: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002400, p);
            }
            id  = ((enemy->placeKey >> 12) << 8) | 0x4001000E;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
            break;
        }
        case 12: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80003600, p);
            }
            break;
        }
        case 13: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80004500, p);
            }
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002480, p);
            }
            ofs2.vy = 0x3E8;
            ofs2.vx = 0;
            ofs2.vz = -0x12C;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[1], 0x80005900, &ofs2);
            break;
        }
    }
    func_actor_323400_80163B58(task);
    ((TmdObject*)task->extra)->coords->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323400/actor_323400_2", func_actor_323400_801644C4);

void func_actor_323400_8016475C(void)
{
}
