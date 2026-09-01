#ifndef ROOMS_DRYFIELD_SOUVENIR_SHOP_H
#define ROOMS_DRYFIELD_SOUVENIR_SHOP_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// One of the shop's drawn objects, reached through `DssWork::object`.
///
/// `func_dryfield_souvenir_shop_8017D6B4` loads `coord.m` into the GTE rotation
/// registers (`ctc2` 0..4) to transform the quad's vertices, and adds
/// `coord.t` component-wise onto the emitted primitive's coordinates, so the
/// `MATRIX` at 0x24 is this object's world transform. Fields before it, and the
/// object's total size, are still unknown.
typedef struct DssShopObject {
    /* 0x00 */ byte   pad_0[0x24];
    /* 0x24 */ MATRIX coord;
} DssShopObject;

/// Work block hung off `Task::extra` (0x2C) for the souvenir-shop task family.
/// Only the object pointer is known so far.
typedef struct DssWork {
    /* 0x00 */ byte           pad_0[0x8];
    /* 0x08 */ DssShopObject* object;
} DssWork;

/// Draws four of the quad's vertices starting at `vertexBase`.
void func_dryfield_souvenir_shop_8017D6B4(DssShopObject* object, s16 vertexBase);

#endif // ROOMS_DRYFIELD_SOUVENIR_SHOP_H
