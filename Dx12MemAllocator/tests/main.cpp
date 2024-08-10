#include <d3d12.h>
#include <dxgi1_6.h>
#include <gtest/gtest.h>
#include <wrl/client.h>

// #define DX12_MA_DEBUG
#include "dx12_ma.h"

using Microsoft::WRL::ComPtr;
using namespace dx12_ma;

class MemAllocatorTest : public ::testing::Test {
 protected:
  ComPtr<IDXGIFactory6> factory_ = nullptr;
  ComPtr<ID3D12Device> device_ = nullptr;

  MemAllocator allocator_;

  void SetUp() override {
    ASSERT_TRUE(SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory_))));

    ComPtr<IDXGIAdapter> adapter;
    ASSERT_TRUE(SUCCEEDED(factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter))));

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr =
        D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&device_));
    ASSERT_TRUE(SUCCEEDED(hr));

    allocator_ = (device_.Get());
    ASSERT_TRUE(SUCCEEDED(hr));
  }
};

TEST_F(MemAllocatorTest, AllocateCpuMemory) {
  // Allocate CPU memory
  Allocation alloc = allocator_.Allocate(1024, D3D12_HEAP_TYPE_UPLOAD);  // 1 KB
  ASSERT_GT(alloc.size, 0);
  ASSERT_EQ(alloc.heap_type, D3D12_HEAP_TYPE_UPLOAD);
  ASSERT_NE(alloc.heap, nullptr);
}

TEST_F(MemAllocatorTest, AllocateGpuMemory) {
  // Allocate GPU memory
  Allocation alloc =
      allocator_.Allocate(1024, D3D12_HEAP_TYPE_DEFAULT);  // 1 KB
  ASSERT_GT(alloc.size, 0);
  ASSERT_EQ(alloc.heap_type, D3D12_HEAP_TYPE_DEFAULT);
  ASSERT_NE(alloc.heap, nullptr);
}

TEST_F(MemAllocatorTest, AllocateAndFree) {
  // Allocate CPU memory
  Allocation alloc = allocator_.Allocate(1024, D3D12_HEAP_TYPE_UPLOAD);  // 1 KB
  ASSERT_GT(alloc.size, 0);

  // Free the allocation
  allocator_.Free(alloc);

  // Check if free blocks merged correctly
  uint32_t free_blocks_size = allocator_.get_free_blocks_size();
  ASSERT_EQ(free_blocks_size, 1);
}

TEST_F(MemAllocatorTest, MultipleAllocationsAndFrees) {
  // Allocate multiple blocks
  Allocation alloc1 =
      allocator_.Allocate(512, D3D12_HEAP_TYPE_UPLOAD);  // 512 bytes
  Allocation alloc2 =
      allocator_.Allocate(512, D3D12_HEAP_TYPE_DEFAULT);  // 512 bytes
  Allocation alloc3 =
      allocator_.Allocate(256, D3D12_HEAP_TYPE_UPLOAD);  // 256 bytes

  ASSERT_EQ(alloc1.size, 512);
  ASSERT_EQ(alloc2.size, 512);
  ASSERT_EQ(alloc3.size, 256);
  ASSERT_EQ(alloc1.offset, 0);
  ASSERT_EQ(alloc2.offset, 0);  // gpu allocated, therefore 0
  ASSERT_EQ(alloc3.offset, 512);

  // Free the allocations
  allocator_.Free(alloc1);
  allocator_.Free(alloc2);
  allocator_.Free(alloc3);

  // Check if free blocks merged correctly
  ASSERT_EQ(allocator_.get_free_blocks_size(), 2);
}

TEST_F(MemAllocatorTest, AllocateAndFreeAndAllocate) {
  // Allocate multiple blocks
  Allocation alloc1 =
      allocator_.Allocate(256, D3D12_HEAP_TYPE_UPLOAD);  // 256 bytes
  ASSERT_EQ(alloc1.size, 256);
  ASSERT_EQ(alloc1.offset, 0);

  Allocation alloc2 =
      allocator_.Allocate(512, D3D12_HEAP_TYPE_UPLOAD);  // 512 bytes
  ASSERT_EQ(alloc2.size, 512);
  ASSERT_EQ(alloc2.offset, 256);

  allocator_.Free(alloc2);

  Allocation alloc3 =
      allocator_.Allocate(1024, D3D12_HEAP_TYPE_UPLOAD);  // 1024 bytes
  ASSERT_EQ(alloc3.size, 1024);
  ASSERT_EQ(alloc3.offset, 256);

  Allocation alloc4 =
      allocator_.Allocate(1024, D3D12_HEAP_TYPE_UPLOAD);  // 1024 bytes
  ASSERT_EQ(alloc4.size, 1024);
  ASSERT_EQ(alloc4.offset, 1024 + 256);

  Allocation alloc5 =
      allocator_.Allocate(1024, D3D12_HEAP_TYPE_UPLOAD);  // 1024 bytes
  ASSERT_EQ(alloc5.size, 1024);
  ASSERT_EQ(alloc5.offset, 1024 + 1024 + 256);

  allocator_.Free(alloc5);

  Allocation alloc6 =
      allocator_.Allocate(1024, D3D12_HEAP_TYPE_UPLOAD);  // 1024 bytes
  ASSERT_EQ(alloc6.size, 1024);
  ASSERT_EQ(alloc6.offset, 1024 + 1024 + 256);

  allocator_.Free(alloc1);
  allocator_.Free(alloc3);
  allocator_.Free(alloc4);
  allocator_.Free(alloc6);

  Allocation alloc7 =
      allocator_.Allocate(4096, D3D12_HEAP_TYPE_UPLOAD);  // 4096 bytes
  ASSERT_EQ(alloc7.size, 4096);
  ASSERT_EQ(
      alloc7.offset,
      0);  // All previous memory was freed, so the offset should restart at 0

  // Check if free blocks merged correctly
  uint32_t free_blocks_size = allocator_.get_free_blocks_size();
  ASSERT_EQ(free_blocks_size, 1);
}

TEST_F(MemAllocatorTest, OverAllocateCpuMemory) {
  // Attempt to allocate more memory than available
  Allocation alloc =
      allocator_.Allocate(DX12_MA_HEAP_BLOCK_SIZE + 1, D3D12_HEAP_TYPE_UPLOAD);
  ASSERT_NE(alloc.size,
            0);  // Should not fail, as a new heap should be allocated

  // Check if free blocks are correct
  uint32_t free_blocks_size = allocator_.get_free_blocks_size();
  ASSERT_EQ(free_blocks_size, 1);
}

TEST_F(MemAllocatorTest, AllocateExactHeapSize) {
  // Allocate entire CPU heap
  Allocation alloc =
      allocator_.Allocate(DX12_MA_HEAP_BLOCK_SIZE, D3D12_HEAP_TYPE_UPLOAD);
  ASSERT_EQ(alloc.size, DX12_MA_HEAP_BLOCK_SIZE);

  // Ensure that the next allocation doesn't fail
  Allocation alloc_fail = allocator_.Allocate(1, D3D12_HEAP_TYPE_UPLOAD);
  ASSERT_NE(alloc_fail.size, 0);

  // Check if free blocks are correct
  uint32_t free_blocks_size = allocator_.get_free_blocks_size();
  ASSERT_EQ(free_blocks_size, 1);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
