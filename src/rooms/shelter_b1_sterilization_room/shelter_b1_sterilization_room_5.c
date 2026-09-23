#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/display.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
extern TaskDesc  D_shelter_b1_sterilization_room_80188504;
extern s32       D_shelter_b1_sterilization_room_8018C340;
extern s16       D_80073BA0;
extern u8        D_801153F4;
extern s32       D_shelter_b1_sterilization_room_80188590;
extern s32       D_shelter_b1_sterilization_room_80188624;
extern GpU16Pair D_shelter_b1_sterilization_room_80188738;

/// Redraw the room's two backdrop halves as semi-transparent `SPRT`s in OT
/// slot 8, tinting both with `shade`, then append each half's tpage.
void RoomsShared8017e144(s32 shade)
{
    SPRT* p;

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = -0xA0;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0xC0;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    Room_Draw42(0x340, 0);

    p              = (SPRT*)gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setSprt(p);
    setSemiTrans(p, 1);
    p->r0   = shade;
    p->g0   = shade;
    p->b0   = shade;
    p->u0   = 0;
    p->v0   = 0;
    p->x0   = 0x20;
    p->y0   = -0x78;
    p->clut = 0;
    p->w    = 0x80;
    p->h    = 0xF0;
    addPrim(gGpuCurrentOt + 8, p);
    Room_Draw42(0x180, 0x100);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_sterilization_room/shelter_b1_sterilization_room_5", func_shelter_b1_sterilization_room_80180D74);

void func_shelter_b1_sterilization_room_80180F74(Task* task)
{
    Task*          player;
    GsCOORDINATE2* coord;
    s32            pan;
    GpStateC08*    st;

    switch (task->state) {
        case 0:
            gGameSession->restartMode = 2;
            task->state++;
            return;
        case 1:
            if (GameFlag_GetNibble(0x77) == 0) {
                if (gGameSession->eventState == 0 && D_801153F4 == 0) {
                    player = gameGetPtrSlot(3);
                    task->killCountdown++;
                    if (task->killCountdown == 0x78) {
                        Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_shelter_b1_sterilization_room_80188738, 0), 0);
                    } else if (task->killCountdown >= 0x79) {
                        if (D_80073BA0 > 0) {
                            coord = ((TmdObject*)player->extra)->coords;
                            Gp_DispatchMsg(player, 0x3F7, (s32)&D_shelter_b1_sterilization_room_80188590, 0);
                            Gp_PlayerWeaponId(&D_shelter_b1_sterilization_room_80188624);
                            Gp_DispatchMsg(player, 0x3E8, (s32)&D_shelter_b1_sterilization_room_80188624, 0);
                            pan = (s8)Gp_GetObjPan(coord);
                            SndEvt_EnqueueType6(0x54100011, pan, (s8)gpGetObjDepth(coord));
                            task->killCountdown = 0;
                        }
                        st           = &Gp_StateC08;
                        st->field_6 |= 1;
                    } else if (task->killCountdown == 0x49) {
                        Gp_MsgPlayerWeapon(1);
                    }
                    task->spawnArg1 = 0;
                    return;
                }
                if (task->spawnArg1 == 0) {
                    SndEvt_EnqueueType7(0x54100011, 1);
                    task->spawnArg1 = 1;
                }
                return;
            }
            SndEvt_EnqueueType7(0x54100011, 1);
        default:
            taskKill(task);
            break;
    }
}

void func_shelter_b1_sterilization_room_8018118C(s32 arg0)
{
    if (!(((s32)D_shelter_b1_sterilization_room_8018C340 >> arg0) & 1)) {
        D_shelter_b1_sterilization_room_8018C340 |= 1 << arg0;
        Task_SpawnFromTable(&D_shelter_b1_sterilization_room_80188504, arg0, 0, 0);
    }
}
