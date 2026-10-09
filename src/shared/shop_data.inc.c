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

static u16 Shop_Data_801815F8[4] = {
    140,
    143,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181600[4] = {
    172,
    175,
    0xFFFE,
    0xFFFF,
};

static u16 Shop_Data_80181608[4] = {
    103,
    98,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181610[8] = {
    65,
    59,
    1,
    6,
    8,
    4,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181620[8] = {
    131,
    140,
    143,
    10,
    70,
    138,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181630[8] = {
    160,
    172,
    171,
    169,
    175,
    0xFFFE,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181640[4] = {
    108,
    100,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181648[8] = {
    65,
    59,
    1,
    6,
    8,
    4,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181658[8] = {
    132,
    140,
    143,
    10,
    70,
    66,
    138,
    0xFFFF,
};

static u16 Shop_Data_80181668[8] = {
    160,
    172,
    171,
    169,
    175,
    0xFFFE,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181678[4] = {
    98,
    105,
    106,
    0xFFFF,
};

static u16 Shop_Data_80181680[10] = {
    65,
    59,
    58,
    1,
    2,
    6,
    8,
    4,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181694[12] = {
    131,
    157,
    140,
    142,
    143,
    10,
    70,
    69,
    67,
    138,
    0xFFFF,
    0,
};

static u16 Shop_Data_801816AC[10] = {
    160,
    161,
    172,
    173,
    171,
    169,
    175,
    0xFFFE,
    0xFFFF,
    0,
};

/// Item catalogue keys used by these base stock lists.
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
    SHOP_STOCK_ITEM_SHOULDER_HOLSTER = 108,
    SHOP_STOCK_ITEM_P08              = 131,
    SHOP_ITEM_GRENADE_PISTOL         = 138,
    SHOP_STOCK_ITEM_MM1              = 139,
    SHOP_STOCK_ITEM_PA3              = 140,
    SHOP_STOCK_ITEM_AS12             = 142,
    SHOP_STOCK_ITEM_M4A1_RIFLE       = 143,
    SHOP_STOCK_ITEM_M249             = 144,
    SHOP_ITEM_MP5A5_FIRST            = 157,
    SHOP_STOCK_ITEM_9MM_PB           = 160,
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
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalogue.
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
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalogue.
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
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalogue.
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
/// `SHOP_ROW_RECHARGE_SERVICE` is a service row, outside the item catalogue.
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

static u16 Shop_Data_80181888[4] = {
    160,
    171,
    175,
    0xFFFF,
};

static u16 Shop_Data_80181890[4] = {
    108,
    100,
    13,
    0xFFFF,
};

static u16 Shop_Data_80181898[6] = {
    65,
    59,
    58,
    60,
    11,
    0xFFFF,
};

static u16 Shop_Data_801818A4[6] = {
    140,
    138,
    143,
    70,
    0xFFFF,
    0,
};

static u16 Shop_Data_801818B0[4] = {
    171,
    175,
    0xFFFF,
    0,
};

static u16 Shop_Data_801818B8[6] = {
    108,
    100,
    98,
    13,
    0xFFFF,
    0,
};

static u16 Shop_Data_801818C4[6] = {
    65,
    59,
    58,
    60,
    11,
    0xFFFF,
};

static u16 Shop_Data_801818D0[6] = {
    140,
    138,
    143,
    70,
    0xFFFF,
    0,
};

static u16 Shop_Data_801818DC[2] = {
    171,
    0xFFFF,
};

static u16 Shop_Data_801818E0[6] = {
    108,
    100,
    98,
    103,
    13,
    0xFFFF,
};

static u16 Shop_Data_801818EC[6] = {
    65,
    59,
    58,
    60,
    11,
    0xFFFF,
};

static u16 Shop_Data_801818F8[6] = {
    140,
    138,
    143,
    70,
    157,
    0xFFFF,
};

static u16 Shop_Data_80181904[4] = {
    171,
    175,
    0xFFFE,
    0xFFFF,
};

static u16 Shop_Data_8018190C[6] = {
    108,
    100,
    98,
    13,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181918[6] = {
    65,
    59,
    58,
    60,
    11,
    0xFFFF,
};

static u16 Shop_Data_80181924[6] = {
    140,
    138,
    143,
    70,
    157,
    0xFFFF,
};

static u16 Shop_Data_80181930[4] = {
    171,
    0xFFFE,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181938[6] = {
    108,
    100,
    98,
    103,
    13,
    0xFFFF,
};

static u16 Shop_Data_80181944[6] = {
    65,
    59,
    58,
    60,
    11,
    0xFFFF,
};

static ShopTier Shop_Data_80181950[SHOP_TIER_COUNT] = {
    { 0x38A4, { 109, 55, 2 } },
    { 0x3E80, { 70, 10, 58 } },
    { 0xABE0, { 69, 60, 161 } },
    { 0xC738, { 66, 13, 6 } },
    { 0xDEA8, { 67, 11, 97 } },
    { 0xF230, { 68, 14, 56 } },
    { 0x101D0, { 107, 162, 57 } },
    { 0x10D88, { 142, 174, 173 } },
    { 0x11940, { 136, 166, 54 } },
    { 0x124F8, { 144, 167, 5 } },
    { 0x30D40, { 139, 170, 3 } },
    { 0x61A80, { 149, 63, 7 } },
    { 0x7FFFFFFF, { 150, 61, 62 } },
};

static s32 Shop_Data_801819EC = -1;

static u8 Shop_Data_801819F0[20] = {
    80,
    117,
    114,
    99,
    104,
    97,
    115,
    101,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static u8 Shop_Data_80181A04[8] = {
    80,
    97,
    115,
    115,
    0,
    0,
    0,
    0,
};

static u8 Shop_Data_80181A0C[16] = {
    66,
    97,
    116,
    116,
    101,
    114,
    105,
    101,
    115,
    47,
    70,
    117,
    101,
    108,
    0,
    0,
};

static u8 Shop_Data_80181A1C[4] = { 0 };

static u8 Shop_Data_80181A20[60] = {
    87,
    101,
    97,
    112,
    111,
    110,
    115,
    32,
    117,
    115,
    105,
    110,
    103,
    32,
    98,
    97,
    116,
    116,
    101,
    114,
    105,
    101,
    115,
    32,
    111,
    114,
    32,
    102,
    117,
    101,
    108,
    10,
    99,
    97,
    110,
    32,
    98,
    101,
    32,
    114,
    101,
    108,
    111,
    97,
    100,
    101,
    100,
    32,
    102,
    111,
    114,
    32,
    102,
    114,
    101,
    101,
    46,
    0,
    0,
    0,
};

static u8 Shop_Data_80181A5C[8] = {
    87,
    101,
    97,
    112,
    111,
    110,
    115,
    0,
};

static u8 Shop_Data_80181A64[12] = {
    65,
    109,
    109,
    117,
    110,
    105,
    116,
    105,
    111,
    110,
    0,
    0,
};

static u8 Shop_Data_80181A70[8] = {
    65,
    114,
    109,
    111,
    114,
    0,
    0,
    0,
};

static u8 Shop_Data_80181A78[8] = {
    73,
    116,
    101,
    109,
    115,
    0,
    0,
    0,
};

static u8 Shop_Data_80181A80[20] = {
    73,
    110,
    115,
    117,
    102,
    102,
    105,
    99,
    105,
    101,
    110,
    116,
    32,
    66,
    80,
    46,
    0,
    0,
    0,
    0,
};

static u8 Shop_Data_80181A94[16] = {
    73,
    110,
    118,
    101,
    110,
    116,
    111,
    114,
    121,
    32,
    102,
    117,
    108,
    108,
    46,
    0,
};

static u8 Shop_Data_80181AA4[32] = {
    65,
    109,
    109,
    117,
    110,
    105,
    116,
    105,
    111,
    110,
    32,
    99,
    97,
    112,
    97,
    99,
    105,
    116,
    121,
    32,
    114,
    101,
    97,
    99,
    104,
    101,
    100,
    46,
    0,
    0,
    0,
    0,
};

static u8 Shop_Data_80181AC4[12] = {
    65,
    109,
    111,
    117,
    110,
    116,
    0,
    0,
    0,
    0,
    0,
    0,
};

static u8 Shop_Data_80181AD0[4] = {
    120,
    0,
    0,
    0,
};

static u16 Shop_Data_80181AD4[2] = {
    0xFFFF,
    0,
};

static UiListRowCallback Shop_Data_80181AD8[1] = {
    _shopDrawItemRow,
};

static UiListRowCallback Shop_Data_80181ADC[1] = {
    _shopDrawCategoryRow,
};

static UiList Shop_Data_80181AE0 = { Shop_Data_80181ADC, 1, { .unsignedValue = 1 }, 0, 15, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

static UiListRowCallback Shop_Data_80181B04[2] = {
    _shopDrawPurchaseRow,
    _shopDrawPassRow,
};

static UiList Shop_Data_80181B0C = { Shop_Data_80181B04, 2, { .unsignedValue = 2 }, 1, 10, 0, { .unsignedValue = 0 }, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { .unsignedValue = 0 }, 0 };

static UiObjectDesc Shop_Data_80181B30 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -144, -104, 128, 40 }, 56, 0, TASK_BODY_NONE, 192, _shopCategoryListTask, 0 };

static UiObjectDesc Shop_Data_80181B4C = { USER_INTERFACE_PANEL_TITLE_STYLE, { -140, -93, 188, 160 }, 48, 0, TASK_BODY_NONE, 192, _shopItemListTask, 0 };

static UiObjectDesc Shop_Data_80181B68 = { 0, { 48, 4, 96, 60 }, 52, 0, TASK_BODY_NONE, 192, _shopBalancePanelTask, 0 };

static UiObjectDesc Shop_Data_80181B84 = { 0, { 48, 32, 70, 32 }, 20, 0, TASK_BODY_NONE, 192, _shopPurchasePromptTask, 0 };

static UiObjectDesc Shop_Data_80181BA0 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -96, -48, 192, 96 }, 8, 0, TASK_BODY_NONE, 192, _shopNoticeTask, 0 };

/* Retained complete UI descriptor, including its embedded task seed. */
static UiObjectDesc Shop_ItemListDescriptor = { 0, { -128, -32, 160, 92 }, 48, 0, TASK_BODY_NONE, 192, _shopItemListTask, 0 };
