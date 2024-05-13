# Homework02-rendering

## Lut

![3dlut-to-2d](.images/2024-05-13-3dlut-to-2d.jpg)

A lut is a 3d image map one color to another, usually `16(width)*16(height)*16(sub image count)`, or `16*16*64(count)`, we should use trilinear interpolation to sample the lut texture.

Every sub image shares one same blue component, and in one sub image, the x axis means red, y axis means green.

TIPS: The top left sub image can't see any blue, because its blue components almost equals to 0, but the bottom right one looks very blue.
	
When we do sampling, color.b describes the z axis, representing which 16*16 sub image we will use. We should do twice sample on the two neighboring layers, which is floor(z) and ceil(z).

for example: if color.b * count = 15.5, then we sample on the 15th and 16th sub image, on the sub image we also do an interpolation for r and g.

NOTE: We should use nearest sampler not linear sampler when sampling this texture, because the color in this texture is pixelated, or say, discrete.
