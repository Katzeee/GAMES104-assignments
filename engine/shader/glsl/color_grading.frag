#version 310 es

#extension GL_GOOGLE_include_directive : enable

#include "constants.h"

layout(input_attachment_index = 0, set = 0, binding = 0) uniform highp subpassInput in_color;

layout(set = 0, binding = 1) uniform sampler2D color_grading_lut_texture_sampler;

layout(location = 0) out highp vec4 out_color;

void main()
{
    ivec2 lut_tex_size = textureSize(color_grading_lut_texture_sampler, 0);
    highp float height = float(lut_tex_size.y); // 16
	highp float width = float(lut_tex_size.x);
    highp vec4 color = subpassLoad(in_color);

	// map 0-1 to 0-15 to decide which sub image we will use
    highp float u = floor(color.b * 15.0);
    // map 0, 1...15 to 0, 16...240 then plus the pixel address in sub image 
	u = (u * 16.0) + floor(color.r * 15.0);
	u /= width;
	highp float v = floor(color.g * 15.0);
	v /= height;
    highp vec2 uv = vec2(u, v);
    highp vec4 color_l = texture(color_grading_lut_texture_sampler, uv);

    u = ceil(color.b * 15.0);
	u = (u * 16.0) + ceil(color.r * 15.0);
	u /= width;
	v = ceil(color.g * 15.0);
	v /= height;
    uv = vec2(uv);
    highp vec4 color_r = texture(color_grading_lut_texture_sampler, uv);

    highp float mix_param = color.b * 15.0 - floor(color.b * 15.0);
    out_color = mix(color_l, color_r, mix_param);
}
