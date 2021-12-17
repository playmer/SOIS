#include <array>

#include "glm/gtc/type_ptr.hpp"

//#include "fmt/format.h"

#include "Renderer.hpp"


namespace AllocatorTypes
{
	const std::string Mesh{ "Mesh" };
	const std::string Texture{ "Texture" };
	const std::string UniformBufferObject{ "UniformBufferObject" };
	const std::string BufferUpdates{ "BufferUpdates" };
}

Renderer::Renderer()
{

}

template <typename T>
GPUBuffer<T> CreateBuffer(GPUAllocator* aAllocator, size_t aSize)
{
	return aAllocator->CreateBuffer<T>(aSize,
		GPUAllocation::BufferUsage::TransferDst |
		GPUAllocation::BufferUsage::VertexBuffer,
		GPUAllocation::MemoryProperty::DeviceLocal);
}

