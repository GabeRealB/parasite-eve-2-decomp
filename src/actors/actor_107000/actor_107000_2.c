#include "common.h"
#include "actors/actor_107000.h"
#include "actors/actor_107000_anim.h"
#include "actors/actors_shared_801673f8.h"
#include "actors/actors_shared_8013454c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
void              func_actor_107000_801334C8(Task* arg0, u8 arg1);
void              func_actor_107000_80132E9C(Task* arg0);
extern GpU16Pair  D_actor_107000_80138744;
extern GpPairSrcE D_actor_107000_80138748;
extern u8         D_actor_107000_80139E50[];
extern u8         D_actor_107000_80139E78[];
extern SVECTOR    D_actor_107000_80139E90;

s32 func_actor_107000_80133E18(Task* arg0, s32 arg1, Actor107000Msg* arg2)
{
    Actor107000Work* work;
    Actor107000Ctx*  ctx;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          rot;
    u16              word;
    s16              heading;
    s32              magnitude;
    s32              mode;
    s32              state;
    s32              sound;
    s32              pan;

    obj   = (TmdObject*)arg0->extra;
    ctx   = arg0->spawnArg2;
    state = arg0->state;
    work  = (Actor107000Work*)arg0->work;
    coord = obj->coords;
    if (state == 1) {
        mode = arg2->field_2;
        if (mode == 4) {
            Gp_SpawnEff(0x60080, coord, 0x400, &D_actor_107000_80139E90);
            work->field_2B8 = 1;
            Actor107000_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2B2 = 4;
            return 0;
        }
        if (mode == 5) {
            Gp_SpawnEff(0x60080, coord, 0x400, &D_actor_107000_80139E90);
            work->field_2B8 = 1;
            Actor107000_TickAnim(arg0);
            work->field_2BC = 0;
            work->field_2D4 = 0;
            work->field_2B2 = 4;
            return 0;
        }
    }
    word = arg2->field_2 & 0xFF;
    if ((word & 0xFF) == 1) {
        if ((u32)(arg0->state - 1) >= 2U) {
            if (gGameSession->at4.loc.area == 0x27) {
                rot.vx            = 0;
                rot.vy            = D_8018B74C[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_8018B74C[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_8018B74C[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_8018B74C[arg2->field_2 >> 8].z;
                sound             = (((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x54270006);
                pan               = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, pan, (s8)gpGetObjDepth(coord));
            } else if (gGameSession->at4.loc.area == 0x28) {
                rot.vx            = 0;
                rot.vy            = D_801874C4[arg2->field_2 >> 8].heading;
                rot.vz            = 0;
                coord->coord.t[0] = D_801874C4[arg2->field_2 >> 8].x;
                coord->coord.t[1] = D_801874C4[arg2->field_2 >> 8].y;
                coord->coord.t[2] = D_801874C4[arg2->field_2 >> 8].z;
            }
            heading         = rot.vy;
            work->field_2B0 = heading;
            magnitude       = heading >= 0 ? heading : -heading;
            if (magnitude >= 0x801) {
                if (heading >= 0x801) {
                    work->field_2B0 = heading - 0x1000;
                } else if (heading < -0x800) {
                    work->field_2B0 = heading + 0x1000;
                }
            }
            Tmd_AllocBuffers((TmdObject*)arg0->extra);
            ((TmdObject*)arg0->extra)->flags &= 0xFF7F;
            ((TmdObject*)arg0->extra)->flags &= 0xFFFB;
            ctx->field_14                     = 0;
            work->field_11A                  |= 0x8000;
            work->field_152                  |= 0xC200;
            RotMatrix(&rot, &coord->coord);
            work->field_2BE                        = 0xC8;
            work->field_2E2                        = 1;
            work->field_2DE                        = 0x64;
            work->field_2E0                        = 0;
            work->field_2B2                        = 1;
            work->field_2C8                        = 1;
            ((TmdObject*)arg0->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        }
        return 0;
    }
    if ((word & 0xFF) == 3) {
        ((TmdObject*)arg0->extra)->flags |= 0x80;
        ((TmdObject*)arg0->extra)->flags |= 4;
        ctx->field_14                     = 1;
        work->field_11A                  &= 0x7FFF;
        work->field_152                  &= 0x3DFF;
        rot.vz                            = 0;
        rot.vy                            = 0;
        rot.vx                            = 0;
        RotMatrix(&rot, &coord->coord);
        coord->coord.t[2]                      = 0;
        coord->coord.t[1]                      = 0;
        coord->coord.t[0]                      = 0;
        ((TmdObject*)arg0->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)arg0->extra)->coords);
        arg0->state     = 3;
        work->field_2E2 = 0;
        work->field_2B2 = 0;
        work->field_2C8 = 0;
    }
    return 0;
}
