/* Continue cap_captions.inc.c after the preceding overlay wrappers. */

static void CapCaption_ShowModal(s16 arg0, s16 arg1, s16 arg2)
{
    CapCaption_SelectScript(arg0, arg1, 0xD0);
    displayQueueModeTask(&CapCaption_Data_80154508, arg2, 0, STAGE_ENTRY_RELOAD);
}

static inline void CapCaption_LoadResource(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                    = 0;
    CapCaption_Data_801544EC = arg0;
    CapCaption_Data_801544EE = arg1;
    for (i = 0; i < ARRAY_SIZE(D_8006C338); i++) {
        if (D_8006C338[i].kind == FILE_SYSTEM_RESOURCE_DATA) {
            if (count == arg2) {
                CapCaption_Relocate(D_8006C338[i].data);
                break;
            }
            count++;
        }
    }
}
