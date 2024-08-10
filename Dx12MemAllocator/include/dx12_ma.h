/*
MIT License

Copyright (c) 2024 deneo (https://github.com/deneonet)

This file is part of https://github.com/deneonet/dx12-ma.
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

#include <cstdint>
#include <unordered_set>

namespace dx12_ma {

#ifndef DX12_MA_HEAP_BLOCK_SIZE
// Initial heap block size in bytes
// The current size has no specific meaning, it's just a starting size
#define DX12_MA_HEAP_BLOCK_SIZE 64 * UINT16_MAX
// 64 * UINT16_MAX = 4194240 bytes = 4,19424 MB
#endif

#ifndef DX12_MA_MAX_HEAP_COUNT
// Initial max heap count
// Max of 200 heaps should be more than enough
#define DX12_MA_MAX_HEAP_COUNT 200
#endif

#ifdef _DEBUG
#define DX12_MA_DEBUG
#endif

// Represents a memory allocation with size, offset, heap type, and heap pointer
struct Allocation {
  UINT64 size = 0;
  UINT64 offset = 0;

  D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
  UINT32 heap_index = 0;

  ID3D12Heap* heap = nullptr;

  bool operator==(const Allocation& other) const {
    return size == other.size && offset == other.offset &&
           heap_index ==
               other.heap_index;  // Also checking the heap and heap type
  }
};

#ifdef DX12_MA_DEBUG
struct AllocationHasher {
  size_t operator()(const Allocation& a) const {
    size_t ho = std::hash<UINT64>{}(a.offset);
    size_t hi = std::hash<UINT32>{}(a.heap_index);
    return ho ^ (hi << 1);
  }
};
#endif

// Represents a free block of memory in the heap
struct FreeBlock {
  UINT64 size = 0;
  UINT64 offset = 0;

  D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
  UINT32 heap_index = 0;

  FreeBlock* next = nullptr;
  ID3D12Heap* heap = nullptr;

  FreeBlock(const FreeBlock&) = delete;
  FreeBlock& operator=(const FreeBlock&) = delete;

  FreeBlock(FreeBlock&& other) noexcept = delete;
  FreeBlock& operator=(FreeBlock&& other) noexcept = delete;

  FreeBlock(UINT64 size, UINT64 offset, D3D12_HEAP_TYPE heap_type,
            UINT32 heap_index, FreeBlock* next, ID3D12Heap* heap)
      : size(size),
        offset(offset),
        heap_type(heap_type),
        heap_index(heap_index),
        next(next),
        heap(heap) {}
};

class Allocator {
 public:
  Allocator() = default;
  // Allocator does not take ownership of `device` (`device` is not going
  // to be released and has to be valid for the lifetime of
  // Allocator)
  Allocator(ID3D12Device* device) : device_(device) {}

  ~Allocator() {
    // Free all allocated memory

    for (UINT32 i = 0; i < heap_count_; i++) heaps_[i]->Release();
    heap_count_ = 0;

    PrintLeakedMemory();

    FreeBlock* ptr = head_;

    while (ptr) {
      FreeBlock* next = ptr->next;
      delete ptr;
      ptr = next;
    }
  }

  // Prints all memory allocations that haven't been freed so far
  inline void PrintLeakedMemory() {
#ifdef DX12_MA_DEBUG
    for (const Allocation& alloc : allocations_) {
      std::cerr << "[dx12-ma] Memory Leaked: " << alloc.size
                << " bytes at offset " << alloc.offset
                << " with dx12 heap type/index: " << alloc.heap_type << "/"
                << alloc.heap_index << "\n";
    }
#endif
  }

  // Returns the count of all free blocks
  inline uint32_t get_free_blocks_size() {
    uint32_t size = 0;
    FreeBlock* ptr = head_;
    while (ptr) {
      ptr = ptr->next;
      size++;
    }
    return size;
  }

  // Returns all allocated heaps
  inline const ID3D12Heap* const* get_allocated_heaps() const { return heaps_; }

  // Returns the count of all allocated heaps
  inline UINT32 get_allocated_heap_count() const { return heap_count_; }

  // Allocates a memory block from the specified heap using `heap_type`
  Allocation Allocate(UINT64 size, D3D12_HEAP_TYPE heap_type,
                      UINT64 alignment = 0) {
    if (size == 0) return {0};
    size = alignment == 0 ? size : (size + alignment - 1) & ~(alignment - 1);

    FreeBlock* ptr = head_;
    FreeBlock* prev = nullptr;

    while (ptr) {
      UINT64 ptr_size = ptr->size;
      // Heap type doesn't match -> continue
      if (ptr->heap_type != heap_type) ptr_size = 0;

      // Found a block that has the correct heap type, as well as enough size
      if (ptr_size >= size) {
        UINT64 ptr_offset = ptr->offset;
        ID3D12Heap* ptr_heap = ptr->heap;
        UINT32 ptr_heap_index = ptr->heap_index;

        if (ptr_size == size) {
          // Exact size match -> remove free block entirely
          if (prev)
            prev->next = ptr->next;
          else
            head_ = ptr->next;

          delete ptr;

#ifdef DX12_MA_DEBUG
          allocations_.insert(
              {size, ptr_offset, heap_type, ptr_heap_index, ptr_heap});
#endif

          return Allocation{size, ptr_offset, heap_type, ptr_heap_index,
                            ptr_heap};
        }

        // No exact size match -> split the free block
        ptr->size -= size;
        ptr->offset += size;

#ifdef DX12_MA_DEBUG
        allocations_.insert(
            {size, ptr_offset, heap_type, ptr_heap_index, ptr_heap});
#endif

        return Allocation{size, ptr_offset, heap_type, ptr_heap_index,
                          ptr_heap};
      }

      // Move to the next free block
      prev = ptr;
      ptr = ptr->next;
    }

    // Out of Memory (No suitable free block found) -> Allocate new heap
    // memory
    UINT64 heap_block_size = DX12_MA_HEAP_BLOCK_SIZE;
    if (heap_block_size - size <= 0) heap_block_size = size * 4;

    assert(device_);

    D3D12_HEAP_DESC heap_desc{};
    heap_desc.SizeInBytes = heap_block_size;
    heap_desc.Flags = D3D12_HEAP_FLAG_NONE;
    heap_desc.Properties.Type = heap_type;
    heap_desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

    ID3D12Heap* new_heap = nullptr;
    HRESULT hr = device_->CreateHeap(&heap_desc, IID_PPV_ARGS(&new_heap));
    if (FAILED(hr)) return {0};
    heaps_[heap_count_] = new_heap;

    // Create a new free block and reserve the allocation size
    FreeBlock* new_block =
        new FreeBlock(heap_block_size - size, size, heap_type, heap_count_,
                      nullptr, new_heap);

    // Set the new free block as new head
    new_block->next = head_;
    head_ = new_block;

#ifdef DX12_MA_DEBUG
    allocations_.insert({size, 0, heap_type, heap_count_, new_heap});
#endif

    heap_count_++;
    return Allocation{size, 0, heap_type, heap_count_ - 1, new_heap};
  }

  // Frees a previously allocated block and merges adjacent free blocks if
  // possible
  void Free(const Allocation& alloc) {
#ifdef DX12_MA_DEBUG
    allocations_.erase(alloc);
#endif

    FreeBlock* new_block =
        new FreeBlock(alloc.size, alloc.offset, alloc.heap_type,
                      alloc.heap_index, nullptr, alloc.heap);

    // head_ == nullptr, then insert directly at head
    if (!head_) {
      head_ = new_block;
      return;
    }

    FreeBlock* prev = nullptr;
    FreeBlock* current = head_;

    // Find a suitable location to insert the new block at, sorted by offset,
    // as well as the heap index
    while (current && (current->heap_index != alloc.heap_index ||
                       current->offset < alloc.offset)) {
      prev = current;
      current = current->next;
    }

    // Try merging with previous block
    if (prev && prev->offset + prev->size == new_block->offset &&
        prev->heap_index == new_block->heap_index) {
      prev->size += new_block->size;
      delete new_block;
      new_block = prev;
    } else {
      // Unable to merge, proceed to insert
      if (prev)
        prev->next = new_block;
      else
        // No block before the new one -> insert at head
        head_ = new_block;
      new_block->next = current;
    }

    // Try merging with next block
    if (current && new_block->offset + new_block->size == current->offset &&
        current->heap_index == new_block->heap_index) {
      new_block->size += current->size;
      new_block->next = current->next;
      delete current;
    }
  }

 private:
  FreeBlock* head_ = nullptr;
  ID3D12Device* device_ = nullptr;

  UINT32 heap_count_ = 0;
  ID3D12Heap* heaps_[DX12_MA_MAX_HEAP_COUNT]{};

#ifdef DX12_MA_DEBUG
  std::unordered_set<Allocation, AllocationHasher> allocations_;
#endif
};

template <typename T>
class ResourceWrapper {
 public:
  // ResourceWrapper does not take ownership of `mem_alloc` (`mem_alloc` is not
  // going to be released and has to be valid for the lifetime of
  // ResourceWrapper)
  ResourceWrapper(const Allocation& alloc, Allocator* mem_alloc)
      : alloc_(alloc), mem_alloc_(mem_alloc) {}
  ~ResourceWrapper() {
    if (mem_alloc_ == nullptr) return;
    mem_alloc_->Free(alloc_);
    if (resource_) {
      if (memory_mapped_) resource_->Unmap(0, nullptr);
      resource_->Release();
    }
  }

  ResourceWrapper(const ResourceWrapper&) = delete;
  ResourceWrapper& operator=(const ResourceWrapper&) = delete;

  ResourceWrapper(ResourceWrapper&& other) noexcept
      : alloc_(std::move(other.alloc_)),
        data_(other.data_),
        resouce_(other.resource_),
        memory_mapped_(other.memory_mapped_),
        mem_alloc_(other.mem_alloc_) {
    other.data_ = nullptr;
    other.resouce_ = nullptr;
    other.mem_alloc_ = nullptr;
    other.memory_mapped_ = false;
  }

  ResourceWrapper& operator=(ResourceWrapper&& other) noexcept {
    if (this != &other) {
      alloc_ = std::move(other.alloc_);
      data_ = other.data_;
      resouce_ = other.resource_;
      memory_mapped_ = other.memory_mapped_;
      mem_alloc_ = other.mem_alloc_;

      other.data_ = nullptr;
      other.resouce_ = nullptr;
      other.mem_alloc_ = nullptr;
      other.memory_mapped_ = false;
    }
    return *this;
  };

  inline char* memory() { return data_; }
  inline D3D12_GPU_VIRTUAL_ADDRESS get_gpu_address() {
    return resource_->GetGPUVirtualAddress();
  }

  inline void MapMemory() {
    D3D12_RANGE read_range{0, 0};
    resource_->Map(0, &read_range, reinterpret_cast<void**>(&data_));
    memory_mapped_ = true;
  }
  inline void UnmapMemory() {
    resource_->Unmap(0, nullptr);
    memory_mapped_ = false;
  }

  // set_resource takes ownership over `resource` and releases it on
  // deconstruction
  inline void set_resource(T* resource) { resource_ = resource; }
  inline T* get_resource() { return resource_; }
  inline T** get_resource_2r() { return &resource_; }

 private:
  Allocation alloc_;
  char* data_ = nullptr;
  T* resource_ = nullptr;
  bool memory_mapped_ = false;
  Allocator* mem_alloc_ = nullptr;

  template <typename U>
  struct has_release {
   private:
    template <typename C>
    static auto test(int)
        -> decltype(std::declval<C>().Release(), std::true_type{});

    template <typename>
    static std::false_type test(...) {}

   public:
    static constexpr bool value = decltype(test<U>(0))::value;
  };

  static_assert(has_release<T>::value, "Type T must have a Release method");
};

}  // namespace dx12_ma