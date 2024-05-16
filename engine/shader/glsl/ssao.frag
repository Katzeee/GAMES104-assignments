#version 310 es

#extension GL_GOOGLE_include_directive : enable

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_cur_frame;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform highp subpassInput in_scene_depth;

layout(location = 0) out highp vec4 out_color;

void main()
{
    out_color = vec4(1.0f);
}