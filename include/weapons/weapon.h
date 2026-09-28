#ifndef WEAPONS_WEAPON_H
#define WEAPONS_WEAPON_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/actor.h"
#include "gameplay/geometry.h"

/// The item a weapon's rounds are taken from, given the weapon's index. Weapon
/// items follow item 0x7F in weapon order, so every weapon consumes item
/// `index + 0x7F`.
#define WEAPON_ITEM(index) ((index) + 0x7F)

/// Work block of a fired grenade, allocated zeroed by the projectile's spawn
/// state and kept at `Task::work`. The weapon overlays that fire grenades and
/// Kyle's thrown-object task carry the same code. It leads with the two
/// `GpObj` list nodes the exit callback hands back to `Gp_UnlinkObj`: `obj`
/// is a node whose `ctx.recs` is `rec0` directly, and `obj2` a node whose
/// `ctx.d4rec` is `d4rec`, reaching `rec1` through that shape's `recs`.
/// `field_88` is 16.16: the whole word is the flight timer the flight state
/// counts, and its high half the per-frame divisor the grenade's step along
/// `dir` is taken by, so the grenade slows as the timer runs. `dir` is the
/// launch direction: the muzzle's forward column pitched up and normalised.
typedef struct WeaponGrenadeWork {
    GpObj        obj;
    GpObj        obj2;
    GpRec18      rec0[1];
    GpRec18      rec1[1];
    GpActorD4Rec d4rec;
    GpFixed16    field_88;
    s32          field_8C;
    s32          field_90;
    SVECTOR      dir;
    byte         pad_9C[4];
} WeaponGrenadeWork;
STATIC_ASSERT_SIZEOF(WeaponGrenadeWork, 0xA0);

/// The scratch-pad block a grenade's flight state takes when the attachment
/// id comes from the task's spawn argument. `delta` is handed to
/// `func_800E0FEC` and then holds the per-frame translation added onto the
/// projectile coordinate; `field_30` keeps the byte of the spawn argument
/// above the attachment id, which seeds the sound bank, and `sfx` is the
/// attachment id itself.
typedef struct WeaponGrenadeScratch {
    byte           pad_0[0x20];
    GpDeltaScratch delta;
    s32            field_30;
    s32            sfx;
} WeaponGrenadeScratch;
STATIC_ASSERT_SIZEOF(WeaponGrenadeScratch, 0x38);

/// Name of a public symbol in a source several packages are built from: the
/// package's SLOT_PREFIX (declared in the overlay manifest) in place of a fixed
/// package name, so each build exports its own. `SLOT_FUNC(8011DBD0)` is
/// `func_mm1_8011DBD0` in the MM1's build.
#define SLOT_FUNC(addr)           SLOT_FUNC_(SLOT_PREFIX, addr)
#define SLOT_FUNC_(prefix, addr)  SLOT_FUNC__(prefix, addr)
#define SLOT_FUNC__(prefix, addr) func_##prefix##_##addr

/// Task entries the resident task descriptor tables name. A table in main or
/// gameplay reaches each of these by name, so they are the family's interface
/// to the resident code.
void func_tonfa_baton_8011DB98(Task* arg0);
void func_grenade_pistol_8011DBD0(Task* arg0);
void func_m4a1_grenade_8011DE68(Task* arg0);
void func_hypervelocity_8011F6C0(Task* arg0);
void func_mm1_8011DBD0(Task* arg0);

/// Models those descriptors attach.
extern TmdSource D_unused_85_8011D53C;
extern TmdSource D_p08_8011D924;
extern TmdSource D_mongoose_8011D934;
extern TmdSource D_pa3_8011DA04;
extern TmdSource D_m93r_8011DA74;
extern TmdSource D_m950_8011DA9C;
extern TmdSource D_sp12_8011DB44;
extern TmdSource D_as12_8011DCF0;
extern TmdSource D_m249_8011DE74;
extern TmdSource D_m4a1_8011DEC4;
extern TmdSource D_grenade_pistol_8011E28C;
extern TmdSource D_tonfa_baton_8011E460;
extern TmdSource D_mm1_8011E494;
extern TmdSource D_p229_8011E5E4;
extern TmdSource D_tonfa_baton_8011E5EC;
extern TmdSource D_m4a1_bayonet_8011E9FC;
extern TmdSource D_m4a1_grenade_8011EA2C;
extern TmdSource D_mp5a5_8011EAFC;
extern TmdSource D_gunblade_8011EEB0;
extern TmdSource D_m4a1_pyke_8011F56C;
extern TmdSource D_m4a1_hammer_8011F778;
extern TmdSource D_hypervelocity_801202F8;
extern TmdSource D_hypervelocity_801205E4;
extern TmdSource D_m4a1_javelin_8012071C;
extern TmdSource D_hypervelocity_80120860;
/// Model arguments of descriptors that point into the zeroed tail of their
/// package, where the disc image holds no model record.
extern TmdSource D_grenade_pistol_8012B5A4;
extern TmdSource D_mm1_8012D444;
extern TmdSource D_m4a1_grenade_8012E1FC;

#endif /* WEAPONS_WEAPON_H */
