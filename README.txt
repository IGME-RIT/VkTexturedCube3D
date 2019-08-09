Textures are used in every modern game, and
in this tutorial, we show exactly how to use them.

Remember how in the last tutorial, there could only
be too values per color (red and green, but not blue)?
Well guess what, those weren't really color values, they
were UV coordinates. UV coordinates are (X, Y) coordinates
on a texture, that tell the graphics card what pixels of
the texture should be drawn on geometry. So the first thing
I did in this tutorial, was go to the Vertex Sader, and 
rename "inColor" and "outColor" to "inUV and outUV".
In the fragment shader we have inUV to get the UV from the
Vertex Shader, and we have outColor to send a color to the 
screen. To get a pixel from the texture, the command is 
simply 
	outColor = texture(samplerColor, uv, 0);
Also, at the top of the fragment shader, we take in a 
texture as a uniform, just like the uniform buffer in
the vertex shader:
	layout (binding = 1) uniform sampler2D samplerColor;
To get this to the shader, we have to adjust descriptors

In Demo.h we add a BufferCPU for the texture that we load
from a PNG file, and we add a TextureGPU to move this
buffer to the GPU. Loading textures is like a combination
of how we did vertex buffers, and how we did the depth buffer.
We will load the texture from PNG, put it in a CPU buffer,
then copy it into a TextureGPU buffer so the graphics card
can use the texture.

In Demo.h and Demo.cpp, we have prepare_sampler(), which
tells the GPU how to get pixels from the texture, which is
called "sampling the image", and we have prepare_textures(),
to load textures into the program (just one texture for now).
We have VkSampler in Demo.h to hold the sampler that we make.

Next, we have to change our prepare_descriptor_pool to have
room in the program for textures, then we need to change
the descriptor set to allow for textures, and we need to change
the descriptor layout to allow for textures. All of these
can be done without a lot of code, even though it sounds like
a lot, we really add 3 or 4 lines to each function.

Remember, in prepare(), after the initCmd is executed, we need
to delete the texture's CPU buffer, and when the program reaches
the Demo destructor, we need to delete the texture's GPU buffer,
as well as the sampler