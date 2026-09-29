/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in shop.h. */

static void Shop_SessionTask(Task* task);

static void Shop_QuantityTask(Task* task);

static void Shop_PreviewTask(Task* task);

static void Shop_ChargeTask(Task* task);

static UiObjectDesc Shop_Data_80181BD8 = { 2, 0xFFB8, 0xFFDC, 144, 64, 32, 0, 0, 192, Shop_ChargeTask, 0 };

static UiObjectDesc Shop_Data_80181BF4 = { 0, 48, 0xFFA3, 96, 97, 44, 0, 0, 192, Shop_PreviewTask, 0 };

static UiObjectDesc Shop_Data_80181C10 = { 3, 0xFFB8, 0xFFE0, 184, 48, 16, 0, 0, 192, Shop_QuantityTask, 0 };
