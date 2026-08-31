#ifndef WEAPONS_SHARED_8011DE24_H
#define WEAPONS_SHARED_8011DE24_H

#include "common.h"

/// 0x20-byte `GpObj` list node. Two of them lead the weapon's work block and
/// are unlinked together when its task tears down.
typedef struct WeaponShared8011de24Obj {
    /* 0x00 */ byte pad_0[0x20];
} WeaponShared8011de24Obj;

typedef struct WeaponShared8011de24Work {
    /* 0x00 */ WeaponShared8011de24Obj field_0;
    /* 0x20 */ WeaponShared8011de24Obj field_20;
} WeaponShared8011de24Work;

/// The overlay's view of the owning `Task`: only the work pointer is known.
typedef struct WeaponShared8011de24 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ WeaponShared8011de24Work* field_1C;
} WeaponShared8011de24;

void WeaponsShared8011de24(WeaponShared8011de24* arg0);

#endif
