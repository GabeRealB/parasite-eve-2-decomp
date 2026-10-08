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

static u16 Shop_Data_801816C0[4] = {
    108,
    100,
    102,
    0xFFFF,
};

static u16 Shop_Data_801816C8[8] = {
    65,
    59,
    1,
    6,
    8,
    4,
    0xFFFF,
    0,
};

static u16 Shop_Data_801816D8[12] = {
    157,
    9,
    140,
    142,
    138,
    143,
    10,
    70,
    69,
    66,
    67,
    0xFFFF,
};

static u16 Shop_Data_801816F0[10] = {
    162,
    166,
    173,
    174,
    171,
    169,
    170,
    175,
    0xFFFE,
    0xFFFF,
};

static u16 Shop_Data_80181704[4] = {
    100,
    98,
    97,
    0xFFFF,
};

static u16 Shop_Data_8018170C[10] = {
    65,
    59,
    58,
    1,
    2,
    3,
    6,
    8,
    4,
    0xFFFF,
};

static u16 Shop_Data_80181720[14] = {
    157,
    9,
    140,
    142,
    143,
    10,
    70,
    69,
    66,
    67,
    68,
    138,
    0xFFFF,
    0,
};

static u16 Shop_Data_8018173C[8] = {
    162,
    173,
    174,
    171,
    170,
    0xFFFE,
    0xFFFF,
    0,
};

static u16 Shop_Data_8018174C[6] = {
    103,
    98,
    100,
    97,
    107,
    0xFFFF,
};

static u16 Shop_Data_80181758[12] = {
    65,
    59,
    58,
    1,
    2,
    3,
    6,
    7,
    8,
    4,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181770[14] = {
    140,
    142,
    138,
    143,
    10,
    70,
    69,
    66,
    67,
    68,
    157,
    9,
    0xFFFF,
    0,
};

static u16 Shop_Data_8018178C[10] = {
    162,
    166,
    173,
    174,
    171,
    169,
    170,
    175,
    0xFFFE,
    0xFFFF,
};

static u16 Shop_Data_801817A0[4] = {
    100,
    98,
    97,
    0xFFFF,
};

static u16 Shop_Data_801817A8[10] = {
    65,
    59,
    58,
    1,
    2,
    3,
    6,
    8,
    4,
    0xFFFF,
};

static u16 Shop_Data_801817BC[16] = {
    140,
    142,
    138,
    139,
    143,
    10,
    70,
    69,
    66,
    67,
    68,
    144,
    157,
    9,
    0xFFFF,
    0,
};

static u16 Shop_Data_801817DC[8] = {
    162,
    173,
    174,
    171,
    170,
    0xFFFE,
    0xFFFF,
    0,
};

static u16 Shop_Data_801817EC[6] = {
    100,
    98,
    97,
    103,
    107,
    0xFFFF,
};

static u16 Shop_Data_801817F8[12] = {
    65,
    59,
    58,
    1,
    2,
    3,
    6,
    7,
    8,
    4,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181810[2] = {
    139,
    0xFFFF,
};

static u16 Shop_Data_80181814[2] = {
    171,
    0xFFFF,
};

static u16 Shop_Data_80181818[4] = {
    108,
    13,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181820[8] = {
    65,
    59,
    58,
    60,
    11,
    55,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181830[4] = {
    131,
    138,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181838[4] = {
    160,
    171,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181840[4] = {
    108,
    100,
    13,
    0xFFFF,
};

static u16 Shop_Data_80181848[6] = {
    65,
    59,
    58,
    60,
    11,
    0xFFFF,
};

static u16 Shop_Data_80181854[4] = {
    140,
    138,
    0xFFFF,
    0,
};

static u16 Shop_Data_8018185C[6] = {
    160,
    172,
    171,
    175,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181868[4] = {
    98,
    13,
    0xFFFF,
    0,
};

static u16 Shop_Data_80181870[6] = {
    65,
    59,
    58,
    60,
    11,
    0xFFFF,
};

static u16 Shop_Data_8018187C[6] = {
    131,
    138,
    143,
    70,
    0xFFFF,
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
