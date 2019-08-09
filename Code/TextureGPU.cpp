/*
Copyright 2019
Original authors: Niko Procopi
Written under the supervision of David I. Schwartz, Ph.D., and
supported by a professional development seed grant from the B. Thomas
Golisano College of Computing & Information Sciences
(https://www.rit.edu/gccis) at the Rochester Institute of Technology.
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.
<http://www.gnu.org/licenses/>.

Special Thanks to Exzap from Team Cemu,
he gave me advice on how to optimize Vulkan
graphics, he is working on a Wii U emulator
that utilizes Vulkan, see more at http://cemu.info
*/

#include "TextureGPU.h"
#include "Helper.h"

// When we create a GPU buffer, we need the Device (lets us give commands to GPU),
// we need the Memory Properties from the PhysicalDevice (same GPU, but the properties of teh GPU),
// and we need the BufferCreateInfo, to tell us what type of buffer this is (uniform, vertex, index, etc)

// Texture buffers are a little different from other GPU buffers. They require
// extra information to let our GPU know what type of texture we are dealing with,
// what format the texture is in, as well as extra information for mipmap textures.
// If we attempt to use the BufferGPU class, then the GPU will have our raw
// pixel data that was in the texture file, but the GPU won't know what to do with it.
// By using this special class, it allows the GPU to get information about our texture.

// In addition to Device and Memory Properties,
// we need VkImageCreateInfo, and VkImageAspectFlags
TextureGPU::TextureGPU(
	VkDevice d,
	VkPhysicalDeviceMemoryProperties memory_properties,
	VkImageCreateInfo image_create_info,
	VkImageAspectFlags aspect)
{
	// save device, so that
	// we can use it to store
	// data, and delete data
	device = d;

	// create image with the device, by using VkImageCreateInfo.
	// This sepecifically makes a VkImage, rather than an ordinary
	// VkBuffer, which allows the GPU to have image properteis
	vkCreateImage(device, &image_create_info, NULL, &image);

	// get memory requirements, so that we know
	// what we need in order to allocate the memory
	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(device, image, &mem_reqs);

	// create allocation information
	// this uses the memory requirements
	// to tell the device how to allocate the buffer
	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = mem_reqs.size;

	// This is exactly the same as BufferGPU,
	// we are looking for memory on the Device (VkDevice),
	// which allows us to store the data in the VRAM
	Helper::memory_type_from_properties(
		memory_properties,
		mem_reqs.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&memAllocInfo.memoryTypeIndex);

	// allocate memory
	// our memoryAllocationInfo got the required size
	// from the VkMemoryRequirements, and it got
	// the type of memory from memory_type_from_properties
	vkAllocateMemory(device, &memAllocInfo, NULL, &memory);

	// After our memory is allocated, we have
	// to bind the buffer to the memory, so that
	// we can use the memory. Binding image memory
	// requires a special vkBindImageMemory function
	vkBindImageMemory(device, image, memory, 0);

	// When we created our Swapchain, it was mentioned
	// that we had our images in the form of VkImage, but
	// those images were not easily accessible without 
	// VkImageView. To make this image easy to work with
	// we will put our VkImage into a VkImageView, so that
	// we can store sepcific information about the image,
	
	// information such as:
		// type: 2D, 3D, skybox, etc
		// format: RGB, Greyscale, etc
		// aspect: color or depth, etc

	// This TextureGPU class can be used for textures,
	// as well as depth buffers for 3D scenes
	// We create this imageView almost exactly the same
	// way as we did when we made the ImageViews for
	// the Swapchain Images
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = image_create_info.format;

	// we hvae to label each of the color components
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;

	// we get the aspect of this image from the parameter,
	// it can be an image used for color, an image used
	// for depth, or several other types
	viewInfo.subresourceRange.aspectMask = aspect;

	// If you don't know what mipmaps are, or mipmap chains, 
	// don't worry about it. This image (the swapchain image),
	// does not have a mipmap chain, so the base level is 0,
	// and there is 1 level. Honestly just don't worry about it.
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;

	// Technically, in Vulkan's eyes, every image is an array
	// so we tell it that there is one image in the array,
	// and that the element of the array we want is 0,
	// which literally means "there is one image, so use
	// that one image"
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	// Put the VKImage inside the VkImageViewInfo
	viewInfo.image = image;

	// Create the VkImageView given our VkImageViewInfo
	vkCreateImageView(device, &viewInfo, NULL, &imageView);

	// save the VkImageViewCreaetInfo, it will come in handy
	// later when we are copying data from CPU to GPU
	viewCreateInfo = viewInfo;
}

TextureGPU::~TextureGPU()
{
	// First we destroy the memory,
	// then we destroy the image that used the memory
	// then we destroy the imageView that used the image
	// the order doesn't really matter though
	vkFreeMemory(device, memory, NULL);
	vkDestroyImage(device, image, NULL);
	vkDestroyImageView(device, imageView, NULL);
}

// Keep in mind, this function is used to store data
// by copying it from CPU to GPU, we will only use it 
// for 2D Textures that come from files. We will not use
// it for the depth buffer, because depth buffers are not
// copied from CPU to GPU, they are created on GPU, 
// written to by the GPU, and read by the GPU.
void TextureGPU::Store(VkCommandBuffer cmd, VkBuffer cpuBuffer, int width, int height)
{
	// If anyone thinks it will be easy to store GPU textures in VRAM
	// as easy as it was to store other GPU buffers into VRAM,
	// that person is in for a big surprise.

	// In the BufferGPU class, we had a VkBufferCopy structure
	// that held the size of the buffer, and we had a command called
	// vkCmdCopyBuffer to submit a command into the command buffer
	
	// In TextureGPU, we have a similar structure with
	// VkBufferImageCopy and vkCmdCopyBufferToImage.
	// However, simply copying an image from CPU to GPU is not
	// enough for Vulkan to consider our code to be valid, and 
	// this will result in many validation errors.
	// In addition to copying our data, we also need to make
	// sure that this data is only readible by the shaders,
	// and we need to make sure that this data is READ-ONLY

	// By setting our data to READ-ONLY, it stores our
	// buffer in a location in memory that allows the 
	// GPU to read the memory faster, than if it were
	// in an area of memory that allows for reading and
	// writing at any point in time.

	// Bonous Challenge:
	// dIf anyone wants to, they can transfer some of 
	// these optimizations to the BufferGPU class to let
	// the Graphics card know that the GPU buffers for 
	// Vertex and Index buffers are READ-ONLY, feel free
	// to try it out

	// Create an image memory barrier
	// This allows us to edit properties of GPU memory,
	// such as specifically where in GPU memory it is that 
	// our memory exists, and which pipeline stages can 
	// access the memory
	VkImageMemoryBarrier image_memory_barrier = {};

	// currently, our GPU's image memory has no AccessMask (0)
	// and it has no layout (PREINITIALIZED)

	// Let's change the AccessMask to WRITE_BIT, so we can write
	// and Let's change the layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	// so that it can recieve

	// The memory we are using is this texture
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.srcAccessMask = (VkAccessFlagBits)0;
	image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	// copy data from the imageViewCreateInfo that we made
	// in the TextureGPU constructor, into the memory barrier
	image_memory_barrier.image = viewCreateInfo.image;
	image_memory_barrier.subresourceRange = viewCreateInfo.subresourceRange;

	// submit the request to the GPU
	// TOP_OF_PIPE is the stage that the GPU's memory is currently at, which is where all memory is initially
	// VK_PIPELINE_STAGE_TRANSFER_BIT is the stage that we want the memory to be in, so we can transfer data to 
	// this buffer We move the memory (defined in image_memory_barrier) from top of pipe to the transfer stage
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, NULL, 0, NULL,
		1, &image_memory_barrier);

	// Create information that lets us copy the buffer,
	// by knowing the width, the height, which is needed
	// twice for some reason
	VkBufferImageCopy copy_region = {};
	copy_region.bufferRowLength = width;
	copy_region.bufferImageHeight = height;
	copy_region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

	// this subResource is not exactly the same as
	// the imageCreateInfo subresource, so we put in
	// values manually
	copy_region.imageSubresource = 
		{ viewCreateInfo.subresourceRange.aspectMask, 0, 0, 1 };

	// Copy the image data from the CPU buffer to the GPU buffer
	vkCmdCopyBufferToImage(cmd, cpuBuffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

	// We are done writing to the GPU for this buffer
	// so let's change AccessMask from WRITE_BIT, to nothing (0)
	// then let's change layout from DST_OPTIMAL to READ_ONLY_OPTIMAL
	image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	image_memory_barrier.dstAccessMask = (VkAccessFlagBits)0;
	image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// submit the request to the GPU
	// VK_PIPELINE_STAGE_TRANSFER_BIT is the stage that the GPU's memory is currently at
	// VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT is where memory can be accessed by the fragment shader
	// We move the memory from transfer stage to the fragment shader
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, NULL, 0, NULL,
		1, &image_memory_barrier);
}