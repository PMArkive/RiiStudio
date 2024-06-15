#pragma once

#include <algorithm>        // std::fill
#include <core/common.h>    // assert
#include <string_view>      // std::string_view

namespace rsl {

class StringBuilder {
public:
  StringBuilder(char* buf, std::size_t size)
      : mBuf(buf), mIt(buf), mEnd(buf + size) {
    std::fill(mBuf, mEnd, '\0');
  }

  void append(std::string_view string) {
    assert(mIt + string.length() < mEnd);
    std::memcpy(mIt, string.data(), string.length());
    mIt += string.length();
  }
  void reset() {
    std::fill(mBuf, mIt, '\0');
    mIt = mBuf;
  }

  StringBuilder& operator+=(std::string_view string) {
    append(string);
    return *this;
  }

private:
  char* mBuf;
  char* mIt;
  char* mEnd;
};

} // namespace rsl
