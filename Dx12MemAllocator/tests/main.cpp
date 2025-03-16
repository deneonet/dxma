#include <d3d12.h>
#include <dxgi1_6.h>
#include <gtest/gtest.h>
#include <wrl/client.h>

#define DXMA_DEBUG
#include "dxma.h"

using Microsoft::WRL::ComPtr;

// Test fixture for DirectX 12 Memory Allocator tests
class DirectXMemoryAllocatorTest : public ::testing::Test {
 protected:
  ComPtr<IDXGIFactory6> dxgiFactory_ =
      nullptr;  // DXGI factory for creating adapters
  ComPtr<ID3D12Device> d3dDevice_ = nullptr;  // DirectX 12 device
  DxmaAllocator memoryAllocator_ = nullptr;   // Memory allocator instance

  void SetUp() override {
    // Create DXGI factory
    ASSERT_TRUE(SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_))));

    // Create a WARP adapter (software rendering)
    ComPtr<IDXGIAdapter> warpAdapter;
    ASSERT_TRUE(
        SUCCEEDED(dxgiFactory_->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))));

    // Create DirectX 12 device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D12CreateDevice(warpAdapter.Get(), featureLevel,
                                   IID_PPV_ARGS(&d3dDevice_));
    ASSERT_TRUE(SUCCEEDED(hr));

    // Initialize the memory allocator
    dxmaCreateAllocator(&memoryAllocator_, d3dDevice_.Get());
  }

  void TearDown() override {
    // Clean up the memory allocator
    dxmaDestroyAllocator(memoryAllocator_);
  }
};

// Test case: Allocate CPU-accessible memory
TEST_F(DirectXMemoryAllocatorTest, AllocateCpuAccessibleMemory) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 1024;                    // 1 KB
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap

  DxmaAllocation allocation = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation);

  ASSERT_NE(allocation, nullptr);
  ASSERT_GT(allocation->GetSize(), 0);
  ASSERT_EQ(allocation->GetHeapType(), D3D12_HEAP_TYPE_UPLOAD);
  ASSERT_NE(allocation->GetHeap(), nullptr);
}

// Test case: Allocate CPU-accessible memory and ensure it is freed correctly
TEST_F(DirectXMemoryAllocatorTest, AllocateAndFreeCpuAccessibleMemory) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 1024;                    // 1 KB
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap

  DxmaAllocation allocation = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation);

  ASSERT_NE(allocation, nullptr);
  ASSERT_GT(allocation->GetSize(), 0);
  ASSERT_EQ(allocation->GetHeapType(), D3D12_HEAP_TYPE_UPLOAD);
  ASSERT_NE(allocation->GetHeap(), nullptr);

  // Free the allocation
  dxmaFree(memoryAllocator_, allocation, nullptr);

  // Ensure no memory leaks are reported in debug mode
}

// Test case: Allocate GPU-accessible memory
TEST_F(DirectXMemoryAllocatorTest, AllocateGpuAccessibleMemory) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 1024;                     // 1 KB
  allocationInfo.type = D3D12_HEAP_TYPE_DEFAULT;  // GPU-accessible heap

  DxmaAllocation allocation = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation);

  ASSERT_NE(allocation, nullptr);
  ASSERT_GT(allocation->GetSize(), 0);
  ASSERT_EQ(allocation->GetHeapType(), D3D12_HEAP_TYPE_DEFAULT);
  ASSERT_NE(allocation->GetHeap(), nullptr);
}

// Test case: Allocate and free memory, then verify free block merging
TEST_F(DirectXMemoryAllocatorTest, AllocateFreeAndVerifyFreeBlockMerging) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 1024;                    // 1 KB
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap

  DxmaAllocation allocation = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation);

  // Free the allocation
  dxmaFree(memoryAllocator_, allocation, nullptr);

  // Verify that free blocks are merged correctly
  uint32_t freeBlockCount = memoryAllocator_->GetFreeBlockCount();
  ASSERT_EQ(freeBlockCount, 1);
}

// Test case: Allocate multiple memory blocks and free them
TEST_F(DirectXMemoryAllocatorTest, AllocateMultipleBlocksAndFreeThem) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 512;                     // 512 bytes
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap

  // Allocate three blocks
  DxmaAllocation allocation1 = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation1);

  allocationInfo.type = D3D12_HEAP_TYPE_DEFAULT;  // GPU-accessible heap
  DxmaAllocation allocation2 = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation2);

  allocationInfo.size = 256;                     // 256 bytes
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap
  DxmaAllocation allocation3 = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation3);

  // Verify allocations
  ASSERT_EQ(allocation1->GetSize(), 512);
  ASSERT_EQ(allocation2->GetSize(), 512);
  ASSERT_EQ(allocation3->GetSize(), 256);

  // Free all allocations
  dxmaFree(memoryAllocator_, allocation1, nullptr);
  dxmaFree(memoryAllocator_, allocation2, nullptr);
  dxmaFree(memoryAllocator_, allocation3, nullptr);

  // Verify free block merging
  ASSERT_EQ(memoryAllocator_->GetFreeBlockCount(), 2);
}

// Test case: Allocate, free, and reallocate memory
TEST_F(DirectXMemoryAllocatorTest, AllocateFreeAndReallocateMemory) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 256;                     // 256 bytes
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap

  // Allocate first block
  DxmaAllocation allocation1 = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation1);
  ASSERT_EQ(allocation1->GetSize(), 256);
  ASSERT_EQ(allocation1->GetOffset(), 0);

  // Allocate second block
  allocationInfo.size = 512;  // 512 bytes
  DxmaAllocation allocation2 = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation2);
  ASSERT_EQ(allocation2->GetSize(), 512);
  ASSERT_EQ(allocation2->GetOffset(), 256);

  // Free the second allocation
  dxmaFree(memoryAllocator_, allocation2, nullptr);

  // Allocate a larger block
  allocationInfo.size = 1024;  // 1 KB
  DxmaAllocation allocation3 = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation3);
  ASSERT_EQ(allocation3->GetSize(), 1024);
  ASSERT_EQ(allocation3->GetOffset(), 256);

  // Free all allocations
  dxmaFree(memoryAllocator_, allocation1, nullptr);
  dxmaFree(memoryAllocator_, allocation3, nullptr);

  // Verify free block merging
  ASSERT_EQ(memoryAllocator_->GetFreeBlockCount(), 1);
}

// Test case: Allocate more memory than the default heap size
TEST_F(DirectXMemoryAllocatorTest, AllocateMemoryLargerThanDefaultHeapSize) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = DXMA_HEAP_BLOCK_SIZE + 1;  // Exceed default heap size
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;    // CPU-accessible heap

  DxmaAllocation allocation = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation);

  ASSERT_NE(allocation, nullptr);
  ASSERT_GT(allocation->GetSize(), 0);

  // Verify free block count
  ASSERT_EQ(memoryAllocator_->GetFreeBlockCount(), 1);
}

// Test case: Allocate memory and create a DirectX 12 resource
TEST_F(DirectXMemoryAllocatorTest, AllocateMemoryAndCreateResource) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 1024;                    // 1 KB
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap

  DxmaAllocation allocation = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation);

  ASSERT_NE(allocation, nullptr);

  // Create a DirectX 12 resource
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

  HRESULT hr = dxmaCreateResource(memoryAllocator_, allocation, &resourceDesc,
                                  D3D12_RESOURCE_STATE_COMMON);
  ASSERT_TRUE(SUCCEEDED(hr));

  // Free the allocation (resource will be auto-managed)
  dxmaFree(memoryAllocator_, allocation);
}

// Test case: Map and unmap memory for CPU access
TEST_F(DirectXMemoryAllocatorTest, MapAndUnmapMemoryForCpuAccess) {
  DxmaAllocationInfo allocationInfo{};
  allocationInfo.size = 1024;                    // 1 KB
  allocationInfo.type = D3D12_HEAP_TYPE_UPLOAD;  // CPU-accessible heap

  DxmaAllocation allocation = nullptr;
  dxmaAllocate(memoryAllocator_, allocationInfo, &allocation);

  ASSERT_NE(allocation, nullptr);

  // Create a DirectX 12 resource
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

  HRESULT hr = dxmaCreateResource(memoryAllocator_, allocation, &resourceDesc,
                                  D3D12_RESOURCE_STATE_COMMON);
  ASSERT_TRUE(SUCCEEDED(hr));

  // Map the memory for CPU access
  void* mappedData = nullptr;
  hr = dxmaMapMemory(allocation, &mappedData);
  ASSERT_TRUE(SUCCEEDED(hr));
  ASSERT_TRUE(allocation->IsMemoryMapped());

  // Write data to the mapped memory
  memcpy(mappedData, "Hello", 5);

  // Unmap the memory
  dxmaUnmapMemory(allocation);
  ASSERT_FALSE(allocation->IsMemoryMapped());

  // Free the allocation
  dxmaFree(memoryAllocator_, allocation);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}