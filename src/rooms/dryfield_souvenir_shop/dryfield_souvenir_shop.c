#include "common.h"

#include "rooms/dryfield_souvenir_shop.h"

void func_dryfield_souvenir_shop_8017D654(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_souvenir_shop/dryfield_souvenir_shop", D_dryfield_souvenir_shop_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_souvenir_shop/dryfield_souvenir_shop", func_dryfield_souvenir_shop_8017D65C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_souvenir_shop/dryfield_souvenir_shop", func_dryfield_souvenir_shop_8017D6B4);

void func_dryfield_souvenir_shop_8017DFD4(Task* task)
{
    DssShopObject* object = ((DssWork*)task->extra)->object;

    func_dryfield_souvenir_shop_8017D6B4(object, 0);
    func_dryfield_souvenir_shop_8017D6B4(object, 8);
}
