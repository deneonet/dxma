# DirectX 12 Memory Allocator (dx12-ma)

## Overview

The `dx12-ma` library provides a custom memory allocator for DirectX 12 (DX12). This allocator is designed to efficiently manage memory for DirectX 12 resources by allocating and freeing memory blocks from heaps. It supports dynamic heap management and handles memory leaks in debug builds.

## Features

- **Dynamic Heap Management**: Allocates and manages memory blocks from DX12 heaps.
- **Debugging Support**: Optionally tracks and reports memory leaks.
- **Efficient Allocation and Deallocation**: Supports both allocation of new heaps and freeing of previously allocated memory blocks.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Dependencies

- DirectX 12 SDK
- C++17 or later

## Getting Started

### Building

To build the `dx12-ma` library, include the source files in your project and link against the DirectX 12 SDK. Ensure that your build environment is set up to use C++17 or later.

### Usage

#### Memory Allocator

The `MemAllocator` class is responsible for managing memory allocations:

```cpp
#include "dx12_ma.h" // Adjust the include path as necessary

ID3D12Device* device; // Assume this is initialized
dx12_ma::Allocator allocator(device);

// Allocate memory
dx12_ma::Allocation allocation = allocator.Allocate(size, D3D12_HEAP_TYPE_DEFAULT);

// Free memory
allocator.Free(allocation);
```

#### Resource Wrapper

The `ResourceWrapper` class is used to manage DirectX resources with automatic deallocation:

```cpp
#include "dx12_ma.h" // Adjust the include path as necessary

// Stack or Heap allocated
dx12_ma::Allocator allocator = /* allocator instance */;
dx12_ma::Allocation allocation = /* some allocation */;

// Assume ResourceType is a type with a Release() method, like ID3D12Resource
ResourceType* resource = /* resource creation with `allocation` */;

dx12_ma::ResourceWrapper<ResourceType> wrapper(allocation, &allocator);
wrapper.set_resource(resource);

// Access the resource
ResourceType* myResource = wrapper.get_resource();

// After wrapper goes out of scope, the allocation gets freed and the resource is released using the Release function
```

### Debugging

In debug builds (`DX12_MA_DEBUG` defined), the allocator reports memory leaks upon destruction, this is done by printing leaked allocations to `std::cerr`. To manually print all memory allocations, use:

```cpp
allocator.PrintLeakedMemory();
```

## Time Complexity

- **Allocation**: O(n), where `n` is the number of different heap types allocated. For example, with two CPU heaps and one GPU heap, the time complexity would be O(2) for GPU allocations and O(1) for CPU allocations.
- **Deallocation**: O(n) in the worst case, where `n` is the number of free blocks.
- **Heap Management**: The management of heaps (`heaps_`) has a fixed upper limit (200 heaps), so operations involving heaps are effectively O(1).

## API Reference

### `Allocator`

- **Constructor**: `Allocator(ID3D12Device* device)`
  - Initializes the allocator with a DirectX 12 device.

- **Destructor**: Frees all allocated memory and, if `DX12_MA_DEBUG` defined, prints any memory leaks.

- **`Allocation Allocate(UINT64 size, D3D12_HEAP_TYPE heap_type)`**
  - Allocates a memory block of the specified size and heap type.

- **`void Free(const Allocation& alloc)`**
  - Frees a previously allocated memory block.

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

## Contributing

Contributions are welcome! Please open an issue or submit a pull request on [GitHub](https://github.com/deneonet/dx12-ma).