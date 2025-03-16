/*
MIT License

Copyright (c) 2024 deneo (https://github.com/deneonet)

This file is part of https://github.com/deneonet/dxma
- A DirectX 12 Memory Allocator

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <d3d12.h>

#include <cassert>
#include <cstdint>
#include <unordered_set>

#ifndef DXMA_HEAP_BLOCK_SIZE
// Initial heap block size in bytes (default: 41.9424 MB)
#define DXMA_HEAP_BLOCK_SIZE 640 * UINT16_MAX
#endif

#ifndef DXMA_MAX_HEAP_COUNT
// Maximum number of heaps (default: 200)
#define DXMA_MAX_HEAP_COUNT 200
#endif

#ifdef _DEBUG
#define DXMA_DEBUG
#endif

namespace dxma_detail {

// Represents a memory allocation within a heap
struct Allocation {
 private:
  UINT64 size_ = 0;    // Size of the allocation
  UINT64 offset_ = 0;  // Offset within the heap
  D3D12_HEAP_TYPE heap_type_ = D3D12_HEAP_TYPE_DEFAULT;  // Type of heap
  UINT32 heap_index_ = 0;                                // Index of the heap
  ID3D12Heap* heap_ = nullptr;                           // Pointer to the heap
  ID3D12Resource* resource_ = nullptr;  // Pointer to the resource
  bool manage_resource_ =
      true;  // Whether the resource is managed by this allocation
  bool memory_mapped_ = false;  // Whether the resource is memory-mapped

 public:
  Allocation(UINT64 size, UINT64 offset, D3D12_HEAP_TYPE type,
             UINT32 heap_index, ID3D12Heap* heap)
      : size_(size),
        offset_(offset),
        heap_type_(type),
        heap_index_(heap_index),
        heap_(heap) {}

  ~Allocation() {
    if (manage_resource_ && resource_ != nullptr) {
      if (memory_mapped_) resource_->Unmap(0, nullptr);
      resource_->Release();
    }
  }

  // Getters
  UINT64 GetSize() const { return size_; }
  UINT64 GetOffset() const { return offset_; }
  D3D12_HEAP_TYPE GetHeapType() const { return heap_type_; }
  UINT32 GetHeapIndex() const { return heap_index_; }
  ID3D12Heap* GetHeap() const { return heap_; }
  ID3D12Resource* GetResource() const { return resource_; }
  bool IsMemoryMapped() const { return memory_mapped_; }

  // Setters
  void SetResource(ID3D12Resource* resource, bool manage_resource = true) {
    resource_ = resource;
    manage_resource_ = manage_resource;
  }

  void SetMemoryMapped(bool mapped) { memory_mapped_ = mapped; }

  bool operator==(const Allocation& other) const {
    return size_ == other.size_ && offset_ == other.offset_ &&
           heap_index_ == other.heap_index_;
  }
};

// Represents a free block of memory within a heap
struct FreeBlock {
 private:
  UINT64 size_ = 0;    // Size of the free block
  UINT64 offset_ = 0;  // Offset within the heap
  D3D12_HEAP_TYPE heap_type_ = D3D12_HEAP_TYPE_DEFAULT;  // Type of heap
  UINT32 heap_index_ = 0;                                // Index of the heap
  FreeBlock* next_ = nullptr;   // Pointer to the next free block
  ID3D12Heap* heap_ = nullptr;  // Pointer to the heap

 public:
  FreeBlock(UINT64 size, UINT64 offset, D3D12_HEAP_TYPE type, UINT32 heap_index,
            FreeBlock* next, ID3D12Heap* heap)
      : size_(size),
        offset_(offset),
        heap_type_(type),
        heap_index_(heap_index),
        next_(next),
        heap_(heap) {}

  // Getters
  UINT64 GetSize() const { return size_; }
  UINT64 GetOffset() const { return offset_; }
  D3D12_HEAP_TYPE GetHeapType() const { return heap_type_; }
  UINT32 GetHeapIndex() const { return heap_index_; }
  FreeBlock* GetNext() const { return next_; }
  ID3D12Heap* GetHeap() const { return heap_; }

  // Setters
  void SetSize(UINT64 size) { size_ = size; }
  void SetOffset(UINT64 offset) { offset_ = offset; }
  void SetNext(FreeBlock* next) { next_ = next; }
};

#ifdef DXMA_DEBUG
// Hash function for allocations (used in debug mode)
struct AllocationHasher {
  size_t operator()(Allocation* a) const {
    size_t ho = std::hash<UINT64>{}(a->GetOffset());
    size_t hi = std::hash<UINT32>{}(a->GetHeapIndex());
    return ho ^ (hi << 1);
  }
};
#endif

// Main allocator class for managing memory allocations
class Allocator {
 private:
  FreeBlock* head_ = nullptr;       // Head of the free block list
  ID3D12Device* device_ = nullptr;  // Pointer to the DirectX 12 device
  UINT32 heap_count_ = 0;           // Number of allocated heaps
  ID3D12Heap* heaps_[DXMA_MAX_HEAP_COUNT]{};  // Array of heaps

#ifdef DXMA_DEBUG
  std::unordered_set<Allocation*, AllocationHasher>
      allocations_;  // Track allocations in debug mode
#endif

 public:
  Allocator() = default;
  explicit Allocator(ID3D12Device* device) : device_(device) {}

  ~Allocator() {
    // Release all allocated heaps
    for (UINT32 i = 0; i < heap_count_; i++) {
      heaps_[i]->Release();
    }
    heap_count_ = 0;

    PrintLeakedMemory();

    // Free all free blocks
    FreeBlock* ptr = head_;
    while (ptr) {
      FreeBlock* next = ptr->GetNext();
      delete ptr;
      ptr = next;
    }
  }

  // Print memory leaks in debug mode
  void PrintLeakedMemory() {
#ifdef DXMA_DEBUG
    for (Allocation* alloc : allocations_) {
      std::cerr << "[DXMA] Memory Leaked: " << alloc->GetSize()
                << " bytes at offset " << alloc->GetOffset()
                << " with heap type/index: " << alloc->GetHeapType() << "/"
                << alloc->GetHeapIndex() << "\n";
    }
#endif
  }

  // Get the number of free blocks
  uint32_t GetFreeBlockCount() const {
    uint32_t count = 0;
    FreeBlock* ptr = head_;
    while (ptr) {
      ptr = ptr->GetNext();
      count++;
    }
    return count;
  }

  // Get the head of the free block list
  FreeBlock* GetHead() const { return head_; }

  // Set the head of the free block list
  void SetHead(FreeBlock* new_head) { head_ = new_head; }

  // Get the DirectX 12 device
  ID3D12Device* GetDevice() const { return device_; }

  // Get the array of allocated heaps
  ID3D12Heap** GetHeaps() { return heaps_; }

  // Get the number of allocated heaps
  UINT32 GetHeapCount() const { return heap_count_; }

  // Increment the heap count
  void IncrementHeapCount() { heap_count_++; }

  // Add an allocation to the tracking set (debug mode only)
  void AddAllocation(Allocation* allocation) {
#ifdef DXMA_DEBUG
    allocations_.insert(allocation);
#endif
  }

  // Remove an allocation from the tracking set (debug mode only)
  uint32_t RemoveAllocation(Allocation* allocation) {
#ifdef DXMA_DEBUG
    return allocations_.erase(allocation);
#else
    return 0;
#endif
  }
};

// Define handle types for the library user
#define DEFINE_DXMA_HANDLE(name) typedef dxma_detail::name* Dxma##name;

}  // namespace dxma_detail

// Define handles for Allocation, FreeBlock, and Allocator
DEFINE_DXMA_HANDLE(Allocation)
DEFINE_DXMA_HANDLE(FreeBlock)
DEFINE_DXMA_HANDLE(Allocator)

// Create a new allocator instance
void dxmaCreateAllocator(DxmaAllocator* allocator, ID3D12Device* device) {
  *allocator = new dxma_detail::Allocator(device);
}

// Destroy an allocator instance
void dxmaDestroyAllocator(DxmaAllocator allocator) { delete allocator; }

// Create a resource in the specified allocation
HRESULT dxmaCreateResource(DxmaAllocator allocator, DxmaAllocation allocation,
                           const D3D12_RESOURCE_DESC* resource_desc,
                           D3D12_RESOURCE_STATES initial_state,
                           bool auto_manage_resource = true) {
  ID3D12Resource* resource;
  HRESULT result = allocator->GetDevice()->CreatePlacedResource(
      allocation->GetHeap(), allocation->GetOffset(), resource_desc,
      initial_state, nullptr, IID_PPV_ARGS(&resource));
  allocation->SetResource(resource, auto_manage_resource);
  return result;
}

// Destroy a resource associated with an allocation
void dxmaDestroyResource(DxmaAllocation allocation, ID3D12Resource* resource) {
  if (resource && !allocation->GetResource()) {
    resource->Release();
  }
  allocation->SetResource(nullptr, false);
}

// Information required for memory allocation
struct DxmaAllocationInfo {
  UINT64 size = 0;                                 // Size of the allocation
  D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT;  // Type of heap
  UINT64 alignment = 0;                            // Alignment requirement
};

// Map memory for CPU access
HRESULT dxmaMapMemory(DxmaAllocation allocation, void** data) {
  if (!allocation->IsMemoryMapped() && allocation->GetResource() != nullptr) {
    D3D12_RANGE read_range{0, 0};
    HRESULT result = allocation->GetResource()->Map(0, &read_range, data);

    if (SUCCEEDED(result)) {
      allocation->SetMemoryMapped(true);
    }
    return result;
  }
  return S_OK;
}

// Unmap memory
void dxmaUnmapMemory(DxmaAllocation allocation) {
  if (allocation->IsMemoryMapped() && allocation->GetResource() != nullptr) {
    allocation->GetResource()->Unmap(0, nullptr);
    allocation->SetMemoryMapped(false);
  }
}

// Allocate memory from the allocator
void dxmaAllocate(DxmaAllocator allocator, const DxmaAllocationInfo& alloc_info,
                  DxmaAllocation* allocation) {
  UINT64 size = alloc_info.size;
  D3D12_HEAP_TYPE type = alloc_info.type;
  UINT64 alignment = alloc_info.alignment;

  if (size == 0) return;
  size = alignment == 0 ? size : (size + alignment - 1) & ~(alignment - 1);

  DxmaFreeBlock ptr = allocator->GetHead();
  DxmaFreeBlock prev = nullptr;

  while (ptr) {
    UINT64 ptr_size = ptr->GetSize();
    if (ptr->GetHeapType() != type) ptr_size = 0;

    if (ptr_size >= size) {
      UINT64 ptr_offset = ptr->GetOffset();
      ID3D12Heap* ptr_heap = ptr->GetHeap();
      UINT32 ptr_heap_index = ptr->GetHeapIndex();

      if (ptr_size == size) {
        // Exact match: remove the free block
        if (prev) {
          prev->SetNext(ptr->GetNext());
        } else {
          allocator->SetHead(ptr->GetNext());
        }
        delete ptr;

        *allocation = new dxma_detail::Allocation(size, ptr_offset, type,
                                                  ptr_heap_index, ptr_heap);
#ifdef DXMA_DEBUG
        allocator->AddAllocation(*allocation);
#endif
        return;
      }

      // Split the free block
      ptr->SetSize(ptr_size - size);
      ptr->SetOffset(ptr_offset + size);

      *allocation = new dxma_detail::Allocation(size, ptr_offset, type,
                                                ptr_heap_index, ptr_heap);
#ifdef DXMA_DEBUG
      allocator->AddAllocation(*allocation);
#endif
      return;
    }

    prev = ptr;
    ptr = ptr->GetNext();
  }

  // Out of memory: allocate a new heap
  UINT64 heap_block_size = DXMA_HEAP_BLOCK_SIZE;
  if (heap_block_size - size <= 0) heap_block_size = size * 4;

  ID3D12Device* device = allocator->GetDevice();
  assert(device);

  D3D12_HEAP_DESC heap_desc{};
  heap_desc.SizeInBytes = heap_block_size;
  heap_desc.Flags = D3D12_HEAP_FLAG_NONE;
  heap_desc.Properties.Type = type;
  heap_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

  ID3D12Heap* new_heap = nullptr;
  HRESULT hr = device->CreateHeap(&heap_desc, IID_PPV_ARGS(&new_heap));
  assert(SUCCEEDED(hr));

  if (FAILED(hr)) return;

  UINT32 heap_count = allocator->GetHeapCount();
  ID3D12Heap** heaps = allocator->GetHeaps();
  heaps[heap_count] = new_heap;

  // Create a new free block
  DxmaFreeBlock new_block =
      new dxma_detail::FreeBlock(heap_block_size - size, size, type, heap_count,
                                 allocator->GetHead(), new_heap);

  allocator->SetHead(new_block);
  allocator->IncrementHeapCount();

  *allocation =
      new dxma_detail::Allocation(size, 0, type, heap_count, new_heap);
#ifdef DXMA_DEBUG
  allocator->AddAllocation(*allocation);
#endif
}

// Free a memory allocation
void dxmaFree(DxmaAllocator allocator, DxmaAllocation allocation,
              ID3D12Resource* resource = nullptr) {
  if (allocation->GetSize() == 0 || allocation->GetHeap() == nullptr) {
    assert(!"Invalid allocation passed to dxmaFree: size is 0 or heap is null");
    return;
  }

#ifdef DXMA_DEBUG
  if (allocator->RemoveAllocation(allocation) != 1) {
    assert(
        !"Invalid allocation passed to dxmaFree: allocation was not tracked");
    return;
  }
#endif

  DxmaFreeBlock new_block = new dxma_detail::FreeBlock(
      allocation->GetSize(), allocation->GetOffset(), allocation->GetHeapType(),
      allocation->GetHeapIndex(), nullptr, allocation->GetHeap());

  DxmaFreeBlock head = allocator->GetHead();
  if (!head) {
    allocator->SetHead(new_block);
    return;
  }

  DxmaFreeBlock prev = nullptr;
  DxmaFreeBlock current = head;

  // Find the correct position to insert the new block
  while (current && (current->GetHeapIndex() != allocation->GetHeapIndex() ||
                     current->GetOffset() < allocation->GetOffset())) {
    prev = current;
    current = current->GetNext();
  }

  // Release the resource if it's not managed by the allocation
  if (resource && !allocation->GetResource()) {
    resource->Release();
  }

  delete allocation;
  allocation = nullptr;

  // Merge with the previous block if possible
  if (prev && prev->GetOffset() + prev->GetSize() == new_block->GetOffset() &&
      prev->GetHeapIndex() == new_block->GetHeapIndex()) {
    prev->SetSize(prev->GetSize() + new_block->GetSize());
    delete new_block;
    new_block = prev;
  } else {
    // Insert the new block
    if (prev) {
      prev->SetNext(new_block);
    } else {
      allocator->SetHead(new_block);
    }
    new_block->SetNext(current);
  }

  // Merge with the next block if possible
  if (current &&
      new_block->GetOffset() + new_block->GetSize() == current->GetOffset() &&
      current->GetHeapIndex() == new_block->GetHeapIndex()) {
    new_block->SetSize(new_block->GetSize() + current->GetSize());
    new_block->SetNext(current->GetNext());
    delete current;
  }
}