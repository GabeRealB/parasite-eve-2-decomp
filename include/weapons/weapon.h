#ifndef WEAPONS_WEAPON_H
#define WEAPONS_WEAPON_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"

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

#endif /* WEAPONS_WEAPON_H */
