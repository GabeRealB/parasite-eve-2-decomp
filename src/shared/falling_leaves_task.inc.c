/* Part of the falling leaves library; see falling_leaves.h. */

/// One drifting mote of the room's ambient effect. The first tick seeds it
/// from `Gp_LcgState`: a size of 0x20, a random tilt pair (`period` /
/// `step`) and a random drift in `move`. While it flies, the drift
/// moves its coordinate frame and the tilt rotates it; each drift axis eases
/// back towards zero by one a tick and re-rolls a fresh multiple of 8 when it
/// gets there, and the tilt wanders by a random step. Once the frame has
/// risen past the origin the mote fades in by 0x10 a tick up to 0x80, then
/// fades back out and releases its work block.
static inline void leafFallTask(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        vy;
    s32        vx;
    s32        vz;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale   = 0x20;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step    = 0x80 - (((u32)Gp_LcgState >> 16) & 0xF0);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state   = 1;
            /* fallthrough */
        case 1:
            coord->coord.t[0] += work->move.vx;
            coord->coord.t[1] += work->move.vy;
            coord->coord.t[2] += work->move.vz;
            Gfx_RotMatrixX(&coord->coord, work->period, 0);
            Gfx_RotMatrixZ(&coord->coord, work->step, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;

            vy = work->move.vy;
            if (vy >= 0x1D) {
                vy = vy - 1;
            } else {
                vy = vy + 1;
            }
            work->move.vy = vy;

            vx = work->move.vx;
            if (vx == 0) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vx += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vx > 0) {
                    vx = vx - 1;
                } else {
                    vx = vx + 1;
                }
                work->move.vx = vx;
            }

            vz = work->move.vz;
            if (vz == 0) {
                work->move.vz += work->step % 32;
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vz += (2 - (u16)(((u32)Gp_LcgState >> 16) % 5U)) * 8;
            } else {
                if (vz > 0) {
                    vz = vz - 1;
                } else {
                    vz = vz + 1;
                }
                work->move.vz = vz;
            }

            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->step   += (1 - (u16)(((u32)Gp_LcgState >> 16) % 3U)) * 8;

            if (coord->coord.t[1] > 0) {
                task->state = 2;
            }
            leafDraw(coord, work->scale, 0);
            break;
        case 2:
            if (work->angle < 0x80) {
                work->angle += 0x10;
            } else {
                task->state = 3;
            }
            leafDraw(coord, work->scale, 0);
            break;
        case 3:
            if (work->angle >= 0x11) {
                work->angle -= 0x10;
                leafDraw(coord, work->scale, work->angle);
            } else {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}
