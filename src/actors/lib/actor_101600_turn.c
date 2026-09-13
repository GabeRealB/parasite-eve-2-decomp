#include "common.h"
#include "actors/actor_101600.h"
#include "main/gfx.h"

u8 Actor01600_Fn04EB0(Actor01600* arg0)
{
    SVECTOR         dir;
    s32             distance;
    Actor01600Work* work;
    s32             flags;
    s32             angle;
    s32             mag;
    s32             other;
    s32             span;
    s32             midpoint;
    s32             delta1;
    s32             best;
    s32             direction;
    s32             i;
    s32             count;
    s32             savedAngle;
    s32             callAngle;
    s32             high;
    s32             circle;

    work  = arg0->field_1C;
    flags = 0;
    switch (work->field_4EA) {
        case 0:
            savedAngle = Actor01600_Fn045A8(arg0, &distance);
            TOUCH_REG(savedAngle);
            Actor01600_Fn04C64(arg0, distance, savedAngle);
            work->field_4E0 = savedAngle;
            work->field_4EA++;
            break;
        case 1:
            callAngle       = Actor01600_Fn045A8(arg0, &distance);
            work->field_4E0 = callAngle;
            if (Actor01600_Fn04C64(arg0, distance, callAngle) & 0xFF) {
                Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, &dir);
                ratan2(dir.vx, dir.vz);
                mag   = __builtin_abs(work->field_4E0);
                other = 0x1000 - mag;
                TOUCH_REG_USE(other, flags);
                SOFT_TOUCH_REG(other);
                SOFT_TOUCH_REG(other);
                flags = 0;
                if (other < mag)
                    angle = other;
                else {
                    flags = 0x80;
                    angle = mag;
                }
                work->field_4F0 = angle / 16;
                if (work->field_4F0 < 0x20)
                    work->field_4F0 = 0x20;
                return flags | 1;
            }
            work->field_4EA++;
            break;
        case 2:
            work->field_4EC = -0x71;
            work->field_4E8 = 0;
            work->field_4EE = -10;
            for (i = 0; i < 8; i++) {
                work->ranges[i].low  = 0xFFFF;
                work->ranges[i].high = 0xFFFF;
            }
            Actor01600_Fn04C64(arg0, 2000, 0);
            work->field_4EA++;
            break;
        case 3:
            if (Actor01600_Fn04C64(arg0, 2000, 0) & 0xFF) {
                if (!work->field_4E8)
                    return 0xFF;
                work->field_4EA++;
            } else
                return 0;
            break;
        case 4:
            angle = Actor01600_Fn045A8(arg0, &distance);
            if (angle < 0)
                angle += 0x1000;
            mag             = work->field_4E8;
            work->field_4E0 = 0;
            if (mag == 1) {
                mag   = work->ranges[0].high;
                other = work->ranges[0].low;
                span  = mag - other;
                if (span >= 0x400) {
                    delta1   = __builtin_abs(angle - mag);
                    midpoint = __builtin_abs(angle - other);
                    if (delta1 < midpoint)
                        work->field_4E0 = mag;
                    else
                        work->field_4E0 = other;
                } else
                    work->field_4E0 = other + span / 2;
            } else {
                best = 0xFFFF;
                i    = 0;
                if (mag > 0) {
                    circle = 0x1000;
                    count  = mag;
                    do {
                        high     = *(volatile s32*)&work->ranges[i].high;
                        mag      = *(volatile s32*)&work->ranges[i].low;
                        span     = high - mag;
                        midpoint = mag + span / 2;
                        if ((u32)(midpoint - 0x780) >= 0x101U) {
                            mag       = __builtin_abs(angle - midpoint);
                            other     = circle - mag;
                            direction = 0;
                            if (other < mag)
                                mag = other;
                            else
                                direction = 0x80;
                            if (mag < best) {
                                flags = direction;
                                best  = mag;
                                if (span >= 0x400) {
                                    other    = work->ranges[i].high;
                                    mag      = *(volatile s32*)&work->ranges[i].low;
                                    delta1   = __builtin_abs(angle - other);
                                    midpoint = __builtin_abs(angle - mag);
                                    if (delta1 < midpoint)
                                        midpoint = other;
                                    else
                                        midpoint = mag;
                                }
                                work->field_4E0 = midpoint;
                            }
                        }
                        i++;
                    } while (i < count);
                }
            }
            if (work->field_4E0 > 0x800)
                work->field_4E0 -= 0x1000;
            else if (work->field_4E0 < -0x800)
                work->field_4E0 += 0x1000;
            Gfx_MatrixCol2(&arg0->field_2C->field_8->coord, &dir);
            ratan2(dir.vx, dir.vz);
            angle = __builtin_abs(work->field_4E0);
            if (angle > 0x800)
                angle = 0x1000 - angle;
            work->field_4F0 = angle / 16;
            if (work->field_4F0 < 0x20)
                work->field_4F0 = 0x20;
            return flags | 2;
    }
    return 0;
}

const s32 Actor01600_D001D0 = 0;
