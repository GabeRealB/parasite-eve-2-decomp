#include "common.h"
#include "actors/actor_102500.h"
#include "main/wipsys.h"

extern s32 Gp_LcgState;
void       Gp_SetLightMode(Actor02500Ctx* ctx, s32 mode);
void*      Gp_SpawnEff(s32 id, GsCOORDINATE2* coord, s32 flags, void* offset);

void Actor02500_Fn012F0(Actor02500* actor)
{
    Actor02500Obj2C*      obj;
    Actor02500Obj2C*      loadedObj;
    Actor02500Work*       work;
    GsCOORDINATE2*        coord;
    s16                   timer2;
    s16                   timer3;
    s16                   timer4;
    s16                   effectTimer;
    s16                   state;
    s32                   sound;
    s32                   radius;
    s32                   dx;
    s32                   dz;
    s32                   index;
    s32                   i;
    s32                   pan;
    u32                   random;
    Actor02500RotScratch* scratch;
    Actor02500OffsetPair* pair;

    scratch   = (Actor02500RotScratch*)(*(u32*)0x1F8003FC -= 0x18);
    loadedObj = actor->field_2C;
    __asm__("" : "+r"(loadedObj) : : "v0");
    work = actor->field_1C;
    __asm__ volatile("addu %0,%1,$0" : "=r"(obj) : "r"(loadedObj), "r"(work));
    state = work->field_324;
    coord = obj->field_8;
    switch (state) {
        case 0:
            obj->field_C                  = 0x80;
            actor->field_20->node.field_4 = 1;
            dx                            = Player_Status.coordMtx->t[0] - work->field_314;
            scratch->delta.vy             = 0;
            scratch->delta.vx             = dx;
            dz                            = Player_Status.coordMtx->t[2] - work->field_318;
            scratch->delta.vz             = dz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0 || Gp_StateF0.field_21 != 0 || Gp_StateF0.field_8 != 0) {
                D_80115410[1]   = 1;
                work->field_324 = 2;
                work->field_32E = ((u16)actor->field_20->field_8 >> 0xC) * 0xA;
            }
            break;
        case 1:
            obj->field_C                  = 0x80;
            actor->field_20->node.field_4 = 1;
            if (Gp_StateF0.field_21 != 0 || Gp_StateF0.field_8 != 0) {
                work->field_324 = 2;
                work->field_32E = ((u16)actor->field_20->field_8 >> 0xC) * 0xA;
            }
            break;
        case 2:
            obj->field_C                  = 0x80;
            actor->field_20->node.field_4 = 1;
            timer2                        = (u16)work->field_32E - 1;
            work->field_32E               = timer2;
            if (timer2 <= 0) {
                work->field_324 = 3;
                work->field_32E = 0xA;
                work->field_330 = 0x14;
                sound           = (((u16)actor->field_20->field_8 >> 0xC) << 8) | 0x40190003;
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)Gp_GetObjDepth(coord));
            }
            break;
        case 3:
            timer3          = (u16)work->field_32E - 1;
            work->field_32E = timer3;
            if (timer3 > 0) {
                obj->field_C = 0x80;
            } else {
                Gp_SetLightMode(actor->field_20, 0);
                obj->field_C              = (u16)obj->field_C | 2;
                work->obj1A4.flags       |= 0x8000;
                actor->field_20->field_54 = work->field_1C4;
                work->field_31C           = 0xA;
                work->field_32E           = 0;
                work->field_324           = 4;
            }
            break;
        case 4:
            timer4          = (u16)work->field_32E + 1;
            work->field_32E = timer4;
            if (timer4 < 0x10) {
                obj->field_C = (u16)obj->field_C | 2;
            }
            if (work->field_32E >= 0x1F) {
                work->field_322 = 1;
                work->field_324 = 0;
                Gp_ArmStateF0(1);
            }
            break;
    }
    if (work->field_330 != 0) {
        effectTimer     = (u16)work->field_330 - 1;
        work->field_330 = effectTimer;
        if (!(effectTimer & 3)) {
            random      = (Gp_LcgState * 5) + 0x71357911;
            i           = 0;
            radius      = ((random >> 0x10) & 0x3F) + 0x12C;
            Gp_LcgState = (s32)random;
            index       = (((u16)work->field_330 >> 2) ^ 1) & 1;
            for (; i < 4; i++) {
                pair            = &Actor02500_D05BE8[index];
                scratch->rot.vx = (s16)((s32)(pair->x * radius) >> 0xC);
                scratch->rot.vy = 0;
                scratch->rot.vz = (s16)((s32)(pair->z * radius) >> 0xC);
                Gp_SpawnEff(0x60054, actor->field_2C->field_8, 0x80002400, &scratch->rot);
                index += 2;
                SOFT_TOUCH_REG_USE(index, radius);
            }
        }
    }
    *(u32*)0x1F8003FC += 0x18;
}
