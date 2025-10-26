#include "curve_mesh.h"

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot
{

namespace
{
static float _clamp_positive(float value, float fallback)
{
    return value > 0.0f ? value : fallback;
}

static float _get_project_texel_size()
{
    ProjectSettings *settings = ProjectSettings::get_singleton();
    if (settings == nullptr)
    {
        return 0.2f;
    }
    const Variant value = settings->get_setting(
        "rendering/lightmapping/primitive_meshes/texel_size");
    Variant::Type type = value.get_type();
    if (type != Variant::FLOAT && type != Variant::INT)
    {
        return 0.2f;
    }
    return _clamp_positive((float)value, 0.2f);
}

static float _get_width_curve_value(const Ref<Curve> &width_curve, float t,
                                    float default_value)
{
    if (width_curve.is_null())
    {
        return default_value;
    }
    return width_curve->sample(t);
}
} // namespace

CurveMesh::CurveMesh() {}

void CurveMesh::_update_lightmap_size()
{
    if (!get_add_uv2() || curve.is_null() || curve->get_point_count() <= 1)
    {
        return;
    }

    Vector2i lightmap_size_hint;
    const float padding = get_uv2_padding();
    float texel_size = _get_project_texel_size();

    float lightmap_length = curve->get_baked_length();
    if (extend_edges && !curve->is_closed())
    {
        float extra_length = 1.0f;
        if (width_curve.is_valid())
        {
            extra_length += width_curve->sample(0.0f);
            extra_length += width_curve->sample(1.0f);
        }
        lightmap_length += extra_length * width;
    }
    lightmap_size_hint.x = (int)Math::ceil(
        Math::max(1.0f, lightmap_length / texel_size) + 2.0f * padding);

    float lightmap_width = width;
    if (width_curve.is_valid())
    {
        lightmap_width *= Math::max(width_curve->get_max_value(),
                                    width_curve->get_min_value());
    }
    float width_padding = 1.0f;
    if (profile == PROFILE_CROSS)
    {
        lightmap_width *= (float)segments;
        width_padding *= (float)segments;
    }
    else if (profile == PROFILE_TUBE)
    {
        lightmap_width *= Math_PI;
        width_padding = 0.0f;
    }

    lightmap_size_hint.y = (int)Math::ceil(
        Math::max(1.0f, lightmap_width / texel_size) + width_padding * padding);
    set_lightmap_size_hint(lightmap_size_hint);
}

Array CurveMesh::_create_mesh_array() const
{
    PackedVector3Array points;
    PackedVector3Array normals;
    PackedFloat32Array tangents;
    PackedVector2Array uvs;
    PackedVector2Array uv2s;
    PackedInt32Array indices;

    const bool add_uv2 = get_add_uv2();
    const float uv2_padding = get_uv2_padding() * _get_project_texel_size();

    if (curve.is_valid() && curve->get_point_count() > 1)
    {
        LocalVector<CenterPoint> center_points;
        real_t total_length = 0.0;
        _generate_curve_points(center_points, total_length);

        if (center_points.size() >= 2)
        {
            LocalVector<EdgePoint> edge_points;
            const int radial_segments =
                (profile == PROFILE_FLAT) ? 1 : segments;
            _generate_edge_vertices(center_points, total_length,
                                    radial_segments, uv2_padding, edge_points);

            if (interleave_vertices && profile != PROFILE_TUBE)
            {
                _interleave_edge_vertices(edge_points, center_points,
                                          radial_segments);
            }

            if (filter_overlaps)
            {
                _filter_overlapping_vertices(edge_points, center_points,
                                             radial_segments);
            }

            _generate_triangles(edge_points, radial_segments, points, normals,
                                tangents, uvs, uv2s, indices);
        }
    }

    if (indices.is_empty())
    {
        points.push_back(Vector3());
        normals.push_back(Vector3(0.0, 1.0, 0.0));
        uvs.push_back(Vector2());
        if (add_uv2)
        {
            uv2s.push_back(Vector2());
        }
        tangents.push_back(1.0f);
        tangents.push_back(0.0f);
        tangents.push_back(0.0f);
        tangents.push_back(1.0f);
        indices.push_back(0);
        indices.push_back(0);
        indices.push_back(0);
    }

    Array arr;
    arr.resize(Mesh::ARRAY_MAX);
    arr[Mesh::ARRAY_VERTEX] = points;
    arr[Mesh::ARRAY_NORMAL] = normals;
    arr[Mesh::ARRAY_TANGENT] = tangents;
    arr[Mesh::ARRAY_TEX_UV] = uvs;
    if (add_uv2)
    {
        arr[Mesh::ARRAY_TEX_UV2] = uv2s;
    }
    arr[Mesh::ARRAY_INDEX] = indices;

    const_cast<CurveMesh *>(this)->_update_lightmap_size();

    return arr;
}

void CurveMesh::_generate_curve_points(
    LocalVector<CenterPoint> &center_points, real_t &total_length) const
{
    int point_count = 0;
    switch (tessellation_mode)
    {
    case TESSELLATION_BAKED:
    {
        PackedVector3Array pts = curve->get_baked_points();
        PackedFloat32Array tilts = curve->get_baked_tilts();
        point_count = pts.size();
        if (curve->is_closed())
        {
            point_count--;
        }
        center_points.resize(point_count);
        for (int i = 0; i < point_count; i++)
        {
            center_points[i].position = pts[i];
            center_points[i].tilt = tilts[i];
        }
    }
    break;
    case TESSELLATION_ADAPTIVE:
    {
        PackedVector3Array pts = curve->tessellate(5, tessellation_tolerance);
        point_count = pts.size();
        if (curve->is_closed())
        {
            point_count--;
        }
        center_points.resize(point_count);
        for (int i = 0; i < point_count; i++)
        {
            center_points[i].position = pts[i];
            center_points[i].tilt = 0.0f;
        }
    }
    break;
    case TESSELLATION_DISABLED:
    {
        point_count = curve->get_point_count();
        center_points.resize(point_count);
        for (int i = 0; i < point_count; i++)
        {
            center_points[i].position = curve->get_point_position(i);
            center_points[i].tilt = curve->get_point_tilt(i);
        }
    }
    break;
    }

    if (point_count < 2)
    {
        total_length = 0;
        return;
    }

    Vector3 next = center_points[1].position;
    Vector3 next_dir = (next - center_points[0].position).normalized();
    Vector3 prev_dir = next_dir;
    if (curve->is_closed())
    {
        prev_dir = (center_points[0].position -
                    center_points[point_count - 1].position)
                       .normalized();
    }

    center_points[0].tangent_prev = prev_dir;
    center_points[0].tangent_next = next_dir;

    total_length = 0.0;
    center_points[0].partial_length = total_length;

    if (extend_edges && !curve->is_closed())
    {
        float extra_width = width * 0.5f;
        if (width_curve.is_valid())
        {
            extra_width *= width_curve->sample(0.0f);
        }
        center_points[0].position -= next_dir * extra_width;
        total_length += extra_width;
    }

    for (int i = 1; i < point_count - 1; i++)
    {
        Vector3 prev_vec =
            center_points[i].position - center_points[i - 1].position;
        float prev_length = prev_vec.length();
        prev_dir = prev_vec.normalized();
        next_dir = (center_points[i + 1].position - center_points[i].position)
                       .normalized();
        total_length += prev_length;
        center_points[i].partial_length = total_length;
        center_points[i].tangent_prev = prev_dir;
        center_points[i].tangent_next = next_dir;
    }

    Vector3 prev_vec = center_points[point_count - 1].position -
                       center_points[point_count - 2].position;
    float prev_length = prev_vec.length();
    prev_dir = prev_vec.normalized();
    next_dir = prev_dir;
    total_length += prev_length;
    center_points[point_count - 1].partial_length = total_length;
    if (curve->is_closed())
    {
        next_dir = (center_points[0].position -
                    center_points[point_count - 1].position);
        float extra_length = next_dir.length();
        if (extra_length > 0.0f)
        {
            next_dir /= extra_length;
        }
        total_length += extra_length;
    }
    center_points[point_count - 1].tangent_prev = prev_dir;
    center_points[point_count - 1].tangent_next = next_dir;

    if (extend_edges && !curve->is_closed())
    {
        float extra_width = width * 0.5f;
        if (width_curve.is_valid())
        {
            extra_width *= width_curve->sample(1.0f);
        }
        center_points[point_count - 1].position += next_dir * extra_width;
        total_length += extra_width;
        center_points[point_count - 1].partial_length += extra_width;
    }

    if (!curve->is_closed())
    {
        center_points[point_count - 1].corner_point = true;
        center_points[0].corner_point = true;
    }
}

void CurveMesh::_generate_edge_vertices(
    LocalVector<CenterPoint> &center_points, real_t total_length,
    int radial_segments, float uv2_padding,
    LocalVector<EdgePoint> &edge_points) const
{
    const Vector3 up_vector_normalized = up_vector.normalized();
    float segment_angle = Math_PI;
    if (profile == PROFILE_CROSS)
    {
        segment_angle = Math_PI / radial_segments;
    }
    else if (profile == PROFILE_TUBE)
    {
        segment_angle = Math_PI * 2.0f / radial_segments;
    }

    const float horizontal_total = total_length + 2.0f * uv2_padding;
    const float length_h = (horizontal_total <= 0.0f || total_length <= 0.0f)
                               ? 0.0f
                               : (total_length / horizontal_total);
    const float padding_h =
        (horizontal_total <= 0.0f) ? 0.0f : (uv2_padding / horizontal_total);

    const float max_width_sample =
        width_curve.is_valid() ? Math::max(width_curve->get_max_value(),
                                           -width_curve->get_min_value())
                               : 1.0f;
    const float max_width = width * max_width_sample;
    const float length_v = 1.0f / radial_segments;
    const float edge_padding =
        length_v *
        ((profile == PROFILE_TUBE)
             ? 1.0f
             : (max_width <= 0.0f ? 1.0f
                                  : max_width / (max_width + uv2_padding)));

    Vector3 current_up = up_vector_normalized;

    const int point_count = center_points.size();
    const int edge_count = (profile == PROFILE_TUBE) ? 1 : 2;

    const float corner_scalar_threshold = Math::cos(corner_threshold);
    const bool zero_width = Math::is_zero_approx(width);
    const bool add_uv2 = get_add_uv2();

    edge_points.clear();
    edge_points.reserve(point_count * edge_count * radial_segments);

    for (int i = 0; i < point_count; i++)
    {
        float corner_cosine =
            center_points[i].tangent_prev.dot(center_points[i].tangent_next);
        center_points[i].corner_point =
            center_points[i].corner_point ||
            (corner_cosine < corner_scalar_threshold);

        float local_width = 1.0f;
        float u = (total_length > 0.0f)
                      ? center_points[i].partial_length / total_length
                      : 0.0f;

        if (width_curve.is_valid())
        {
            local_width = width_curve->sample(u);
        }

        Vector3 binormal;
        Vector3 spoke;
        Vector3 tangent_avg =
            (center_points[i].tangent_next + center_points[i].tangent_prev)
                .normalized();

        real_t width_correction = 1.0f;
        Vector3 width_correction_dir;

        if (!zero_width)
        {
            if (!follow_curve)
            {
                Vector3 local_up =
                    up_vector.slide(center_points[i].tangent_next).normalized();
                binormal = tangent_avg.cross(local_up);
            }
            else
            {
                binormal = tangent_avg.cross(current_up);
                current_up = binormal.cross(tangent_avg);
            }
            binormal.normalize();
            binormal = binormal.rotated(tangent_avg, center_points[i].tilt);
            spoke = binormal * width * local_width * 0.5f;

            width_correction = Math::sqrt(2.0f / (1.0f + corner_cosine));
            width_correction_dir =
                (center_points[i].tangent_prev - center_points[i].tangent_next)
                    .normalized();
        }
        else
        {
            binormal = Vector3(0.0f, 0.0f, 1.0f);
            spoke = Vector3(0.0f, 0.0f, 0.0f);
        }

        float v_offset = 0.5f;
        if (scale_uv_by_width)
        {
            v_offset *= local_width;
        }

        EdgePoint base_point;
        Vector3 tangent = tangent_avg;
        if (!smooth_shaded_corners && center_points[i].corner_point)
        {
            tangent = center_points[i].tangent_prev;
        }

        Vector3 normal = -tangent.cross(binormal).normalized();
        if (add_uv2)
        {
            base_point.uv2.x = padding_h + u * length_h;
        }
        if (scale_uv_by_length)
        {
            u *= total_length;
        }
        base_point.uv.x = u;
        base_point.tangent = tangent;

        for (int e = 0; e < edge_count; e++)
        {
            int edge = e * 2 - 1;
            for (int j = 0; j < radial_segments; j++)
            {
                EdgePoint point = base_point;
                if (!zero_width)
                {
                    float angle = j * segment_angle;
                    Vector3 spoke_rotated = spoke.rotated(tangent_avg, angle);

                    Vector3 stretched_component =
                        spoke_rotated.dot(width_correction_dir) *
                        width_correction_dir;
                    Vector3 fixed_component =
                        spoke_rotated - stretched_component;
                    spoke_rotated = width_correction * stretched_component +
                                    fixed_component;

                    point.position =
                        center_points[i].position + edge * spoke_rotated;

                    Vector3 normal_rotated = (profile == PROFILE_TUBE)
                                                 ? -edge * normal.cross(tangent)
                                                 : normal;
                    normal_rotated = normal_rotated.rotated(tangent, angle);
                    point.normal = normal_rotated;
                }
                else
                {
                    point.position = center_points[i].position;
                    point.normal = normal;
                }

                if (profile == PROFILE_CROSS && tile_segment_uv)
                {
                    point.uv.y = (e + j) * length_v;
                }
                else if (profile == PROFILE_TUBE)
                {
                    point.uv.y = j * length_v;
                }
                else
                {
                    point.uv.y = 0.5f + edge * v_offset;
                }

                if (add_uv2)
                {
                    point.uv2.y = e * edge_padding + j * length_v;
                }

                int index = edge_points.size();
                if (index >= radial_segments)
                {
                    point.prev_point = index - radial_segments;
                    edge_points[point.prev_point].next_point = index;
                }

                point.source_index = i;
                point.edge = e;
                edge_points.push_back(point);
            }
        }

        if (!smooth_shaded_corners && center_points[i].corner_point)
        {
            tangent = center_points[i].tangent_next;
            normal = -tangent.cross(binormal).normalized();

            for (int e = 0; e < edge_count; e++)
            {
                int edge = e * 2 - 1;
                for (int j = 0; j < radial_segments; j++)
                {
                    int duplicated_index =
                        edge_points.size() - radial_segments * edge_count;
                    EdgePoint point = edge_points[duplicated_index];
                    point.tangent = tangent;
                    Vector3 normal_rotated = (profile == PROFILE_TUBE)
                                                 ? -edge * normal.cross(tangent)
                                                 : normal;
                    normal_rotated = normal_rotated.rotated(
                        tangent, (float)j * segment_angle);
                    point.normal = normal_rotated;
                    int index = edge_points.size();
                    point.prev_point = index - radial_segments;
                    edge_points[point.prev_point].next_point = index;
                    edge_points[duplicated_index].next_connected = false;
                    point.prev_connected = false;
                    edge_points.push_back(point);
                }
            }
        }
    }

    for (int j = 0; j < radial_segments; j++)
    {
        int last_index = edge_points.size() - radial_segments + j;
        edge_points[last_index].next_point = j;
        edge_points[j].prev_point = last_index;
        if (!curve->is_closed())
        {
            for (int e = 0; e < edge_count; e++)
            {
                int base = e * radial_segments;
                edge_points[j + base].prev_connected = false;
                edge_points[last_index - (edge_count - 1 - e) * radial_segments]
                    .next_connected = false;
            }
        }
    }
}

void CurveMesh::_interleave_edge_vertices(
    LocalVector<EdgePoint> &edge_points,
    LocalVector<CenterPoint> &center_points, int radial_segments) const
{
    auto remove_point = [&edge_points](EdgePoint &point)
    {
        edge_points[point.prev_point].next_point = point.next_point;
        edge_points[point.next_point].prev_point = point.prev_point;
    };

    for (int j = 0; j < radial_segments; j++)
    {
        EdgePoint *point = &edge_points[j];
        int point_index = 0;
        while (point->next_point >= point_index)
        {
            point_index = point->next_point;
            EdgePoint *next_point = &edge_points[point->next_point];
            if (center_points[point->source_index].corner_point ||
                center_points[next_point->source_index].corner_point ||
                point->source_index == next_point->source_index)
            {
                point = next_point;
                continue;
            }
            remove_point(*point);
            remove_point(*next_point);
            point->removed = true;
            next_point->removed = true;
            point = &edge_points[next_point->next_point];
            point = &edge_points[point->next_point];
            point = &edge_points[point->next_point];
        }
    }
}

void CurveMesh::_filter_overlapping_vertices(
    LocalVector<EdgePoint> &edge_points,
    LocalVector<CenterPoint> &center_points, int radial_segments) const
{
    auto remove_point = [&edge_points](EdgePoint &point)
    {
        edge_points[point.prev_point].next_point = point.next_point;
        edge_points[point.next_point].prev_point = point.prev_point;
    };

    bool points_removed = true;
    while (points_removed)
    {
        points_removed = false;
        for (int j = 0; j < radial_segments; j++)
        {
            int point_index = j;
            int last_index = -1;
            EdgePoint *point = &edge_points[point_index];
            int next_index = point->next_point;
            EdgePoint *next_point = &edge_points[next_index];

            while (point_index > last_index)
            {
                if (next_index < point_index && !curve->is_closed())
                {
                    break;
                }
                if (next_point->edge == point->edge)
                {
                    Vector3 center_dir =
                        center_points[next_point->source_index].position -
                        center_points[point->source_index].position;
                    Vector3 next_dir = next_point->position - point->position;
                    if (next_dir.dot(center_dir) < 0.0f)
                    {
                        point->filter = true;
                        next_point->filter = true;
                    }

                    if (profile == PROFILE_TUBE)
                    {
                        const EdgePoint *top_point =
                            &edge_points[point_index - j +
                                         ((j + 1) % radial_segments)];
                        const EdgePoint *bottom_point =
                            &edge_points[next_index - j +
                                         ((j + radial_segments - 1) %
                                          radial_segments)];

                        while (top_point->filter)
                        {
                            if (center_points[top_point->source_index]
                                    .corner_point)
                            {
                                break;
                            }
                            top_point = &edge_points[top_point->prev_point];
                        }

                        while (bottom_point->filter)
                        {
                            if (center_points[bottom_point->source_index]
                                    .corner_point)
                            {
                                break;
                            }
                            bottom_point =
                                &edge_points[bottom_point->next_point];
                        }

                        Vector3 top_dir = top_point->position - point->position;
                        Vector3 bottom_dir =
                            bottom_point->position - next_point->position;
                        if (top_dir.cross(next_dir).dot(point->normal) < 0.0f)
                        {
                            Vector3 top_side =
                                top_point->position -
                                center_points[top_point->source_index].position;
                            Vector3 next_side =
                                next_point->position -
                                center_points[next_point->source_index]
                                    .position;
                            Vector3 point_side =
                                point->position -
                                center_points[point->source_index].position;
                            if (top_side.dot(point_side) > 0.0f &&
                                top_side.dot(next_side) > 0.0f)
                            {
                                point->filter = true;
                            }
                        }

                        if (next_dir.cross(bottom_dir).dot(point->normal) <
                            0.0f)
                        {
                            Vector3 bottom_side =
                                bottom_point->position -
                                center_points[bottom_point->source_index]
                                    .position;
                            Vector3 next_side =
                                next_point->position -
                                center_points[next_point->source_index]
                                    .position;
                            Vector3 point_side =
                                point->position -
                                center_points[point->source_index].position;
                            if (bottom_side.dot(point_side) > 0.0f &&
                                bottom_side.dot(next_side) > 0.0f)
                            {
                                next_point->filter = true;
                            }
                        }
                    }

                    last_index = point_index;
                    point_index = point->next_point;
                    point = &edge_points[point_index];
                    next_index = point->next_point;
                    next_point = &edge_points[next_index];
                }
                else
                {
                    next_index = next_point->next_point;
                    next_point = &edge_points[next_index];
                }
            }
        }

        for (uint32_t k = 0; k < edge_points.size(); ++k)
        {
            EdgePoint *point = &edge_points[k];
            if (point->filter && !point->removed)
            {
                if (center_points[point->source_index].corner_point ||
                    point->next_point == point->prev_point)
                {
                    point->filter = false;
                }
            }
        }

        for (uint32_t k = 0; k < edge_points.size(); ++k)
        {
            EdgePoint *point = &edge_points[k];
            if (point->filter && !point->removed)
            {
                LocalVector<int> group_indices;
                group_indices.push_back(k);

                int next_idx = point->next_point;
                while (next_idx < (int)edge_points.size())
                {
                    if (edge_points[next_idx].edge == point->edge)
                    {
                        if (edge_points[next_idx].filter)
                        {
                            group_indices.push_back(next_idx);
                        }
                        else
                        {
                            break;
                        }
                    }
                    next_idx = edge_points[next_idx].next_point;
                }

                if (group_indices.size() > 1)
                {
                    int first_idx = group_indices[0];
                    int last_idx = group_indices[group_indices.size() - 1];

                    int before_idx = edge_points[first_idx].prev_point;
                    while (before_idx >= 0)
                    {
                        if (edge_points[before_idx].edge == point->edge &&
                            !edge_points[before_idx].filter)
                        {
                            break;
                        }
                        else
                        {
                            before_idx = edge_points[before_idx].prev_point;
                        }
                    }

                    int after_idx = edge_points[last_idx].next_point;
                    while (after_idx < (int)edge_points.size())
                    {
                        if (edge_points[after_idx].edge == point->edge &&
                            !edge_points[after_idx].filter)
                        {
                            break;
                        }
                        else
                        {
                            after_idx = edge_points[after_idx].next_point;
                        }
                    }

                    EdgePoint *kept_point = &edge_points[first_idx];

                    if (before_idx >= 0 &&
                        after_idx < (int)edge_points.size() &&
                        edge_points[before_idx].edge == point->edge &&
                        edge_points[after_idx].edge == point->edge)
                    {
                        EdgePoint *before_point = &edge_points[before_idx];
                        EdgePoint *after_point = &edge_points[after_idx];

                        Vector3 before_pos = before_point->position;
                        Vector3 after_pos = after_point->position;
                        Vector3 before_tangent = before_point->tangent;
                        Vector3 after_tangent = after_point->tangent;

                        Vector3 w0 = before_pos - after_pos;
                        float a = before_tangent.dot(before_tangent);
                        float b = before_tangent.dot(after_tangent);
                        float c = after_tangent.dot(after_tangent);
                        float d = before_tangent.dot(w0);
                        float e = after_tangent.dot(w0);

                        float denom = a * c - b * b;
                        Vector3 tangent_position;

                        if (Math::abs(denom) > CMP_EPSILON)
                        {
                            float t1 = (b * e - c * d) / denom;
                            float t2 = (a * e - b * d) / denom;

                            Vector3 point1 = before_pos + t1 * before_tangent;
                            Vector3 point2 = after_pos + t2 * after_tangent;
                            tangent_position = (point1 + point2) * 0.5f;
                        }
                        else
                        {
                            tangent_position = (before_pos + after_pos) * 0.5f;
                        }

                        kept_point->position = tangent_position;
                        kept_point->normal =
                            (before_point->normal + after_point->normal)
                                .normalized();
                        kept_point->tangent =
                            (before_point->tangent + after_point->tangent)
                                .normalized();
                        kept_point->uv =
                            (before_point->uv + after_point->uv) * 0.5f;
                        kept_point->uv2 =
                            (before_point->uv2 + after_point->uv2) * 0.5f;
                    }

                    kept_point->filter = false;

                    for (uint32_t i = 1; i < group_indices.size(); ++i)
                    {
                        EdgePoint *remove_candidate =
                            &edge_points[group_indices[i]];
                        remove_point(*remove_candidate);
                        remove_candidate->removed = true;
                        remove_candidate->filter = false;
                        points_removed = true;
                    }
                }
                else
                {
                    remove_point(*point);
                    point->removed = true;
                    point->filter = false;
                    points_removed = true;
                }
            }
        }
    }
}

void CurveMesh::_generate_triangles(
    LocalVector<EdgePoint> &edge_points, int radial_segments,
    PackedVector3Array &points, PackedVector3Array &normals,
    PackedFloat32Array &tangents, PackedVector2Array &uvs,
    PackedVector2Array &uv2s, PackedInt32Array &indices) const
{
    const bool add_uv2 = get_add_uv2();
    auto add_point = [&](const EdgePoint &edge_point)
    {
        points.push_back(edge_point.position);
        normals.push_back(edge_point.normal);
        uvs.push_back(edge_point.uv);
        if (add_uv2)
        {
            uv2s.push_back(edge_point.uv2);
        }
        tangents.push_back(edge_point.tangent.x);
        tangents.push_back(edge_point.tangent.y);
        tangents.push_back(edge_point.tangent.z);
        tangents.push_back(1.0f);
    };

    for (uint32_t k = 0; k < edge_points.size(); ++k)
    {
        EdgePoint *point = &edge_points[k];
        if (!point->removed)
        {
            point->source_index = points.size();
            add_point(*point);
        }
    }

    if (profile != PROFILE_TUBE)
    {
        for (int j = 0; j < radial_segments; j++)
        {
            const EdgePoint *point = &edge_points[j];
            const EdgePoint *last_edge_idx[2] = {nullptr, nullptr};

            int stop_index = point->next_point;
            const EdgePoint *stop_point = &edge_points[stop_index];

            while (stop_point->edge == point->edge)
            {
                point = stop_point;
                stop_index = point->next_point;
                stop_point = &edge_points[stop_index];
            }

            last_edge_idx[point->edge] = point;
            last_edge_idx[stop_point->edge] = stop_point;
            point = stop_point;
            int point_index;
            do
            {
                point_index = point->next_point;
                point = &edge_points[point_index];

                bool skip_face = false;

                if (!last_edge_idx[0]->next_connected &&
                    !last_edge_idx[1]->next_connected)
                {
                    skip_face = true;
                }

                if (!point->prev_connected &&
                    !last_edge_idx[1 - point->edge]->prev_connected)
                {
                    skip_face = true;
                }

                if (!skip_face)
                {
                    indices.push_back(last_edge_idx[1]->source_index);
                    indices.push_back(last_edge_idx[0]->source_index);
                    indices.push_back(point->source_index);
                }

                last_edge_idx[point->edge] = &edge_points[point_index];
            } while (point_index != stop_index);
        }
    }
    else
    {
        for (uint32_t i = 0; i < edge_points.size(); i += radial_segments)
        {
            for (int j = 0; j < radial_segments; j++)
            {
                int point_index = i + j;
                const EdgePoint *point = &edge_points[point_index];
                if (point->removed)
                {
                    continue;
                }
                EdgePoint *next_point = &edge_points[point->next_point];
                EdgePoint *top_point =
                    &edge_points[i + ((j + 1) % radial_segments)];
                EdgePoint *bottom_point =
                    &edge_points[point->next_point - j +
                                 ((j + radial_segments - 1) % radial_segments)];

                while (top_point->removed)
                {
                    top_point = &edge_points[top_point->prev_point];
                }

                if (next_point->prev_connected || top_point->next_connected)
                {
                    indices.push_back(point->source_index);
                    indices.push_back(next_point->source_index);
                    indices.push_back(top_point->source_index);
                }

                while (bottom_point->removed)
                {
                    bottom_point = &edge_points[bottom_point->next_point];
                }

                if (point->next_connected || bottom_point->prev_connected)
                {
                    indices.push_back(point->source_index);
                    indices.push_back(bottom_point->source_index);
                    indices.push_back(next_point->source_index);
                }
            }
        }
    }
}

void CurveMesh::_validate_property(PropertyInfo &p_property) const
{
    static const StringName tessellation_tolerance_name(
        "tessellation_tolerance");
    static const StringName segments_name("segments");
    static const StringName tile_segment_uv_name("tile_segment_uv");
    static const StringName interleave_vertices_name("interleave_vertices");
    static const StringName scale_uv_by_width_name("scale_uv_by_width");

    const StringName &property_name = p_property.name;

    if (property_name == tessellation_tolerance_name)
    {
        if (tessellation_mode == TESSELLATION_ADAPTIVE)
        {
            p_property.usage = PROPERTY_USAGE_DEFAULT;
        }
        else
        {
            p_property.usage = PROPERTY_USAGE_NO_EDITOR;
        }
    }
    else if (property_name == segments_name)
    {
        if (profile == PROFILE_FLAT)
        {
            p_property.usage = PROPERTY_USAGE_NO_EDITOR;
        }
        else
        {
            p_property.usage = PROPERTY_USAGE_DEFAULT;
            if (profile == PROFILE_TUBE)
            {
                p_property.hint_string = "3,100,1,or_greater";
            }
            else
            {
                p_property.hint_string = "2,100,1,or_greater";
            }
        }
    }
    else if (property_name == tile_segment_uv_name)
    {
        p_property.usage = (profile == PROFILE_CROSS)
                               ? PROPERTY_USAGE_DEFAULT
                               : PROPERTY_USAGE_NO_EDITOR;
    }
    else if (property_name == interleave_vertices_name)
    {
        p_property.usage = (profile == PROFILE_TUBE) ? PROPERTY_USAGE_NO_EDITOR
                                                     : PROPERTY_USAGE_DEFAULT;
    }
    else if (property_name == scale_uv_by_width_name)
    {
        p_property.usage = (profile == PROFILE_FLAT) ? PROPERTY_USAGE_DEFAULT
                                                     : PROPERTY_USAGE_NO_EDITOR;
    }
}

void CurveMesh::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_curve", "curve"),
                         &CurveMesh::set_curve);
    ClassDB::bind_method(D_METHOD("get_curve"), &CurveMesh::get_curve);

    ClassDB::bind_method(D_METHOD("set_width", "width"),
                         &CurveMesh::set_width);
    ClassDB::bind_method(D_METHOD("get_width"), &CurveMesh::get_width);

    ClassDB::bind_method(D_METHOD("set_width_curve", "curve"),
                         &CurveMesh::set_width_curve);
    ClassDB::bind_method(D_METHOD("get_width_curve"),
                         &CurveMesh::get_width_curve);

    ClassDB::bind_method(D_METHOD("set_extend_edges", "extend_edges"),
                         &CurveMesh::set_extend_edges);
    ClassDB::bind_method(D_METHOD("is_extend_edges"),
                         &CurveMesh::is_extend_edges);

    ClassDB::bind_method(D_METHOD("set_scale_uv_by_length", "enable"),
                         &CurveMesh::set_scale_uv_by_length);
    ClassDB::bind_method(D_METHOD("is_scale_uv_by_length"),
                         &CurveMesh::is_scale_uv_by_length);

    ClassDB::bind_method(D_METHOD("set_scale_uv_by_width", "enable"),
                         &CurveMesh::set_scale_uv_by_width);
    ClassDB::bind_method(D_METHOD("is_scale_uv_by_width"),
                         &CurveMesh::is_scale_uv_by_width);

    ClassDB::bind_method(D_METHOD("set_tile_segment_uv", "enable"),
                         &CurveMesh::set_tile_segment_uv);
    ClassDB::bind_method(D_METHOD("is_tile_segment_uv"),
                         &CurveMesh::is_tile_segment_uv);

    ClassDB::bind_method(D_METHOD("set_tessellation_mode", "mode"),
                         &CurveMesh::set_tessellation_mode);
    ClassDB::bind_method(D_METHOD("get_tessellation_mode"),
                         &CurveMesh::get_tessellation_mode);

    ClassDB::bind_method(D_METHOD("set_tessellation_tolerance", "tolerance"),
                         &CurveMesh::set_tessellation_tolerance);
    ClassDB::bind_method(D_METHOD("get_tessellation_tolerance"),
                         &CurveMesh::get_tessellation_tolerance);

    ClassDB::bind_method(D_METHOD("set_corner_threshold", "corner_threshold"),
                         &CurveMesh::set_corner_threshold);
    ClassDB::bind_method(D_METHOD("get_corner_threshold"),
                         &CurveMesh::get_corner_threshold);

    ClassDB::bind_method(D_METHOD("set_smooth_shaded_corners", "enable"),
                         &CurveMesh::set_smooth_shaded_corners);
    ClassDB::bind_method(D_METHOD("is_smooth_shaded_corners"),
                         &CurveMesh::is_smooth_shaded_corners);

    ClassDB::bind_method(D_METHOD("set_interleave_vertices", "enable"),
                         &CurveMesh::set_interleave_vertices);
    ClassDB::bind_method(D_METHOD("is_interleave_vertices"),
                         &CurveMesh::is_interleave_vertices);

    ClassDB::bind_method(D_METHOD("set_filter_overlaps", "enable"),
                         &CurveMesh::set_filter_overlaps);
    ClassDB::bind_method(D_METHOD("is_filter_overlaps"),
                         &CurveMesh::is_filter_overlaps);

    ClassDB::bind_method(D_METHOD("set_up_vector", "up_vector"),
                         &CurveMesh::set_up_vector);
    ClassDB::bind_method(D_METHOD("get_up_vector"),
                         &CurveMesh::get_up_vector);

    ClassDB::bind_method(D_METHOD("set_follow_curve", "follow"),
                         &CurveMesh::set_follow_curve);
    ClassDB::bind_method(D_METHOD("is_follow_curve"),
                         &CurveMesh::is_follow_curve);

    ClassDB::bind_method(D_METHOD("set_profile", "profile"),
                         &CurveMesh::set_profile);
    ClassDB::bind_method(D_METHOD("get_profile"), &CurveMesh::get_profile);

    ClassDB::bind_method(D_METHOD("set_segments", "segments"),
                         &CurveMesh::set_segments);
    ClassDB::bind_method(D_METHOD("get_segments"), &CurveMesh::get_segments);

    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "curve",
                              PROPERTY_HINT_RESOURCE_TYPE, "Curve3D"),
                 "set_curve", "get_curve");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width", PROPERTY_HINT_RANGE,
                              "0.0,2.0,0.001,or_greater"),
                 "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "width_curve",
                              PROPERTY_HINT_RESOURCE_TYPE, "Curve"),
                 "set_width_curve", "get_width_curve");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scale_uv_by_width"),
                 "set_scale_uv_by_width", "is_scale_uv_by_width");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "profile", PROPERTY_HINT_ENUM,
                              "Flat,Cross,Tube"),
                 "set_profile", "get_profile");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "segments", PROPERTY_HINT_RANGE,
                              "2,100,1,or_greater"),
                 "set_segments", "get_segments");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tile_segment_uv",
                              PROPERTY_HINT_NONE,
                              "hint_tooltip:Tile UVs for each segment."),
                 "set_tile_segment_uv", "is_tile_segment_uv");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "extend_edges", PROPERTY_HINT_NONE,
                              "hint_tooltip:Extend edges to cover the curve."),
                 "set_extend_edges", "is_extend_edges");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "up_vector", PROPERTY_HINT_NONE,
                              "hint_tooltip:Up vector for the curve."),
                 "set_up_vector", "get_up_vector");
    ADD_PROPERTY(
        PropertyInfo(
            Variant::BOOL, "follow_curve", PROPERTY_HINT_NONE,
            "hint_tooltip:Follow the curve's tilt instead of up vector."),
        "set_follow_curve", "is_follow_curve");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "tessellation_mode",
                              PROPERTY_HINT_ENUM, "Adaptive,Baked,Disabled"),
                 "set_tessellation_mode", "get_tessellation_mode");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tessellation_tolerance",
                              PROPERTY_HINT_RANGE,
                              "0.001,16.0,0.001,or_greater,suffix:m"),
                 "set_tessellation_tolerance", "get_tessellation_tolerance");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "corner_threshold",
                              PROPERTY_HINT_RANGE,
                              "0.0,180.0,0.1,radians_as_degrees"),
                 "set_corner_threshold", "get_corner_threshold");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "smooth_shaded_corners",
                              PROPERTY_HINT_NONE,
                              "hint_tooltip:Smooth shaded corners."),
                 "set_smooth_shaded_corners", "is_smooth_shaded_corners");
    ADD_PROPERTY(
        PropertyInfo(
            Variant::BOOL, "interleave_vertices", PROPERTY_HINT_NONE,
            "hint_tooltip:Interleave vertices to reduce vertex count."),
        "set_interleave_vertices", "is_interleave_vertices");
    ADD_PROPERTY(
        PropertyInfo(Variant::BOOL, "filter_overlaps", PROPERTY_HINT_NONE),
        "set_filter_overlaps", "is_filter_overlaps");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scale_uv_by_length"),
                 "set_scale_uv_by_length", "is_scale_uv_by_length");

    BIND_ENUM_CONSTANT(TESSELLATION_BAKED);
    BIND_ENUM_CONSTANT(TESSELLATION_DISABLED);
    BIND_ENUM_CONSTANT(TESSELLATION_ADAPTIVE);

    BIND_ENUM_CONSTANT(PROFILE_FLAT);
    BIND_ENUM_CONSTANT(PROFILE_CROSS);
    BIND_ENUM_CONSTANT(PROFILE_TUBE);
}

void CurveMesh::set_curve(const Ref<Curve3D> &p_curve)
{
    if (curve == p_curve)
    {
        return;
    }

    Callable update_callable(this, "request_update");

    if (curve.is_valid())
    {
        curve->disconnect("changed", update_callable);
    }

    curve = p_curve;

    if (curve.is_valid())
    {
        curve->connect("changed", update_callable,
                       Object::CONNECT_REFERENCE_COUNTED);
    }

rest_request_update:
    request_update();
}

Ref<Curve3D> CurveMesh::get_curve() const { return curve; }

void CurveMesh::set_width(float p_width)
{
    if (!Math::is_equal_approx(width, p_width))
    {
        width = p_width;
        request_update();
    }
}

float CurveMesh::get_width() const { return width; }

void CurveMesh::set_width_curve(const Ref<Curve> &p_curve)
{
    if (width_curve == p_curve)
    {
        return;
    }

    Callable update_callable(this, "request_update");

    if (width_curve.is_valid())
    {
        width_curve->disconnect("changed", update_callable);
    }

    width_curve = p_curve;

    if (width_curve.is_valid())
    {
        width_curve->connect("changed", update_callable,
                             Object::CONNECT_REFERENCE_COUNTED);
    }

    notify_property_list_changed();
    request_update();
}

Ref<Curve> CurveMesh::get_width_curve() const { return width_curve; }

void CurveMesh::set_scale_uv_by_length(bool p_enable)
{
    if (scale_uv_by_length != p_enable)
    {
        scale_uv_by_length = p_enable;
        request_update();
    }
}

bool CurveMesh::is_scale_uv_by_length() const { return scale_uv_by_length; }

void CurveMesh::set_scale_uv_by_width(bool p_enable)
{
    if (scale_uv_by_width != p_enable)
    {
        scale_uv_by_width = p_enable;
        request_update();
    }
}

bool CurveMesh::is_scale_uv_by_width() const { return scale_uv_by_width; }

void CurveMesh::set_tile_segment_uv(bool p_enable)
{
    if (tile_segment_uv != p_enable)
    {
        tile_segment_uv = p_enable;
        request_update();
    }
}

bool CurveMesh::is_tile_segment_uv() const { return tile_segment_uv; }

void CurveMesh::set_interleave_vertices(bool p_enable)
{
    if (interleave_vertices != p_enable)
    {
        interleave_vertices = p_enable;
        request_update();
    }
}

bool CurveMesh::is_interleave_vertices() const { return interleave_vertices; }

void CurveMesh::set_filter_overlaps(bool p_enable)
{
    if (filter_overlaps != p_enable)
    {
        filter_overlaps = p_enable;
        request_update();
    }
}

bool CurveMesh::is_filter_overlaps() const { return filter_overlaps; }

void CurveMesh::set_tessellation_mode(TessellationMode p_mode)
{
    if (tessellation_mode != p_mode)
    {
        tessellation_mode = p_mode;
        notify_property_list_changed();
        request_update();
    }
}

CurveMesh::TessellationMode CurveMesh::get_tessellation_mode() const
{
    return tessellation_mode;
}

void CurveMesh::set_tessellation_tolerance(float p_tolerance)
{
    float clamped = p_tolerance < 0.001f ? 0.001f : p_tolerance;
    if (!Math::is_equal_approx(tessellation_tolerance, clamped))
    {
        tessellation_tolerance = clamped;
        request_update();
    }
}

float CurveMesh::get_tessellation_tolerance() const
{
    return tessellation_tolerance;
}

void CurveMesh::set_corner_threshold(float p_threshold)
{
    if (!Math::is_equal_approx(corner_threshold, p_threshold))
    {
        corner_threshold = p_threshold;
        request_update();
    }
}

float CurveMesh::get_corner_threshold() const { return corner_threshold; }

void CurveMesh::set_smooth_shaded_corners(bool p_enable)
{
    if (smooth_shaded_corners != p_enable)
    {
        smooth_shaded_corners = p_enable;
        request_update();
    }
}

bool CurveMesh::is_smooth_shaded_corners() const
{
    return smooth_shaded_corners;
}

void CurveMesh::set_up_vector(const Vector3 &p_up_vector)
{
    if (!up_vector.is_equal_approx(p_up_vector))
    {
        up_vector = p_up_vector;
        request_update();
    }
}

Vector3 CurveMesh::get_up_vector() const { return up_vector; }

void CurveMesh::set_follow_curve(bool p_enable)
{
    if (follow_curve != p_enable)
    {
        follow_curve = p_enable;
        request_update();
    }
}

bool CurveMesh::is_follow_curve() const { return follow_curve; }

void CurveMesh::set_profile(Profile p_profile)
{
    if (profile != p_profile)
    {
        profile = p_profile;
        if (profile == PROFILE_CROSS)
        {
            segments = Math::max(segments, 2);
        }
        else if (profile == PROFILE_TUBE)
        {
            segments = Math::max(segments, 3);
        }
        notify_property_list_changed();
        request_update();
    }
}

CurveMesh::Profile CurveMesh::get_profile() const { return profile; }

void CurveMesh::set_segments(int p_segments)
{
    int minimum = (profile == PROFILE_TUBE) ? 3 : 2;
    int clamped = Math::max(p_segments, minimum);
    if (segments != clamped)
    {
        segments = clamped;
        request_update();
    }
}

int CurveMesh::get_segments() const { return segments; }

void CurveMesh::set_extend_edges(bool p_enable)
{
    if (extend_edges != p_enable)
    {
        extend_edges = p_enable;
        request_update();
    }
}

bool CurveMesh::is_extend_edges() const { return extend_edges; }

} // namespace godot
