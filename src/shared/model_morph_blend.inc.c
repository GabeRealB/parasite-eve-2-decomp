/* Part of the model morph library; see model_morph.h. */

/// Morphs the task's model by `ramp` (0..0x1000): restores the snapshot's
/// `deltaCount` vertices from `firstVertex` into the model, adds each one's
/// `vertexDeltas` entry scaled by the ramp with `gteMIMefunc`, and, when the
/// morph has `targetNormals`, interpolates each normal from its saved value
/// toward them.
static void modelMorphBlend(Task* task, ModelMorph* morph, s32 ramp)
{
    s32        i;
    s32        count;
    s32        first;
    TmdSource* src;
    u16*       dst;
    u16*       from;
    u16*       dstMid;
    u16*       fromMid;
    SVECTOR*   nrm;
    SVECTOR*   nrmA;
    SVECTOR*   nrmB;
    SVECTOR*   nrmDst;
    s32        blend;
    s32        inv;
    u16        vx;
    u16        vz;

    i     = 0;
    count = morph->deltaCount;
    src   = task->extra.tmd->source;
    first = morph->firstVertex;
    from  = (u16*)&morph->savedVertices[first];
    nrm   = src->normals;
    dst   = (u16*)&src->verts[first];
    if (count > 0) {
        fromMid = from + 2;
        dstMid  = dst + 2;
        do {
            vx         = *from;
            from      += 4;
            i         += 1;
            *dst       = vx;
            dst       += 4;
            dstMid[-1] = fromMid[-1];
            vz         = fromMid[0];
            fromMid   += 4;
            dstMid[0]  = vz;
            dstMid    += 4;
        } while (i < count);
    }
    blend = ramp;
    inv   = 0x1000 - blend;
    gteMIMefunc(src->verts + morph->firstVertex, morph->vertexDeltas, morph->deltaCount, blend);
    nrmA = morph->targetNormals;
    if (nrmA != NULL) {
        count = morph->normalCount;
        nrmB  = morph->savedNormals;
        i     = 0;
        if (count > 0) {
            do {
                gte_lddp(blend);
                gte_ldsv(nrmA);
                gte_gpf12();
                nrmDst = nrm + i;
                gte_lddp(inv);
                gte_ldsv(nrmB);
                gte_gpl12();
                nrmB++;
                i++;
                nrmA++;
                gte_stsv(nrmDst);
            } while (i < count);
        }
    }
}
