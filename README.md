# DirectX 12 Memory Allocator (dx12-ma)

## Overview

The `dx12-ma` library provides a custom memory allocator tailored for DirectX 12 (DX12). This allocator efficiently manages memory for DX12 resources by allocating and freeing memory blocks from heaps. It supports dynamic heap management, automatic resource deallocation, and debug features to handle memory leaks in debug builds.

## Features

- **Dynamic Heap Management**: Efficiently allocates and manages memory blocks from DX12 heaps, with customizable heap sizes and counts.
- **Resource Wrapping**: Automatically manages the lifecycle of DirectX resources, including memory mapping and GPU virtual address retrieval.
- **Debugging Support**: Tracks and reports memory leaks in debug builds, ensuring that all allocations are properly managed.
- **Efficient Memory Operations**: Optimized for both allocation and deallocation, with support for block merging to minimize fragmentation.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Dependencies

- DirectX 12 SDK
- C++17 or later

## Getting Started

### Building

To build the `dx12-ma` library, include the source files in your project and link against the DirectX 12 SDK. Ensure that your build environment is configured to use C++17 or later.

### Configuration

Before including the `dx12_ma.h` header file, you can define configuration options to customize the allocator's behavior. The following options are available:

```cpp
#define DX12_MA_HEAP_BLOCK_SIZE 128 * UINT16_MAX // Custom block size, default: 64 * UINT16_MAX
#define DX12_MA_MAX_HEAP_COUNT 100 // Custom max heap count, default: 200

// Optionally enable debug mode, default: defined if in debug build
#define DX12_MA_DEBUG

#include "dx12_ma.h"
```

## Usage

### Memory Allocation

The `Allocator` class manages memory allocations from DX12 heaps:

```cpp
#include "dx12_ma.h" // Adjust the include path as necessary

ID3D12Device* device; // Assume this is initialized
dx12_ma::Allocator allocator(device);

// Allocate memory with optional alignment, see the API Reference
dx12_ma::Allocation allocation = allocator.Allocate(size, D3D12_HEAP_TYPE_DEFAULT, alignment);

// Free memory
allocator.Free(allocation);
```

### Resource Management

The `ResourceWrapper` class automatically manages DirectX resources, ensuring proper deallocation:

```cpp
#include "dx12_ma.h" // Adjust the include path as necessary

dx12_ma::Allocator allocator = /* allocator instance */;
dx12_ma::Allocation allocation = /* some allocation */;

ResourceType* resource = /* resource creation with `allocation` */;
// Ensure resource has a Release function, like ID3D12Resource

dx12_ma::ResourceWrapper<ResourceType> wrapper(allocation, &allocator);
wrapper.set_resource(resource);

// Access the resource
ResourceType* myResource = wrapper.get_resource();
```

When the `ResourceWrapper` goes out of scope, the allocation is automatically freed and the resource is released.

### Debugging

In debug builds (`DX12_MA_DEBUG` defined), the allocator reports memory leaks upon destruction by printing leaked allocations to `std::cerr`. To manually print all memory allocations, use:

```cpp
allocator.PrintLeakedMemory();
```

The `DX12_MA_DEBUG` definition must be active for this feature.

## Time Complexity

- **Allocation**: O(n), where `n` is the number of different heap types allocated. For example, with two CPU heaps (allocated first) and one GPU heap, the time complexity, for further allocations, would be O(2) for GPU allocations and O(1) for CPU allocations.
- **Deallocation**: O(n) in the worst case, where `n` is the number of free blocks.
- **Heap Management**: The management of heaps (`heaps_`) has a fixed upper limit (200 heaps), so operations involving heaps are effectively O(1).

## API Reference

### `Allocator`

- **Constructor**: `Allocator(ID3D12Device* device)`
  - Initializes the allocator with a DirectX 12 device.

- **Destructor**: Frees all allocated memory and prints memory leaks if `DX12_MA_DEBUG` is defined.

- **`Allocation Allocate(UINT64 size, D3D12_HEAP_TYPE heap_type, UINT64 alignment = 0)`**
  - Allocates a memory block of the specified size and heap type. The `alignment` parameter is optional and specifies the alignment of the allocation. If set to 0, no alignment is enforced.

- **`void Free(const Allocation& alloc)`**
  - Frees a previously allocated memory block and merges adjacent free blocks if possible.

- **`uint32_t get_free_blocks_size()`**
  - Returns the count of all free memory blocks.

- **`const ID3D12Heap* const* get_allocated_heaps() const`**
  - Returns the list of all allocated heaps.

- **`UINT32 get_allocated_heap_count() const`**
  - Returns the count of all allocated heaps.

### `ResourceWrapper<T>`

- **Constructor**: `ResourceWrapper(const Allocation& alloc, Allocator* mem_alloc)`
  - Initializes the resource wrapper with an allocation and a memory allocator.

- **Destructor**: Frees the allocation and releases the resource.

- **`void set_resource(T* resource)`**
  - Sets the resource and takes ownership.

- **`T* get_resource()`**
  - Returns the managed resource.

- **`T** get_resource_2r()`**
  - Returns a double reference to the managed resource.

- **`void MapMemory()` and `void UnmapMemory()`**
  - Maps and unmaps the resource memory, respectively.

- **`D3D12_GPU_VIRTUAL_ADDRESS get_gpu_address()`**
  - Returns the GPU virtual address of the resource.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on [GitHub](https://github.com/deneonet/dx12-ma).