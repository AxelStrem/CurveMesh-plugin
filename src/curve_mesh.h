#ifndef CURVE_MESH_H
#define CURVE_MESH_H

#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot
{

class CurveMesh : public PrimitiveMesh
{
  GDCLASS(CurveMesh, PrimitiveMesh);

  public:
    enum TessellationMode
    {
        TESSELLATION_ADAPTIVE,
        TESSELLATION_BAKED,
        TESSELLATION_DISABLED,
    };

    enum Profile
    {
        PROFILE_FLAT,
        PROFILE_CROSS,
        PROFILE_TUBE,
    };

  private:
    struct CenterPoint
    {
        Vector3 position;
        Vector3 tangent_next;
        Vector3 tangent_prev;
        float partial_length = 0.0f;
        float tilt = 0.0f;
        bool corner_point = false;
    };

    struct EdgePoint
    {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;
        Vector2 uv2;
        Vector3 tangent;
        int source_index = -1;
        int next_point = -1;
        int prev_point = -1;
        int edge = 0;
        bool filter = false;
        bool removed = false;
        bool next_connected = true;
        bool prev_connected = true;
    };

  private:
    Ref<Curve3D> curve;
    float width = 0.5f;
    Ref<Curve> width_curve;
    bool extend_edges = false;

    TessellationMode tessellation_mode = TESSELLATION_BAKED;
    float tessellation_tolerance = 4.0f;

    Vector3 up_vector = Vector3(0.0f, 1.0f, 0.0f);
    bool follow_curve = true;
    float corner_threshold = 0.5236f;
    bool smooth_shaded_corners = true;

    Profile profile = PROFILE_FLAT;
    int segments = 2;

    bool interleave_vertices = false;
    bool filter_overlaps = false;

    bool scale_uv_by_length = false;
    bool scale_uv_by_width = false;
    bool tile_segment_uv = true;

  private:
    void _update_lightmap_size();
    void _generate_curve_points(LocalVector<CenterPoint> &center_points,
                                real_t &total_length) const;
    void _generate_edge_vertices(LocalVector<CenterPoint> &center_points,
                                 real_t total_length, int radial_segments,
                                 float uv2_padding,
                                 LocalVector<EdgePoint> &edge_points) const;
    void _interleave_edge_vertices(LocalVector<EdgePoint> &edge_points,
                                   LocalVector<CenterPoint> &center_points,
                                   int radial_segments) const;
    void _filter_overlapping_vertices(LocalVector<EdgePoint> &edge_points,
                                      LocalVector<CenterPoint> &center_points,
                                      int radial_segments) const;
    void _generate_triangles(LocalVector<EdgePoint> &edge_points,
                             int radial_segments, PackedVector3Array &points,
                             PackedVector3Array &normals,
                             PackedFloat32Array &tangents,
                             PackedVector2Array &uvs, PackedVector2Array &uv2s,
                             PackedInt32Array &indices) const;

  protected:
    static void _bind_methods();
    void _validate_property(PropertyInfo &p_property) const;

  public:
    Array _create_mesh_array() const override;
    void set_curve(const Ref<Curve3D> &p_curve);
    Ref<Curve3D> get_curve() const;

    void set_width(float p_width);
    float get_width() const;

    void set_width_curve(const Ref<Curve> &p_curve);
    Ref<Curve> get_width_curve() const;

    void set_scale_uv_by_length(bool p_enable);
    bool is_scale_uv_by_length() const;

    void set_scale_uv_by_width(bool p_enable);
    bool is_scale_uv_by_width() const;

    void set_tile_segment_uv(bool p_enable);
    bool is_tile_segment_uv() const;

    void set_interleave_vertices(bool p_enable);
    bool is_interleave_vertices() const;

    void set_filter_overlaps(bool p_enable);
    bool is_filter_overlaps() const;

    void set_tessellation_mode(TessellationMode p_mode);
    TessellationMode get_tessellation_mode() const;

    void set_tessellation_tolerance(float p_tolerance);
    float get_tessellation_tolerance() const;

    void set_corner_threshold(float p_threshold);
    float get_corner_threshold() const;

    void set_smooth_shaded_corners(bool p_enable);
    bool is_smooth_shaded_corners() const;

    void set_up_vector(const Vector3 &p_up_vector);
    Vector3 get_up_vector() const;

    void set_follow_curve(bool p_enable);
    bool is_follow_curve() const;

    void set_profile(Profile p_profile);
    Profile get_profile() const;

    void set_segments(int p_segments);
    int get_segments() const;

    void set_extend_edges(bool p_enable);
    bool is_extend_edges() const;

  static constexpr const char *get_class_icon_path()
  {
    return "res://addons/curve_mesh/icons/CurveMesh.svg";
  }

  CurveMesh();
};

} // namespace godot

VARIANT_ENUM_CAST(godot::CurveMesh::TessellationMode)
VARIANT_ENUM_CAST(godot::CurveMesh::Profile)

#endif // CURVE_MESH_H
