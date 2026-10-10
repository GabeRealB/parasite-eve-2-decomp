/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in shop.h. */

static void _shopPurchaseQuantityTask(Task* task);

static void _shopItemPreviewTask(Task* task);

static void _shopRefillWeaponSupplyTask(Task* task);

/// Bodyless priority-192 recipe for the free Batteries/Fuel service panel.
///
/// Requires at least one carried rechargeable weapon. The active item-list
/// child receives a zero payload and scans all carried supplies, committing
/// each full load before animating its display. The callback borrows the
/// carrier's refill storage and gameplay catalogue through closure. Bounds are
/// screen-centered pixels; spawning copies the recipe, so only callback code
/// and borrowed state must stay loaded until the panel ends.
static const UiObjectDesc _gShopWeaponSupplyRefillDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -72, -36, 144, 64 },
    .otIndexSeed     = 32,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopRefillWeaponSupplyTask,
};

/// Bodyless priority-192 panel recipe for the selected item's preview and amount.
///
/// Bounds are pixels relative to the screen center. The item-list task spawns
/// it without input, with its list panel as parent; the content callback reads
/// the live selection and loaded item. Spawning copies this recipe, retaining
/// only callback code, which must stay loaded until the child panel closes.
static const UiObjectDesc _gShopItemPreviewDescriptor SECTION(".data") = {
    .style           = 0,
    .bounds          = { 48, -93, 96, 97 },
    .otIndexSeed     = 44,
    .field_E         = 0,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopItemPreviewTask,
    .taskDataValue   = 0,
};

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
