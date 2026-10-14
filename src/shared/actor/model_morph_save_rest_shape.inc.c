/* Part of the model morph library; see model_morph.h. */

/// Saves a model's rest XYZ components into a morph's borrowed snapshot buffers.
///
/// Requires a live TMD task and a readable morph record. `savedVertexCount`
/// and `normalCount` are nonnegative element counts from index zero, independent
/// of the delta range. The source and writable snapshot arrays must cover those
/// counts and must not overlap. Normals are saved only when `targetNormals` is
/// non-NULL; neither target array is read. Each vector's fourth halfword stays
/// intact. All storage is borrowed during this call; the saved XYZ values must
/// remain unchanged for subsequent blends using this rest shape.
static inline void _modelMorphSaveRestShape(const Task* task, const ModelMorph* morph)
{
    const TmdSource* source;
    SVECTOR*         savedVertices;
    SVECTOR*         savedNormals;
    const SVECTOR*   vertices;
    const SVECTOR*   normals;
    s32              elementIndex;

    source        = task->extra.tmd->source;
    savedVertices = morph->savedVertices;
    savedNormals  = morph->savedNormals;
    vertices      = source->verts;
    for (elementIndex = 0; elementIndex < morph->savedVertexCount; elementIndex++) {
        savedVertices[elementIndex].vx = vertices[elementIndex].vx;
        savedVertices[elementIndex].vy = vertices[elementIndex].vy;
        savedVertices[elementIndex].vz = vertices[elementIndex].vz;
    }
    if (morph->targetNormals != NULL) {
        normals = source->normals;
        for (elementIndex = 0; elementIndex < morph->normalCount; elementIndex++) {
            savedNormals[elementIndex].vx = normals[elementIndex].vx;
            savedNormals[elementIndex].vy = normals[elementIndex].vy;
            savedNormals[elementIndex].vz = normals[elementIndex].vz;
        }
    }
}
