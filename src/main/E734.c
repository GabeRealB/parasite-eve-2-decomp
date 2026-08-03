#include "common.h"

#include <psyq/libcd.h>

#include "main/game.h"
#include "main/fs.h"
#include "main/fs.h"

INCLUDE_ASM("main/nonmatchings/E734", func_8001DF34);

s16 func_8001E57C(void);
s16 E734_CDIsShellOpenBitSet(void);

s32 func_8001E2D4(void)
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
                            if (E734_CDIsShellOpenBitSet() != 0) {
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
                    if (func_8001E57C() != 0) {
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
                            if (E734_CDIsShellOpenBitSet() != 0) {
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
                    if (func_8001E57C() != 0) {
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

INCLUDE_ASM("main/nonmatchings/E734", func_8001E57C);

s32 func_8001E6AC(s32 arg0, s32 arg1)
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
                    if (E734_CDIsShellOpenBitSet() != 0) {
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
            if (func_8001E57C() != 0) {
                state->field_228 = 0;
                return 2;
            }
            break;
        default:
            return 0;
    }
    return 0;
}

s16 E734_CDIsShellOpenBitSet(void)
{
    s16 tmp;
    u8  result[8];

    // Writing it as (result[0] & CdlStatShellOpen) != 0 produces the wrong code
    CdControlB(CdlNop, NULL, result);
    tmp = result[0] & CdlStatShellOpen;
    return tmp != 0;
}

bool E734_CDCanIssueCommand(void)
{
    return CdDiskReady(1) == CdlComplete;
}
