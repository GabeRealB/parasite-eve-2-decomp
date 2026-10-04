/* Part of the Moth library; see moth.h. */

/// Records the root position for the contact pass, then moves. Before the alert
/// it jitters each axis by up to +-31 a frame, reversing the step when it would
/// leave a box of +-200 (X/Z) and +-500 (Y) around the home position; once
/// alerted it flies along its facing at gMothSpeeds[place row] plus a random
/// 0-31, holding its height within 400 of a level 0x4B0 above the player.
void mothDrift(Task* arg0)
{
    MothWork* work;
    GfxCoord* coord;
    u32       random;
    u32       random2;
    u32       random3;
    s32       amount;
    s32       amountB;
    s16       delta;
    s16       speed;
    s32       y;
    s32       newY;
    s16       base;

    work             = arg0->work;
    coord            = arg0->extra.tmd->coords;
    work->prevPos.vx = coord->coord.t[0];
    work->prevPos.vy = coord->coord.t[1];
    work->prevPos.vz = coord->coord.t[2];
    switch (work->alerted) {
        case 0:
            random = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            amount = random & 0x1F;
            if (!(random & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)(coord->coord.t[0] + (s16)delta) < work->homePos.vx + 200 &&
                work->homePos.vx - 200 < (s16)(coord->coord.t[0] + (s16)delta)) {
                coord->coord.t[0] += (s16)delta;
            } else {
                coord->coord.t[0] -= (s16)delta;
            }
            amountB = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1F;
            if (work->flapFast != 0) {
                amountB = -amountB;
            }
            delta = amountB;
            if ((s16)(coord->coord.t[1] + (s16)delta) < work->homePos.vy + 500 &&
                work->homePos.vy - 500 < (s16)(coord->coord.t[1] + (s16)delta)) {
                coord->coord.t[1] += (s16)delta;
            } else {
                coord->coord.t[1] -= (s16)delta;
            }
            random3 = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            amount  = random3 & 0x1F;
            if (!(random3 & 0x20)) {
                amount = -amount;
            }
            delta = amount;
            if ((s16)random3 < work->homePos.vz + 200 && work->homePos.vz - 200 < (s16)random3) {
                coord->coord.t[2] += (s16)delta;
            } else {
                coord->coord.t[2] -= (s16)delta;
            }
            break;
        case 1:
            speed = gMothSpeeds[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] +
                    (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1F);
            coord->coord.t[0] += (coord->coord.m[0][2] * speed) >> 12;
            coord->coord.t[2] += (coord->coord.m[2][2] * speed) >> 12;
            base               = gPlayerStatus.coordMtx->t[1] - 0x4B0;
            random2            = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16;
            y                  = coord->coord.t[1];
            if (y >= base + 400) {
                coord->coord.t[1] = y - (random2 & 0xF);
            } else {
                if (base - 400 >= y) {
                    newY = y + (random2 & 0xF);
                } else {
                    amountB = random2 & 0x1F;
                    if (work->flapFast != 0) {
                        newY = y - amountB;
                    } else {
                        newY = y + amountB;
                    }
                }
                coord->coord.t[1] = newY;
            }
            break;
    }
}
