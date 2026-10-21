#pragma once

#include <atomic>
#include <cstdint>

struct ParsedDataset;

namespace spirula {
namespace sky {

struct Rows;

int64_t bake_dataset(Rows& rows, const ParsedDataset& dataset,
                     const std::atomic<bool>& stop_requested);

}  // namespace sky
}  // namespace spirula
