#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/wipsys.h"

void func_grenade_pistol_8011D1D4(GpActorWork* arg0)
{
    GameActor* actor;
    s32        anim;

    actor = arg0->actor;
    switch (actor->field_95E) {
        case 0:
            anim              = 1;
            actor->field_956  = 4;
            actor->field_954  = 0;
            actor->field_95A  = 0;
            actor->field_95C  = 0;
            actor->field_95E += anim;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 8;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->field_958 = 0;
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, (GpAnimSlot*)actor->pad_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
            actor->field_95E = 3;
            actor->field_981 = 0;
            actor->field_940 = 0x28;
            Gp_PlayObjSfx((GpObj38*)arg0->extra->field_8,
                          ((Wip_SysConfig.field_22 - 0xA) << 24) | 0x200B0004, 1);
            Gp_SpawnEff(0x6006C,
                        (GsCOORDINATE2*)((GameActorExt*)actor->field_91C->extra)->field_8, 0xB,
                        NULL);
            Gp_ConsumeSlotQty(0x8A, 1);
            func_80104490(arg0, 0, 1, Wip_SysConfig.field_22 | 0xB00);
            Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 3);
            break;
        case 3:
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.field_22][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
}

INCLUDE_ASM("weapons/nonmatchings/grenade_pistol/grenade_pistol", func_grenade_pistol_8011D3A0);

INCLUDE_ASM("weapons/nonmatchings/grenade_pistol/grenade_pistol", func_grenade_pistol_8011D6FC);

INCLUDE_RODATA("weapons/nonmatchings/grenade_pistol/grenade_pistol", D_grenade_pistol_8011D1C0);
