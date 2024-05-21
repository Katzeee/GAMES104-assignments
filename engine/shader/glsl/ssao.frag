#version 310 es

#extension GL_GOOGLE_include_directive : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_cur_frame;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform highp subpassInput in_scene_depth;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform highp subpassInput in_g_buffer_normal;

struct SamplePoint
{
    highp vec3 data;
    lowp float _point_padding;
};

layout(set = 0, binding = 3) readonly buffer _unnamed
{
    SamplePoint sample_point[64];
    highp float near_plane;
    highp float far_plane;
    lowp float _padding_plane_num_1;
    lowp float _padding_plane_num_2;
    highp mat4 proj_mat;
};

layout(location = 0) in highp vec2 in_texcoord;
layout(location = 0) out highp vec4 out_color;

highp vec2 uv_to_ndcxy(highp vec2 uv) { return uv * vec2(2.0, 2.0) + vec2(-1.0, -1.0); }

const highp float radius = 1.5f;
const highp float kernel_size = 64.0;

void main()
{
    highp vec3 color = subpassLoad(in_cur_frame).rgb;
    highp float scene_depth = subpassLoad(in_scene_depth).r;
    highp vec3 normal = normalize(subpassLoad(in_g_buffer_normal).rgb);
    highp float linear_depth = (near_plane * far_plane) / (far_plane - scene_depth * (far_plane - near_plane));
    highp vec3 view_position;
    {
        highp vec4  ndc                      = vec4(uv_to_ndcxy(in_texcoord), scene_depth, 1.0);
        highp mat4  inverse_proj_mat = inverse(proj_mat);
        highp vec4  view_position_with_w = inverse_proj_mat * ndc;
        view_position                    = view_position_with_w.xyz / view_position_with_w.www;
    }
    const highp vec3 up = vec3(0.0, 0.0, 1.0);
    highp vec3 tangent = normalize(cross(up, normal));
    highp vec3 bitangent = normalize(cross(normal, tangent));
    highp mat3 TBN = mat3(tangent, bitangent, normal);
    highp float oc = 0.0f;

    for (int i = 0; i < 64; i++)
    {
        highp vec3 sample_vec = TBN * sample_point[i].data;
        sample_vec = view_position + sample_vec * radius;
        highp vec4 offset = vec4(sample_vec, 1.0f);
        offset = proj_mat * offset;
        offset.xyz /= offset.w;
        // offset
        highp float sample_depth = texture2D(subpassLoad(in_scene_depth), offset.xy).r;

        highp float range_check = smoothstep(0.0, 1.0, radius / abs(view_position.z - sample_depth));
        oc += (sample_depth > sample_vec.z ? 1.0 : 0.0) * range_check;
    }

    highp vec3 foc = vec3(1.0 - oc / kernel_size);

    out_color = vec4(foc, 1.0f);
}