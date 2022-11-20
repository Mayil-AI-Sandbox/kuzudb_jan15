#pragma once

#include <cassert>
#include <cstring>

#include "third_party/utf8proc/include/utf8proc.h"

#include "src/common/types/include/ku_string.h"

using namespace std;
using namespace kuzu::common;
using namespace kuzu::utf8proc;

namespace kuzu {
namespace function {
namespace operation {

struct Length {
    static inline void operation(ku_string_t& input, int64_t& result) {
        auto totalByteLength = input.len;
        auto inputString = input.getAsString();
        for (auto i = 0; i < totalByteLength; i++) {
            if (inputString[i] & 0x80) {
                int64_t length = 0;
                // Use grapheme iterator to identify bytes of utf8 char and increment once for each
                // char.
                utf8proc_grapheme_callback(
                    inputString.c_str(), totalByteLength, [&](size_t start, size_t end) {
                        length++;
                        return true;
                    });
                result = length;
                return;
            }
        }
        result = totalByteLength;
    }
};

} // namespace operation
} // namespace function
} // namespace kuzu
