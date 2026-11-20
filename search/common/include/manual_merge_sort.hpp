#ifndef WEBSEARCH_SEARCH_COMMON_INCLUDE_MANUAL_MERGE_SORT_HPP_
#define WEBSEARCH_SEARCH_COMMON_INCLUDE_MANUAL_MERGE_SORT_HPP_

#include <cstddef>
#include <utility>
#include <vector>

namespace search_common {

// Records supply Before(a, b). No library sorting or auxiliary lookup containers.
class ManualMergeSort {
   public:
    template <class Record>
    static void Sort(std::vector<Record>& values) {
        std::vector<Record> buffer(values.size());
        for (std::size_t width = 1; width < values.size();) {
            for (std::size_t begin = 0; begin < values.size();) {
                const auto middle = begin + Smaller(width, values.size() - begin);
                const auto end = middle + Smaller(width, values.size() - middle);
                auto left = begin;
                auto right = middle;
                for (auto out = begin; out < end; ++out) {
                    if (right == end || (left < middle && !Record::Before(values[right], values[left]))) {
                        buffer[out] = std::move(values[left++]);
                    } else {
                        buffer[out] = std::move(values[right++]);
                    }
                }
                begin = end;
            }
            values.swap(buffer);
            if (width > values.size() / 2) {
                break;
            }
            width *= 2;
        }
    }

   private:
    static std::size_t Smaller(std::size_t a, std::size_t b) { return a < b ? a : b; }
};

}  // namespace search_common
#endif  // WEBSEARCH_SEARCH_COMMON_INCLUDE_MANUAL_MERGE_SORT_HPP_
