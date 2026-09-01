#include "common.h"

#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_dryfield_night_souvenir_shop_8017D6B4(GsCOORDINATE2* coord, s32 arg1);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_souvenir_shop/dryfield_night_souvenir_shop", func_dryfield_night_souvenir_shop_8017D610);

void func_dryfield_night_souvenir_shop_8017D654(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_souvenir_shop/dryfield_night_souvenir_shop", D_dryfield_night_souvenir_shop_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_souvenir_shop/dryfield_night_souvenir_shop", func_dryfield_night_souvenir_shop_8017D65C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_souvenir_shop/dryfield_night_souvenir_shop", func_dryfield_night_souvenir_shop_8017D6B4);

void func_dryfield_night_souvenir_shop_8017DFF4(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    Gp_UpdateCoord(coord);
    func_dryfield_night_souvenir_shop_8017D6B4(coord, 0);
    func_dryfield_night_souvenir_shop_8017D6B4(coord, 8);
}
