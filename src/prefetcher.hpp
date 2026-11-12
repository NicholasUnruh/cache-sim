#pragma once

#include <cstdint>
#include <vector>

// Stride prefetcher without a PC: the traces carry only addresses, so it
// watches one global stream of block addresses. When two consecutive
// trigger blocks are the same nonzero stride apart (A, A+s, A+2s), it
// prefetches the next `degree` blocks along that stride.
class StridePrefetcher {
 public:
  explicit StridePrefetcher(uint32_t degree) : degree_(degree) {}

  // Feeds one trigger block; returns the blocks to prefetch (maybe none).
  std::vector<uint64_t> train(uint64_t block) {
    int64_t stride = int64_t(block - last_block_);
    if (stride == 0) return {};
    bool confirmed = stride == last_stride_;
    last_block_ = block;
    last_stride_ = stride;
    std::vector<uint64_t> blocks;
    if (confirmed)
      for (uint32_t k = 1; k <= degree_; ++k) blocks.push_back(block + uint64_t(stride) * k);
    return blocks;
  }

 private:
  uint32_t degree_;
  uint64_t last_block_ = 0;
  int64_t last_stride_ = 0;
};
