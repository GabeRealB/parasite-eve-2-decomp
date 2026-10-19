/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in shop.h. */

static void _shopItemListTask(Task* task);

static void _shopPurchasePromptTask(Task* task);

static void _shopDrawPassRow(UiList* list, UiObject* object);

static void _shopNoticeTask(Task* task);

static void _shopDrawPurchaseRow(UiList* list, UiObject* object);

static void _shopBalancePanelTask(Task* task);

static void _shopCategoryListTask(Task* task);

static void _shopDrawCategoryRow(UiList* list, UiObject* object);

static void _shopDrawItemRow(UiList* list, UiObject* object);

/// Item catalog keys used by these base stock lists.
///
/// Stock rows keep their unsigned halfword representation; service and end
/// rows use the separate `SHOP_ROW_*` values.
enum {
    SHOP_STOCK_ITEM_RECOVERY_1       = 1,
    SHOP_STOCK_ITEM_RECOVERY_2       = 2,
    SHOP_STOCK_ITEM_RECOVERY_3       = 3,
    SHOP_STOCK_ITEM_STIM             = 4,
    SHOP_STOCK_ITEM_MP_BOOST_1       = 6,
    SHOP_STOCK_ITEM_MP_BOOST_2       = 7,
    SHOP_STOCK_ITEM_PENICILLIN       = 8,
    SHOP_ITEM_SMG_CLIP_HOLDER        = 9,
    SHOP_ITEM_RIFLE_CLIP_HOLDER      = 10,
    SHOP_STOCK_ITEM_LIPSTICK         = 11,
    SHOP_ITEM_BELT_POUCH             = 13,
    SHOP_STOCK_ITEM_MEDICINE_WHEEL   = 55,
    SHOP_STOCK_ITEM_FLARE            = 58,
    SHOP_STOCK_ITEM_PEPPER_SPRAY     = 59,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE  = 60,
    SHOP_STOCK_ITEM_COMBAT_LIGHT     = 65,
    SHOP_ITEM_HAMMER                 = 66,
    SHOP_ITEM_PYKE                   = 67,
    SHOP_ITEM_JAVELIN                = 68,
    SHOP_ITEM_M203                   = 69,
    SHOP_ITEM_M9                     = 70,
    SHOP_STOCK_ITEM_TACTICAL_ARMOR   = 97,
    SHOP_STOCK_ITEM_COMBAT_ARMOR     = 98,
    SHOP_STOCK_ITEM_PASGT_VEST       = 100,
    SHOP_STOCK_ITEM_EOD_SUIT         = 102,
    SHOP_STOCK_ITEM_TURTLE_VEST      = 103,
    SHOP_STOCK_ITEM_AYA_SPECIAL      = 107,
    SHOP_STOCK_ITEM_NBC_SUIT         = 105,
    SHOP_STOCK_ITEM_PSY_SUIT         = 106,
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER = 108,
    SHOP_STOCK_ITEM_P08              = 131,
    SHOP_STOCK_ITEM_P229             = 132,
    SHOP_ITEM_GRENADE_PISTOL         = 138,
    SHOP_STOCK_ITEM_MM1              = 139,
    SHOP_STOCK_ITEM_PA3              = 140,
    SHOP_STOCK_ITEM_AS12             = 142,
    SHOP_STOCK_ITEM_M4A1_RIFLE       = 143,
    SHOP_STOCK_ITEM_M249             = 144,
    SHOP_ITEM_MP5A5_FIRST            = 157,
    SHOP_STOCK_ITEM_9MM_PB           = 160,
    SHOP_STOCK_ITEM_9MM_HYDRA        = 161,
    SHOP_STOCK_ITEM_9MM_SPARTAN      = 162,
    SHOP_STOCK_ITEM_44_MAGNUM        = 166,
    SHOP_STOCK_ITEM_GRENADE          = 169,
    SHOP_STOCK_ITEM_AIRBURST         = 170,
    SHOP_STOCK_ITEM_RIOT             = 171,
    SHOP_STOCK_ITEM_BUCKSHOT         = 172,
    SHOP_STOCK_ITEM_FIREFLY          = 173,
    SHOP_STOCK_ITEM_R_SLUG           = 174,
    SHOP_STOCK_ITEM_556_RIFLE        = 175,
};

/// Weapons stock for the default selector in standard save modes (0 and 1).
///
/// M.I.S.T. and unrecognized stock selectors use this fallback.
/// Two offered item rows; the stored zero after the terminator is outside the list.
/// Rows are unsigned halfwords, borrowed read-only while the room image is loaded.
static const u16 _gShopStandardDefaultWeaponsStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for the default selector in standard save modes (0 and 1).
///
/// M.I.S.T. and unrecognized stock selectors use this fallback.
/// Two ammunition rows and the Batteries/Fuel recharge service, then the terminator.
/// Rows are unsigned halfwords, borrowed read-only while the room image is loaded.
static const u16 _gShopStandardDefaultAmmunitionStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_BUCKSHOT,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
};

/// Armor stock for the default selector in standard save modes (0 and 1).
///
/// M.I.S.T. and unrecognized stock selectors use this fallback.
/// Two offered item rows; the stored zero after the terminator is outside the list.
/// Rows are unsigned halfwords, borrowed read-only while the room image is loaded.
static const u16 _gShopStandardDefaultArmorStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_TURTLE_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_ROW_END,
    0,
};

/// Items stock for the default selector in standard save modes (0 and 1).
///
/// M.I.S.T. and unrecognized stock selectors use this fallback.
/// Six offered item rows; the stored zero after the terminator is outside the list.
/// Rows are unsigned halfwords, borrowed read-only while the room image is loaded.
static const u16 _gShopStandardDefaultItemsStock[8] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
    0,
};

/// Weapons stock for Dryfield in standard modes (save modes 0 and 1).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardDryfieldWeaponsStock[8] SECTION(".data") = {
    SHOP_STOCK_ITEM_P08,
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_RIFLE_CLIP_HOLDER,
    SHOP_ITEM_M9,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for Dryfield in standard modes (save modes 0 and 1).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// Includes the Batteries/Fuel recharge service.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardDryfieldAmmunitionStock[8] SECTION(".data") = {
    SHOP_STOCK_ITEM_9MM_PB,
    SHOP_STOCK_ITEM_BUCKSHOT,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_GRENADE,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for Dryfield in standard modes (save modes 0 and 1).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardDryfieldArmorStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_ROW_END,
    0,
};

/// Items stock for Dryfield in standard modes (save modes 0 and 1).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardDryfieldItemsStock[8] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
    0,
};

/// Weapons stock for the armory in standard modes (save modes 0 and 1).
///
/// 7 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopStandardArmoryWeaponsStock[8] SECTION(".data") = {
    SHOP_STOCK_ITEM_P229,
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_RIFLE_CLIP_HOLDER,
    SHOP_ITEM_M9,
    SHOP_ITEM_HAMMER,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_ROW_END,
};

/// Ammunition stock for the armory in standard modes (save modes 0 and 1).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// Includes the Batteries/Fuel recharge service.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardArmoryAmmunitionStock[8] SECTION(".data") = {
    SHOP_STOCK_ITEM_9MM_PB,
    SHOP_STOCK_ITEM_BUCKSHOT,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_GRENADE,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for the armory in standard modes (save modes 0 and 1).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopStandardArmoryArmorStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_STOCK_ITEM_NBC_SUIT,
    SHOP_STOCK_ITEM_PSY_SUIT,
    SHOP_ROW_END,
};

/// Items stock for the armory in standard modes (save modes 0 and 1).
///
/// 8 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardArmoryItemsStock[10] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_RECOVERY_2,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
    0,
};

/// Weapons stock for late Dryfield in standard modes (save modes 0 and 1).
///
/// 10 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardDryfieldLateWeaponsStock[12] SECTION(".data") = {
    SHOP_STOCK_ITEM_P08,
    SHOP_ITEM_MP5A5_FIRST,
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_AS12,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_RIFLE_CLIP_HOLDER,
    SHOP_ITEM_M9,
    SHOP_ITEM_M203,
    SHOP_ITEM_PYKE,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for late Dryfield in standard modes (save modes 0 and 1).
///
/// 8 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// Includes the Batteries/Fuel recharge service.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopStandardDryfieldLateAmmunitionStock[10] SECTION(".data") = {
    SHOP_STOCK_ITEM_9MM_PB,
    SHOP_STOCK_ITEM_9MM_HYDRA,
    SHOP_STOCK_ITEM_BUCKSHOT,
    SHOP_STOCK_ITEM_FIREFLY,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_GRENADE,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for late Dryfield in standard modes (save modes 0 and 1).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardDryfieldLateArmorStock[4] = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_EOD_SUIT,
    SHOP_ROW_END,
};

/// Items stock for late Dryfield in standard modes (save modes 0 and 1).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardDryfieldLateItemsStock[8] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
    0,
};

/// Weapons stock for Shelter in standard modes (save modes 0 and 1).
///
/// 11 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardShelterWeaponsStock[12] = {
    SHOP_ITEM_MP5A5_FIRST,
    SHOP_ITEM_SMG_CLIP_HOLDER,
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_AS12,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_RIFLE_CLIP_HOLDER,
    SHOP_ITEM_M9,
    SHOP_ITEM_M203,
    SHOP_ITEM_HAMMER,
    SHOP_ITEM_PYKE,
    SHOP_ROW_END,
};

/// Ammunition stock for Shelter in standard modes (save modes 0 and 1).
///
/// 9 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalog.
static u16 _gShopStandardShelterAmmunitionStock[10] = {
    SHOP_STOCK_ITEM_9MM_SPARTAN,
    SHOP_STOCK_ITEM_44_MAGNUM,
    SHOP_STOCK_ITEM_FIREFLY,
    SHOP_STOCK_ITEM_R_SLUG,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_GRENADE,
    SHOP_STOCK_ITEM_AIRBURST,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
};

/// Armor stock for Shelter in standard modes (save modes 0 and 1).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardShelterArmorStock[4] = {
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_STOCK_ITEM_TACTICAL_ARMOR,
    SHOP_ROW_END,
};

/// Items stock for Shelter in standard modes (save modes 0 and 1).
///
/// 9 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardShelterItemsStock[10] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_RECOVERY_2,
    SHOP_STOCK_ITEM_RECOVERY_3,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
};

/// Weapons stock for Shelter's alternate profile in standard modes (save modes 0 and 1).
///
/// 12 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardShelterAlternateWeaponsStock[14] = {
    SHOP_ITEM_MP5A5_FIRST,
    SHOP_ITEM_SMG_CLIP_HOLDER,
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_AS12,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_RIFLE_CLIP_HOLDER,
    SHOP_ITEM_M9,
    SHOP_ITEM_M203,
    SHOP_ITEM_HAMMER,
    SHOP_ITEM_PYKE,
    SHOP_ITEM_JAVELIN,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for Shelter's alternate profile in standard modes (save modes 0 and 1).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalog.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardShelterAlternateAmmunitionStock[8] = {
    SHOP_STOCK_ITEM_9MM_SPARTAN,
    SHOP_STOCK_ITEM_FIREFLY,
    SHOP_STOCK_ITEM_R_SLUG,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_AIRBURST,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for Shelter's alternate profile in standard modes (save modes 0 and 1).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardShelterAlternateArmorStock[6] = {
    SHOP_STOCK_ITEM_TURTLE_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_TACTICAL_ARMOR,
    SHOP_STOCK_ITEM_AYA_SPECIAL,
    SHOP_ROW_END,
};

/// Items stock for Shelter's alternate profile in standard modes (save modes 0 and 1).
///
/// 10 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardShelterAlternateItemsStock[12] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_RECOVERY_2,
    SHOP_STOCK_ITEM_RECOVERY_3,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_MP_BOOST_2,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
    0,
};

/// Weapons stock for Shelter's event profile in standard modes (save modes 0 and 1).
///
/// 12 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardShelterEventWeaponsStock[14] = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_AS12,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_RIFLE_CLIP_HOLDER,
    SHOP_ITEM_M9,
    SHOP_ITEM_M203,
    SHOP_ITEM_HAMMER,
    SHOP_ITEM_PYKE,
    SHOP_ITEM_JAVELIN,
    SHOP_ITEM_MP5A5_FIRST,
    SHOP_ITEM_SMG_CLIP_HOLDER,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for Shelter's event profile in standard modes (save modes 0 and 1).
///
/// 9 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalog.
static u16 _gShopStandardShelterEventAmmunitionStock[10] = {
    SHOP_STOCK_ITEM_9MM_SPARTAN,
    SHOP_STOCK_ITEM_44_MAGNUM,
    SHOP_STOCK_ITEM_FIREFLY,
    SHOP_STOCK_ITEM_R_SLUG,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_GRENADE,
    SHOP_STOCK_ITEM_AIRBURST,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
};

/// Armor stock for Shelter's event profile in standard modes (save modes 0 and 1).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardShelterEventArmorStock[4] = {
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_STOCK_ITEM_TACTICAL_ARMOR,
    SHOP_ROW_END,
};

/// Items stock for Shelter's event profile in standard modes (save modes 0 and 1).
///
/// 9 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardShelterEventItemsStock[10] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_RECOVERY_2,
    SHOP_STOCK_ITEM_RECOVERY_3,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
};

/// Weapons stock for Shelter's alternate event profile in standard modes (save modes 0 and 1).
///
/// 14 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardShelterAlternateEventWeaponsStock[16] = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_STOCK_ITEM_AS12,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_MM1,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_RIFLE_CLIP_HOLDER,
    SHOP_ITEM_M9,
    SHOP_ITEM_M203,
    SHOP_ITEM_HAMMER,
    SHOP_ITEM_PYKE,
    SHOP_ITEM_JAVELIN,
    SHOP_STOCK_ITEM_M249,
    SHOP_ITEM_MP5A5_FIRST,
    SHOP_ITEM_SMG_CLIP_HOLDER,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for Shelter's alternate event profile in standard modes (save modes 0 and 1).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalog.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardShelterAlternateEventAmmunitionStock[8] = {
    SHOP_STOCK_ITEM_9MM_SPARTAN,
    SHOP_STOCK_ITEM_FIREFLY,
    SHOP_STOCK_ITEM_R_SLUG,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_AIRBURST,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for Shelter's alternate event profile in standard modes (save modes 0 and 1).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopStandardShelterAlternateEventArmorStock[6] = {
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_STOCK_ITEM_TACTICAL_ARMOR,
    SHOP_STOCK_ITEM_TURTLE_VEST,
    SHOP_STOCK_ITEM_AYA_SPECIAL,
    SHOP_ROW_END,
};

/// Items stock for Shelter's alternate event profile in standard modes (save modes 0 and 1).
///
/// 10 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopStandardShelterAlternateEventItemsStock[12] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_RECOVERY_1,
    SHOP_STOCK_ITEM_RECOVERY_2,
    SHOP_STOCK_ITEM_RECOVERY_3,
    SHOP_STOCK_ITEM_MP_BOOST_1,
    SHOP_STOCK_ITEM_MP_BOOST_2,
    SHOP_STOCK_ITEM_PENICILLIN,
    SHOP_STOCK_ITEM_STIM,
    SHOP_ROW_END,
    0,
};

/// Weapons stock for M.I.S.T. in restricted modes (save modes 2 and above).
///
/// 1 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopRestrictedMistWeaponsStock[2] = {
    SHOP_STOCK_ITEM_MM1,
    SHOP_ROW_END,
};

/// Ammunition stock for M.I.S.T. in restricted modes (save modes 2 and above).
///
/// 1 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopRestrictedMistAmmunitionStock[2] = {
    SHOP_STOCK_ITEM_RIOT,
    SHOP_ROW_END,
};

/// Armor stock for M.I.S.T. in restricted modes (save modes 2 and above).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedMistArmorStock[4] = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
    0,
};

/// Items stock for M.I.S.T. in restricted modes (save modes 2 and above).
///
/// 6 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedMistItemsStock[8] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_STOCK_ITEM_MEDICINE_WHEEL,
    SHOP_ROW_END,
    0,
};

/// Weapons stock for Dryfield in restricted modes (save modes 2 and above).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedDryfieldWeaponsStock[4] = {
    SHOP_STOCK_ITEM_P08,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for Dryfield in restricted modes (save modes 2 and above).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedDryfieldAmmunitionStock[4] = {
    SHOP_STOCK_ITEM_9MM_PB,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_ROW_END,
    0,
};

/// Armor stock for Dryfield in restricted modes (save modes 2 and above).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopRestrictedDryfieldArmorStock[4] = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
};

/// Items stock for Dryfield in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopRestrictedDryfieldItemsStock[6] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_ROW_END,
};

/// Weapons stock for the armory in restricted modes (save modes 2 and above).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedArmoryWeaponsStock[4] = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for the armory in restricted modes (save modes 2 and above).
///
/// 4 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedArmoryAmmunitionStock[6] = {
    SHOP_STOCK_ITEM_9MM_PB,
    SHOP_STOCK_ITEM_BUCKSHOT,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for the armory in restricted modes (save modes 2 and above).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedArmoryArmorStock[4] = {
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
    0,
};

/// Items stock for the armory in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static u16 _gShopRestrictedArmoryItemsStock[6] = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_ROW_END,
};

/// Weapons stock for late Dryfield in restricted modes (save modes 2 and above).
///
/// 4 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored word after the terminator is outside the offered list.
static u16 _gShopRestrictedDryfieldLateWeaponsStock[6] = {
    SHOP_STOCK_ITEM_P08,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_M9,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for late Dryfield in restricted modes (save modes 2 and above).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedDryfieldLateAmmunitionStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_9MM_PB,
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_END,
};

/// Armor stock for late Dryfield in restricted modes (save modes 2 and above).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedDryfieldLateArmorStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
};

/// Items stock for late Dryfield in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedDryfieldLateItemsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_ROW_END,
};

/// Weapons stock for the base Shelter profile in restricted modes (save modes 2 and above).
///
/// 4 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopRestrictedShelterWeaponsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_M9,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for the base Shelter profile in restricted modes (save modes 2 and above).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopRestrictedShelterAmmunitionStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for the base Shelter profile in restricted modes (save modes 2 and above).
///
/// 4 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopRestrictedShelterArmorStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
    0,
};

/// Items stock for the base Shelter profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterItemsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_ROW_END,
};

/// Weapons stock for the alternate Shelter profile in restricted modes (save modes 2 and above).
///
/// 4 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopRestrictedShelterAlternateWeaponsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_M9,
    SHOP_ROW_END,
    0,
};

/// Ammunition stock for the alternate Shelter profile in restricted modes (save modes 2 and above).
///
/// 1 offered row ID, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterAlternateAmmunitionStock[2] SECTION(".data") = {
    SHOP_STOCK_ITEM_RIOT,
    SHOP_ROW_END,
};

/// Armor stock for the alternate Shelter profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterAlternateArmorStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_STOCK_ITEM_TURTLE_VEST,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
};

/// Items stock for the alternate Shelter profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterAlternateItemsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_ROW_END,
};

/// Weapons stock for the Shelter event profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterEventWeaponsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_M9,
    SHOP_ITEM_MP5A5_FIRST,
    SHOP_ROW_END,
};

/// Ammunition stock for the Shelter event profile in restricted modes (save modes 2 and above).
///
/// 3 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// Includes the Batteries/Fuel recharge service.
static const u16 _gShopRestrictedShelterEventAmmunitionStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_RIOT,
    SHOP_STOCK_ITEM_556_RIFLE,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
};

/// Armor stock for the Shelter event profile in restricted modes (save modes 2 and above).
///
/// 4 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopRestrictedShelterEventArmorStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
    0,
};

/// Items stock for the Shelter event profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterEventItemsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_ROW_END,
};

/// Weapons stock for the alternate Shelter event profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterAlternateEventWeaponsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_PA3,
    SHOP_ITEM_GRENADE_PISTOL,
    SHOP_STOCK_ITEM_M4A1_RIFLE,
    SHOP_ITEM_M9,
    SHOP_ITEM_MP5A5_FIRST,
    SHOP_ROW_END,
};

/// Ammunition stock for the alternate Shelter event profile in restricted modes (save modes 2 and above).
///
/// 2 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
/// Includes the Batteries/Fuel recharge service.
/// The stored zero after the terminator is outside the offered list.
static const u16 _gShopRestrictedShelterAlternateEventAmmunitionStock[4] SECTION(".data") = {
    SHOP_STOCK_ITEM_RIOT,
    SHOP_ROW_RECHARGE_SERVICE,
    SHOP_ROW_END,
    0,
};

/// Armor stock for the alternate Shelter event profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterAlternateEventArmorStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER,
    SHOP_STOCK_ITEM_PASGT_VEST,
    SHOP_STOCK_ITEM_COMBAT_ARMOR,
    SHOP_STOCK_ITEM_TURTLE_VEST,
    SHOP_ITEM_BELT_POUCH,
    SHOP_ROW_END,
};

/// Items stock for the alternate Shelter event profile in restricted modes (save modes 2 and above).
///
/// 5 offered row IDs, followed by `SHOP_ROW_END`; borrowed read-only
/// while the room image remains loaded.
static const u16 _gShopRestrictedShelterAlternateEventItemsStock[6] SECTION(".data") = {
    SHOP_STOCK_ITEM_COMBAT_LIGHT,
    SHOP_STOCK_ITEM_PEPPER_SPRAY,
    SHOP_STOCK_ITEM_FLARE,
    SHOP_STOCK_ITEM_PROTEIN_CAPSULE,
    SHOP_STOCK_ITEM_LIPSTICK,
    SHOP_ROW_END,
};

/// Additional catalog ids in the replay tier ladder.
enum {
    SHOP_REPLAY_ITEM_MONK_ROBE         = 109,
    SHOP_REPLAY_ITEM_MD_PLAYER         = 14,
    SHOP_REPLAY_ITEM_SKULL_CRYSTAL     = 54,
    SHOP_REPLAY_ITEM_HOLY_WATER        = 56,
    SHOP_REPLAY_ITEM_OFUDA             = 57,
    SHOP_REPLAY_ITEM_RINGERS_SOLUTION  = 61,
    SHOP_REPLAY_ITEM_EAU_DE_TOILETTE   = 62,
    SHOP_REPLAY_ITEM_HUNTER_GOGGLES    = 63,
    SHOP_REPLAY_ITEM_MONGOOSE          = 136,
    SHOP_REPLAY_ITEM_HYPERVELOCITY     = 149,
    SHOP_REPLAY_ITEM_GUNBLADE          = 150,
    SHOP_REPLAY_ITEM_44_MAEDA_SP       = 167,
    SHOP_REPLAY_ITEM_COLA              = 5,
    SHOP_REPLAY_TIER_FINAL_EXP_CEILING = 0x7FFFFFFF
};

/// Thirteen replay-unlock tiers, each contributing three catalog item ids.
///
/// Save bit n selects row n. Normal/replay mode adds every unlocked row's
/// items to the matching category; demo scene 1 unlocks all rows. EXP ceilings
/// mirror the replay-award ladder; shop consumers read only the items.
/// All rows are borrowed read-only while this carrier's room image is loaded.
static const ShopTier _gShopReplayTierLadder[SHOP_TIER_COUNT] SECTION(".data") = {
    { 14500, { SHOP_REPLAY_ITEM_MONK_ROBE, SHOP_STOCK_ITEM_MEDICINE_WHEEL, SHOP_STOCK_ITEM_RECOVERY_2 } },
    { 16000, { SHOP_ITEM_M9, SHOP_ITEM_RIFLE_CLIP_HOLDER, SHOP_STOCK_ITEM_FLARE } },
    { 44000, { SHOP_ITEM_M203, SHOP_STOCK_ITEM_PROTEIN_CAPSULE, SHOP_STOCK_ITEM_9MM_HYDRA } },
    { 51000, { SHOP_ITEM_HAMMER, SHOP_ITEM_BELT_POUCH, SHOP_STOCK_ITEM_MP_BOOST_1 } },
    { 57000, { SHOP_ITEM_PYKE, SHOP_STOCK_ITEM_LIPSTICK, SHOP_STOCK_ITEM_TACTICAL_ARMOR } },
    { 62000, { SHOP_ITEM_JAVELIN, SHOP_REPLAY_ITEM_MD_PLAYER, SHOP_REPLAY_ITEM_HOLY_WATER } },
    { 66000, { SHOP_STOCK_ITEM_AYA_SPECIAL, SHOP_STOCK_ITEM_9MM_SPARTAN, SHOP_REPLAY_ITEM_OFUDA } },
    { 69000, { SHOP_STOCK_ITEM_AS12, SHOP_STOCK_ITEM_R_SLUG, SHOP_STOCK_ITEM_FIREFLY } },
    { 72000, { SHOP_REPLAY_ITEM_MONGOOSE, SHOP_STOCK_ITEM_44_MAGNUM, SHOP_REPLAY_ITEM_SKULL_CRYSTAL } },
    { 75000, { SHOP_STOCK_ITEM_M249, SHOP_REPLAY_ITEM_44_MAEDA_SP, SHOP_REPLAY_ITEM_COLA } },
    { 200000, { SHOP_STOCK_ITEM_MM1, SHOP_STOCK_ITEM_AIRBURST, SHOP_STOCK_ITEM_RECOVERY_3 } },
    { 400000, { SHOP_REPLAY_ITEM_HYPERVELOCITY, SHOP_REPLAY_ITEM_HUNTER_GOGGLES, SHOP_STOCK_ITEM_MP_BOOST_2 } },
    { SHOP_REPLAY_TIER_FINAL_EXP_CEILING, { SHOP_REPLAY_ITEM_GUNBLADE, SHOP_REPLAY_ITEM_RINGERS_SOLUTION, SHOP_REPLAY_ITEM_EAU_DE_TOILETTE } },
};

/// Row currently selected for the shop preview, or SHOP_PREVIEW_NONE before selection.
///
/// Active list input publishes item and service row ids here. The preview task
/// draws a loaded item only after its id agrees with this selection; service
/// rows omit the Amount caption. Each room image owns one active selection.
static s32 _gShopPreviewRowId = SHOP_PREVIEW_NONE;

/// NUL-terminated caption of the shop purchase action.
static const u8 _gShopPurchaseLabel[20] SECTION(".data") = "Purchase";

/// NUL-terminated Pass caption for leaving a category or purchase prompt.
static const u8 _gShopPassLabel[8] SECTION(".data") = "Pass";

/// NUL-terminated caption of the free weapon-supply recharge service row.
static const u8 _gShopRechargeServiceLabel[16] SECTION(".data") = "Batteries/Fuel";

/// Empty caption retained for the special row 0xFFFC.
///
/// Current stock and unlock paths do not produce this row; its service role
/// is unproven. Text drawing reads only the first NUL byte.
static const u8 _gShopUnusedSpecialRowLabel[4] SECTION(".data") = "";

/// NUL-terminated two-line prompt for the selected weapon-supply recharge row.
static const u8 _gShopRechargeServicePrompt[60] SECTION(".data") = "Weapons using batteries or fuel\ncan be reloaded for free.";

/// NUL-terminated caption of the Weapons shop category.
static const u8 _gShopWeaponsCategoryLabel[8] SECTION(".data") = "Weapons";

/// NUL-terminated caption of the Ammunition shop category.
static const u8 _gShopAmmunitionCategoryLabel[12] SECTION(".data") = "Ammunition";

/// NUL-terminated caption of the Armor shop category.
static const u8 _gShopArmorCategoryLabel[8] SECTION(".data") = "Armor";

/// NUL-terminated caption of the Items shop category.
static const u8 _gShopItemsCategoryLabel[8] SECTION(".data") = "Items";

/// NUL-terminated purchase notice shown when BP is below the pack price.
static const u8 _gShopInsufficientBpText[20] SECTION(".data") = "Insufficient BP.";

/// NUL-terminated purchase notice shown when the carried inventory cannot accept the item.
static const u8 _gShopInventoryFullText[16] SECTION(".data") = "Inventory full.";

/// NUL-terminated purchase notice shown when an existing consumable stack cannot accept another pack.
static const u8 _gShopAmmunitionCapacityText[32] SECTION(".data") = "Ammunition capacity reached.";

/// NUL-terminated Amount caption beside the selected item's carried-unit count.
static const u8 _gShopCarriedAmountLabel[12] SECTION(".data") = "Amount";

/// NUL-terminated multiplication marker beside the number of packs to purchase.
static const u8 _gShopPackMultiplierText[4] SECTION(".data") = "x";

/// Empty stock returned when the packed selector has no valid category.
///
/// `SHOP_ROW_END` is the first halfword, so no rows are offered. The stored zero
/// after it is outside the list. Storage is borrowed read-only while the room is loaded.
static const u16 _gShopEmptyStock[2] SECTION(".data") = {
    SHOP_ROW_END,
    0,
};

/// Shared row callback for all offered items and the recharge-service row.
///
/// Lists enable USER_INTERFACE_LIST_SHARED_ROW_CALLBACK: entry zero handles
/// every row. The list borrows this single-entry table for the room's lifetime.
static UiListRowCallback _gShopItemRowCallbacks[] = {
    _shopDrawItemRow,
};

/// Shared row callback for all four shop categories and the service/Pass row.
///
/// The list must enable `USER_INTERFACE_LIST_SHARED_ROW_CALLBACK`; entry 0
/// handles every item. This borrowed table lives for the overlay's lifetime.
static UiListRowCallback _gShopCategoryRowCallbacks[] = {
    _shopDrawCategoryRow,
};

/// Mutable category/Pass list shared by successive sessions in this room image.
///
/// Initialization expands the one-row seed to Weapons, Ammunition, Armor,
/// Items and Pass, with clamped navigation and fifteen-pixel spacing. Fitting
/// clears scrolling and flags, then the controller enables the shared row
/// callback and system cursor sound. Selection survives closure unless the
/// saved cursor preference resets it; only one category panel may use it at once.
static UiList _gShopCategoryList = {
    .rowCallbacks    = _gShopCategoryRowCallbacks,
    .itemCount       = 1,
    .visibleRowCount = { .unsignedValue = 1 },
    .wrapNavigation  = 0,
    .rowHeight       = 15,
};

/// Per-item callbacks for the two purchase-prompt choices: Purchase, then Pass.
///
/// Dispatch uses row indices 0..1 without the shared-row flag. This borrowed
/// table lives for the overlay's lifetime.
static UiListRowCallback _gShopPurchasePromptRowCallbacks[] = {
    _shopDrawPurchaseRow,
    _shopDrawPassRow,
};

/// Mutable two-row Purchase/Pass list shared by successive prompts in this room image.
///
/// Rows 0 and 1 dispatch their own callbacks, with wrapped navigation and
/// ten-pixel spacing. Fitting each prompt clears flags and scrolling; the
/// controller enables system cursor sound without the shared-callback flag.
/// Selection survives prompt closure unless the saved cursor preference resets
/// it when the next prompt is fitted. Only one purchase prompt may use it at once.
static UiList _gShopPurchasePromptList = {
    .rowCallbacks    = _gShopPurchasePromptRowCallbacks,
    .itemCount       = ARRAY_SIZE(_gShopPurchasePromptRowCallbacks),
    .visibleRowCount = { .unsignedValue = ARRAY_SIZE(_gShopPurchasePromptRowCallbacks) },
    .wrapNavigation  = 1,
    .rowHeight       = USER_INTERFACE_LIST_DEFAULT_ROW_HEIGHT,
};

/// Bodyless priority-192 recipe for the shop session's root category panel.
///
/// Bounds are initial screen-centered pixels; the controller fits five rows.
/// spawnArg1 carries the packed stock selector. The active root creates balance
/// and help children; the session owns its tree through closure. Spawning copies
/// the recipe, retaining callback code that requires this room image to stay loaded.
static const UiObjectDesc _gShopCategoryListDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -144, -104, 128, 40 },
    .otIndexSeed     = 56,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopCategoryListTask,
};

/// Bodyless priority-192 recipe for one selected category's offered-item panel.
///
/// The active child receives the packed stock selector and its category panel
/// as parent. Its callback owns list work and a preview child and fits the list
/// to these initial screen-centered pixel bounds. Spawning copies the recipe;
/// this room image must stay loaded while its callback or child panels run.
static const UiObjectDesc _gShopItemListDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -140, -93, 188, 160 },
    .otIndexSeed     = 48,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopItemListTask,
};

/// Bodyless priority-192 recipe for the BP and carried-row-capacity display.
///
/// Spawned without input as a category-panel child; it reads the live player
/// balance and inventory, counting occupied rows rather than item units.
/// Bounds are screen-centered pixels. Spawning copies the recipe; the callback
/// requires this room image and gameplay to stay loaded until the tree closes.
static const UiObjectDesc _gShopBalancePanelDescriptor SECTION(".data") = {
    .bounds          = { 48, 4, 96, 60 },
    .otIndexSeed     = 52,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopBalancePanelTask,
};

/// Bodyless priority-192 recipe for the selected row's Purchase/Pass prompt.
///
/// The item-list parent passes its selected row ID in spawnArg1, positions the
/// fitted child beside that row and suspends its own input. This includes the
/// dormant 0xFFFC row, whose meaning is unproven. Bounds are initial screen-
/// centered pixels. Spawning copies the recipe; its callback and descendants
/// require this room image to stay loaded until closure.
static const UiObjectDesc _gShopPurchasePromptDescriptor SECTION(".data") = {
    .bounds          = { 48, 32, 70, 32 },
    .otIndexSeed     = 20,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopPurchasePromptTask,
};

/// Bodyless panel recipe for purchase-failure notices.
///
/// The caller supplies notice 0 (insufficient BP), 1 (inventory full), or
/// 2 (ammunition capacity) and the purchase prompt as parent. The content task
/// fits the title-style panel to its message and dismisses it on input or expiry.
/// Spawning copies the recipe; its callback requires the room image to stay loaded.
static const UiObjectDesc _gShopNoticeDescriptor SECTION(".data") = {
    .style           = USER_INTERFACE_PANEL_TITLE_STYLE,
    .bounds          = { -96, -48, 192, 96 },
    .otIndexSeed     = 8,
    .field_E         = 0,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopNoticeTask,
    .taskDataValue   = 0,
};

/// Unused item-list panel recipe with a 160-by-92-pixel initial rectangle.
///
/// Retained in each shop overlay's data; no current shop controller spawns it.
/// It seeds a bodyless task with item-list content, OT tag 48 and priority 192.
static UiObjectDesc _gShopRetainedItemListDescriptor = {
    .style           = 0,
    .bounds          = { -128, -32, 160, 92 },
    .otIndexSeed     = 48,
    .field_E         = 0,
    .taskFlags       = TASK_BODY_NONE,
    .taskPriority    = 192,
    .contentCallback = _shopItemListTask,
    .taskDataValue   = 0,
};

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
/// carrier's refill storage and gameplay catalog through closure. Bounds are
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
/// The caller supplies an affordable catalog item id and its purchase prompt
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
