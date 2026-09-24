#include "common.h"

/// The room's active data bank pointer, and the two banks it chooses between.
extern void* D_mist_shooting_gallery_801853C0;
extern u32   D_mist_shooting_gallery_8018D1B4[];
extern u32   D_mist_shooting_gallery_8018DF38[];

/// Publishes one of the room's two data banks as the active one: bank 0 for a
/// zero argument, bank 1 otherwise.
void func_mist_shooting_gallery_801811C0(s16 arg0)
{
    if (arg0 == 0) {
        D_mist_shooting_gallery_801853C0 = D_mist_shooting_gallery_8018D1B4;
        return;
    }
    D_mist_shooting_gallery_801853C0 = D_mist_shooting_gallery_8018DF38;
}
