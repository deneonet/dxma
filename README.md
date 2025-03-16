# DirectX 12 Memory Allocator (DXMA)

## Overview

The **DirectX 12 Memory Allocator (DXMA)** is a lightweight and efficient memory management library designed for DirectX 12 applications. It simplifies memory allocation and deallocation for DirectX resources, providing features like dynamic heap management, automatic resource cleanup, and debugging support to detect memory leaks.

## Features

- **Dynamic Heap Management**: Efficiently allocates and manages memory blocks from DirectX 12 heaps, with customizable heap sizes and counts.
- **Automatic Resource Management**: Automatically handles the lifecycle of DirectX resources, including memory mapping.
- **Debugging Support**: Tracks and reports memory leaks in debug builds, ensuring proper memory management.
- **Optimized Memory Operations**: Minimizes fragmentation by merging adjacent free blocks during deallocation.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Dependencies

- **DirectX 12 SDK**: Required for DirectX 12 functionality.
- **C++17 or later**: The library uses modern C++ features.

## Getting Started

### Building

To use the DXMA library, include the `dxma.h` header file in your project and link against the DirectX 12 SDK. Ensure your build environment supports C++17 or later.

### Configuration

You can customize the allocator's behavior by defining the following options before including the `dxma.h` header:

```cpp
#define DXMA_HEAP_BLOCK_SIZE 128 * UINT16_MAX // Custom heap block size (default: 640 * UINT16_MAX)
#define DXMA_MAX_HEAP_COUNT 100 // Custom maximum heap count (default: 200)

// Enable debug mode (automatically enabled in debug builds)
#define DXMA_DEBUG

#include "dxma.h"
```

## Usage

### Memory Allocation

The `DxmaAllocator` class manages memory allocations from DirectX 12 heaps:

```cpp
#include "dxma.h"

ID3D12Device* device; // Assume this is initialized
DxmaAllocator allocator;
dxmaCreateAllocator(&allocator, device);

// Allocate memory
DxmaAllocationInfo allocationInfo{};
allocationInfo.size = 1024; // 1 KB
allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD; // CPU-accessible heap

DxmaAllocation allocation = nullptr;
dxmaAllocate(allocator, allocationInfo, &allocation);

// Free memory
dxmaFree(allocator, allocation, nullptr /* optional, default: nullptr */);
```

### Resource Management

The library supports automatic resource management. When an allocation is freed, any associated DirectX resource is automatically released:

```cpp
D3D12_RESOURCE_DESC resourceDesc{};
resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
resourceDesc.Width = allocationInfo.size;
resourceDesc.Height = 1;
resourceDesc.DepthOrArraySize = 1;
resourceDesc.MipLevels = 1;
resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
resourceDesc.SampleDesc.Count = 1;
resourceDesc.SampleDesc.Quality = 0;
resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

// Create a DirectX 12 resource
HRESULT hr = dxmaCreateResource(allocator, allocation, &resourceDesc, D3D12_RESOURCE_STATE_COMMON, /* false : Disables the automatic release of resources, when freeing the allocation */);
ASSERT_TRUE(SUCCEEDED(hr));

// Access the resource
ID3D12Resource* resource = allocation->GetResource();

// Free the allocation (resource will be auto-managed)
dxmaFree(allocator, allocation /* resource : If the automatic release of resources is disabled, then pass the resource as last parameter to `dxmaFree` or call `resource->Release();` yourself */);
```

### Debugging

In debug builds (`DXMA_DEBUG` defined), the allocator tracks memory allocations and reports leaks upon destruction. To manually print leaked memory allocations, use:

```cpp
allocator->PrintLeakedMemory();
```

This feature is only available when `DXMA_DEBUG` is defined.

## Time Complexity

- **Allocation**: O(n), where `n` is the number of free blocks in the heap.
- **Deallocation**: O(n), where `n` is the number of free blocks (due to block merging).
- **Heap Management**: O(1) for heap operations, as the maximum number of heaps is fixed.

## API Reference

### `DxmaAllocator`

- **Initialization**: `dxmaCreateAllocator(DxmaAllocator* allocator, ID3D12Device* device)`

  - Initializes the allocator with a DirectX 12 device.

- **Destruction**: `dxmaDestroyAllocator(DxmaAllocator allocator)`

  - Frees all allocated memory and prints memory leaks if `DXMA_DEBUG` is defined.

- **Memory Allocation**: `dxmaAllocate(DxmaAllocator allocator, const DxmaAllocationInfo& info, DxmaAllocation* allocation)`

  - Allocates a memory block of the specified size and heap type.

- **Memory Deallocation**: `dxmaFree(DxmaAllocator allocator, DxmaAllocation allocation, ID3D12Resource* resource)`

  - Frees a previously allocated memory block and merges adjacent free blocks if possible.

- **Resource Creation**: `dxmaCreateResource(DxmaAllocator allocator, DxmaAllocation allocation, const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES initialState, bool autoManageResource = true)`

  - Creates a DirectX 12 resource using the specified allocation.

- **Memory Mapping**: `dxmaMapMemory(DxmaAllocation allocation, void** data)`

  - Maps the resource memory for CPU access.

- **Memory Unmapping**: `dxmaUnmapMemory(DxmaAllocation allocation)`
  - Unmaps the resource memory.

### `DxmaAllocationInfo`

- **Structure**:
  ```cpp
  struct DxmaAllocationInfo {
      UINT64 size = 0;                 // Size of the allocation
      D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT; // Heap type
      UINT64 alignment = 0;            // Alignment requirement
  };
  ```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on [GitHub](https://github.com/deneonet/dxma).
