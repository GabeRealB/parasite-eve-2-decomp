#ifndef WEAPONS_SHARED_8011E4AC_H
#define WEAPONS_SHARED_8011E4AC_H

#include "common.h"

/// 0x20-byte `GpObj` list node hanging off the weapon task's work pointer.
typedef struct WeaponShared8011e4acObj {
    /* 0x00 */ byte pad_0[0x20];
} WeaponShared8011e4acObj;

/// The overlay's view of the owning `Task`: the list node it linked and the
/// `Gp_State1C` block it allocated are the only two fields this teardown
/// touches.
typedef struct WeaponShared8011e4ac {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ WeaponShared8011e4acObj* field_1C;
    /* 0x20 */ void*                    field_20;
} WeaponShared8011e4ac;

void WeaponsShared8011e4ac(WeaponShared8011e4ac* arg0);

#endif
