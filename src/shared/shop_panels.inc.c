/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in shop.h. */

static void _shopPurchaseQuantityTask(Task* task);

static void _shopItemPreviewTask(Task* task);

static void _shopRefillWeaponSupplyTask(Task* task);

static UiObjectDesc Shop_Data_80181BD8 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -72, -36, 144, 64 }, 32, 0, TASK_BODY_NONE, 192, _shopRefillWeaponSupplyTask, 0 };

static UiObjectDesc Shop_Data_80181BF4 = { 0, { 48, -93, 96, 97 }, 44, 0, TASK_BODY_NONE, 192, _shopItemPreviewTask, 0 };

enum { SHOP_PURCHASE_QUANTITY_PANEL_STYLE = 3 };

/// Bodyless panel recipe for choosing and buying ammunition packs.
///
/// The caller supplies an affordable catalogue item id and its purchase prompt
/// as parent. The content task starts at one pack, caps selection by BP and
/// carried capacity, and grants the chosen packs on confirmation. Bounds are
/// initial screen-centered pixels; the task fits three text rows and the caller
/// positions the panel beside the selected Purchase row. Spawning copies the
/// recipe; its callback requires the room image to stay loaded.
static const UiObjectDesc _gShopPurchaseQuantityDescriptor SECTION(".data") = {
    .style           = SHOP_PURCHASE_QUANTITY_PANEL_STYLE,
    .bounds          = { -72, -32, 184, 48 },
    .otIndexSeed     = 16,
    .field_E         = 0,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopPurchaseQuantityTask,
    .taskDataValue   = 0,
};
