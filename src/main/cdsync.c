#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libetc.h>

#include "main/game.h"
#include "main/fs.h"

s16 CdSync_IsShellOpenBitSet(void);

s32 CdCmd_SeekL(u8* loc)
{
    CdCmdQueue* state;
    CdCmdQueue* p;
    s32         temp;
    s32         status;
    s32         one;
    u8          pad[8];

    state = &CdCmd_Queue;
    one   = 1;
    switch (state->field_1D6) {
        case 0:
            switch (state->field_228) {
                case 0:
                    status = CdSync(1, NULL);
                    switch (status) {
                        case CdlComplete:
                            state->field_1d4 = 0;
                            temp             = 1;
                            goto join1;
                        case CdlNoIntr:
                            goto set0_1;
                        case CdlDiskError:
                            state->field_1d4 = one;
                            if (CdSync_IsShellOpenBitSet() != 0) {
                                state->field_228 += 1;
                                goto set0_1;
                            }
                            temp = 2;
                            goto join1;
                        default:
                            temp = 2;
                            goto join1;
                    }
                case 1:
                    if (CdCmd_RecoverDisk() != 0) {
                        state->field_228 = 0;
                        temp             = 2;
                        goto join1;
                    }
                    goto set0_1;
                default:
                    temp = 0;
                    goto join1;
            }
        set0_1:
            temp = 0;
        join1:
            status = temp;
            switch (status) {
                case 0:
                    return 0;
                case 2:
                    CdFlush();
                    /* fallthrough */
                case 1:
                    CdControlF(CdlSetloc, loc);
                    state->field_1D6++;
                    break;
            }
            /* fallthrough */
        case 1:
            p = &CdCmd_Queue;
            switch (p->field_228) {
                case 0:
                    status = CdSync(1, NULL);
                    switch (status) {
                        case CdlComplete:
                            p->field_1d4 = 0;
                            temp         = 1;
                            goto join2;
                        case CdlNoIntr:
                            goto set0_2;
                        case CdlDiskError:
                            p->field_1d4 = 1;
                            if (CdSync_IsShellOpenBitSet() != 0) {
                                p->field_228 += 1;
                                goto set0_2;
                            }
                            temp = 2;
                            goto join2;
                        default:
                            temp = 2;
                            goto join2;
                    }
                case 1:
                    if (CdCmd_RecoverDisk() != 0) {
                        p->field_228 = 0;
                        temp         = 2;
                        goto join2;
                    }
                    goto set0_2;
                default:
                    temp = 0;
                    goto join2;
            }
        set0_2:
            temp = 0;
        join2:
            status = temp;
            switch (status) {
                case 0:
                    return 0;
                case 1:
                    CdControlF(CdlSeekL, NULL);
                    state->field_1D6++;
                    break;
                case 2:
                    goto L_flush_clear;
            }
            /* fallthrough */
        case 2:
            p = &CdCmd_Queue;
            switch (p->field_228) {
                case 0:
                    status = CdSync(1, NULL);
                    switch (status) {
                        case CdlComplete:
                            p->field_1d4 = 0;
                            temp         = 1;
                            goto join3;
                        case CdlNoIntr:
                            goto set0_3;
                        case CdlDiskError:
                            p->field_1d4 = 1;
                            if (CdSync_IsShellOpenBitSet() != 0) {
                                p->field_228 += 1;
                                goto set0_3;
                            }
                            temp = 2;
                            goto join3;
                        default:
                            temp = 2;
                            goto join3;
                    }
                case 1:
                    if (CdCmd_RecoverDisk() != 0) {
                        p->field_228 = 0;
                        temp         = 2;
                        goto join3;
                    }
                    goto set0_3;
                default:
                    temp = 0;
                    goto join3;
            }
        set0_3:
            temp = 0;
        join3:
            status = temp;
            if (status == 1) {
                goto L_done;
            }
            if (status < 2) {
                goto L_ret0;
            }
            if (status == 2) {
                goto L_flush_clear;
            }
            goto L_ret0;
        L_done:
            state->field_1D6 = 0;
            state->field_1d4 = 0;
            return 1;
        L_flush_clear:
            CdFlush();
            state->field_1D6 = 0;
        L_ret0:
            return 0;
        default:
            return 0;
    }
}

s32 CdCmd_PausePoll(void)
{
    CdCmdQueue* state;
    CdCmdQueue* p;
    s32         temp;
    s32         status;
    s32         one;

    state = &CdCmd_Queue;
    switch (state->field_1DE) {
        case 0:
            switch (state->field_228) {
                case 0:
                    status = CdSync(1, NULL);
                    switch (status) {
                        case CdlComplete:
                            state->field_1d4 = 0;
                            temp             = 1;
                            goto join1;
                        case CdlNoIntr:
                            goto set0_1;
                        case CdlDiskError:
                            state->field_1d4 = 1;
                            if (CdSync_IsShellOpenBitSet() != 0) {
                                state->field_228 += 1;
                                goto set0_1;
                            }
                            temp = 2;
                            goto join1;
                        default:
                            temp = 2;
                            goto join1;
                    }
                case 1:
                    if (CdCmd_RecoverDisk() != 0) {
                        state->field_228 = 0;
                        temp             = 2;
                        goto join1;
                    }
                    goto set0_1;
                default:
                    temp = 0;
                    goto join1;
            }
        set0_1:
            temp = 0;
        join1:
            status = temp;
            switch (status) {
                case 0:
                    return 0;
                case 2:
                    CdFlush();
                    /* fallthrough */
                case 1:
                    state->field_1E0 = 0;
                    CdControlF(CdlPause, NULL);
                    state->field_1DE++;
                    break;
            }
            /* fallthrough */
        case 1:
            p = &CdCmd_Queue;
            switch (p->field_228) {
                case 0:
                    status = CdSync(1, NULL);
                    switch (status) {
                        case CdlComplete:
                            p->field_1d4 = 0;
                            temp         = 1;
                            goto join2;
                        case CdlNoIntr:
                            goto set0_2;
                        case CdlDiskError:
                            p->field_1d4 = 1;
                            if (CdSync_IsShellOpenBitSet() != 0) {
                                p->field_228 += 1;
                                goto set0_2;
                            }
                            temp = 2;
                            goto join2;
                        default:
                            temp = 2;
                            goto join2;
                    }
                case 1:
                    if (CdCmd_RecoverDisk() != 0) {
                        p->field_228 = 0;
                        temp         = 2;
                        goto join2;
                    }
                    goto set0_2;
                default:
                    temp = 0;
                    goto join2;
            }
        set0_2:
            temp = 0;
        join2:
            status = temp;
            one    = 1;
            if (status == one) {
                goto L_case1;
            }
            if (status < 2) {
                goto L_ret0;
            }
            if (status == 2) {
                goto L_case2;
            }
            if (status == 3) {
                goto L_case3;
            }
            goto L_ret0;
        L_case1:
            state->field_1DE = 0;
            state->field_1d4 = 0;
            return 1;
        L_case2:
            state->field_1DE = one;
            CdFlush();
            CdControlF(CdlPause, NULL);
            return 0;
        L_case3:
            CdFlush();
            CdControlF(CdlPause, NULL);
            state->field_1DE = one;
            state->field_1E0++;
        L_ret0:
            return 0;
        default:
            return 0;
    }
}

s16 CdCmd_RecoverDisk(void)
{
    u8          mode[8];
    u8          result[8];
    u8          loc[8];
    CdCmdQueue* state;
    s32         temp;

    state = &CdCmd_Queue;
    switch (state->field_226) {
        case 0:
            temp = CdDiskReady(1);
            if (temp == CdlComplete) {
                temp = 1;
            } else {
                temp = 0;
            }
            if (temp != 0) {
                CdControlB(CdlGetTN, NULL, NULL);
                mode[0] = CdlModeSpeed | CdlModeSize1;
                CdControlB(CdlSetmode, mode, NULL);
                VSync(3);
                loc[0] = 0xA;
                loc[1] = 0;
                loc[2] = 0;
                CdControlB(CdlReadN, loc, result);
                if ((result[0] & CdlStatError) && (result[1] & 0x40)) {
                    state->field_226 += 1;
                } else {
                    state->field_226 = 0;
                    return 1;
                }
            }
            break;
        case 1:
            CdControlB(CdlNop, NULL, mode);
            temp = mode[0] & CdlStatShellOpen;
            if (temp == CdlStatShellOpen) {
                temp = 1;
            } else {
                temp = 0;
            }
            if (temp != 0) {
                state->field_1E4 = 0;
            }
            break;
        default:
            break;
    }
    return 0;
}

s32 CdCmd_PollStatus(s32 arg0, s32 arg1)
{
    CdCmdQueue* state;
    s32         status;
    u16         a1;

    state = &CdCmd_Queue;
    switch (state->field_228) {
        case 0:
            status = CdSync(1, NULL);
            switch (status) {
                case CdlNoIntr:
                    break;
                case CdlComplete:
                    state->field_1d4 = 0;
                    return 1;
                case CdlDiskError:
                    state->field_1d4 = 1;
                    if (CdSync_IsShellOpenBitSet() != 0) {
                        state->field_228++;
                        break;
                    }
                    a1 = arg1 & 0xFFFF;
                    if (a1 == 0) {
                        return 2;
                    }
                    if ((arg0 & 0xFFFF) == a1) {
                        return 1;
                    }
                    return 3;
                default:
                    return 2;
            }
            break;
        case 1:
            if (CdCmd_RecoverDisk() != 0) {
                state->field_228 = 0;
                return 2;
            }
            break;
        default:
            return 0;
    }
    return 0;
}

s16 CdSync_IsShellOpenBitSet(void)
{
    s16 tmp;
    u8  result[8];

    // Writing it as (result[0] & CdlStatShellOpen) != 0 produces the wrong code
    CdControlB(CdlNop, NULL, result);
    tmp = result[0] & CdlStatShellOpen;
    return tmp != 0;
}

bool CdSync_CanIssueCommand(void)
{
    return CdDiskReady(1) == CdlComplete;
}
