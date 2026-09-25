#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"
#include "weapons/weapon.h"

/// The P08, the P08 with the snail magazine and the Mongoose are this source
/// built once each, and each declares these values in the manifest.
///
/// `WEAPON_ID` is the weapon's index (4, 1 and 9), which keys the firing sound
/// and the item. `P08_FLASH_EFFECT` is the muzzle-flash effect spawned per shot
/// and `P08_FLASH_WEAPON` the weapon index it is handed - 1 for both P08s, the
/// Mongoose its own. `P08_FIELD_940` is what `field_940` is set to when the
/// firing pose ends.
#if !defined(WEAPON_ID) || !defined(P08_FLASH_EFFECT) || !defined(P08_FLASH_WEAPON) || !defined(P08_FIELD_940)
#error "WEAPON_ID, P08_FLASH_EFFECT, P08_FLASH_WEAPON and P08_FIELD_940 are per-package build parameters"
#endif

void func_p08_8011D1D8(Task* arg0)
{
    GameActor* actor;
    GpCoord*   coord;
    GpCoord*   spot;
    s32        anim;

    SCRATCH_PUSH_BYTES(0x50);
    spot  = SCRATCH_HEAD(GpCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->field_95E) {
        case 0:
            actor->field_956 = 4;
            actor->field_95E = 1;
            anim             = 1;
            actor->field_954 = 0;
            actor->field_95A = 0;
            actor->field_95C = 0;
            actor->field_981 = 0;
            actor->field_979 = 0xB;
            if (((u16)actor->field_958 | actor->field_975) != 0) {
                anim = 5;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->field_958 = 0;
            break;
        case 1:
            if (Gp_AnimGetRec((GpAnimCtx*)actor->field_424, actor->field_438 + 1) !=
                NULL) {
                actor->field_95E++;
            }
            break;
        case 2:
            actor->field_95E++;
            func_80106238(arg0, 0, 0);
            actor->field_12A |= 0xCC00;
            Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
            Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000004 | (WEAPON_ID << 16), 1);
            Gp_SpawnEff(P08_FLASH_EFFECT,
                        actor->field_91C->extra.tmd->coords,
                        P08_FLASH_WEAPON, NULL);
            Gp_AnimResetChildSlots(arg0, 0xA);
            break;
        case 3:
            actor->field_95E++;
            actor->field_12A &= 0x3FFF;
            if (Gp_PickNearestRec18(actor->field_32C, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            /* fallthrough */
        case 4:
            if (actor->field_979 != 0) {
                actor->field_979--;
            }
            if (func_80105894(arg0, D_80112E04[Mc_SaveData.characterId][1], 0, 0) == 0 ||
                ((actor->field_962 & actor->field_96A) != 0 && actor->field_979 == 0)) {
                actor->field_940 = P08_FIELD_940;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_POP_BYTES(0x50);
}
