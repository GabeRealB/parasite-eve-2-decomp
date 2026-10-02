/* Private per-instance storage. Include at the original data position.
 * The configuration contract is documented in shop.h. */

static void Shop_SessionTask(Task* task);

static void Shop_QuantityTask(Task* task);

static void Shop_PreviewTask(Task* task);

static void Shop_ChargeTask(Task* task);

static UiObjectDesc Shop_Data_80181BD8 = { USER_INTERFACE_PANEL_TITLE_STYLE, { -72, -36, 144, 64 }, 32, 0, TASK_BODY_NONE, 192, Shop_ChargeTask, 0 };

static UiObjectDesc Shop_Data_80181BF4 = { 0, { 48, -93, 96, 97 }, 44, 0, TASK_BODY_NONE, 192, Shop_PreviewTask, 0 };

static UiObjectDesc Shop_Data_80181C10 = { 3, { -72, -32, 184, 48 }, 16, 0, TASK_BODY_NONE, 192, Shop_QuantityTask, 0 };
