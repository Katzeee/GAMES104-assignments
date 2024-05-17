#version 310 es

#extension GL_GOOGLE_include_directive : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_cur_frame;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform highp subpassInput in_scene_depth;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform highp subpassInput in_g_buffer_normal;
layout(set = 0, binding = 3) readonly buffer _unnamed
{
    mediump vec3 sample_point[64];
};

layout(location = 0) out highp vec4 out_color;

void main()
{
    highp vec3 color = subpassLoad(in_cur_frame).rgb;
    highp float scene_depth = subpassLoad(in_scene_depth).r;
    highp vec3 normal = subpassLoad(in_g_buffer_normal).rgb;
    out_color = vec4(color, 1.0f);
}