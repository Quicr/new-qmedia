#pragma once

#include <cstddef>
#include <limits>
#include <utility>
#include <deque>
#include <memory>

template <typename T,
          typename U,
          typename Allocator = std::allocator<std::pair<T, U>>>
class ModulusDeque
{
protected:
    // Define the midpoint value for the type T
    static const unsigned midpoint_value = std::numeric_limits<T>::max() / 2 +
                                           1;

public:
    // Define a type useful for use with GetDeque()
    typedef std::deque<std::pair<T, U>, Allocator> SortedPairDeque;

    ModulusDeque(bool allow_duplicates = true);
    ~ModulusDeque();
    bool Insert(const std::pair<T, U> &item);
    bool Insert(std::pair<T, U> &&item);
    bool AllKeysConsecutive() const;
    bool Empty() const;
    std::size_t Size() const;
    const SortedPairDeque &GetDeque() const;
    const std::pair<T, U> &Front() const;
    void PopFront();

protected:
    bool allow_duplicates;
    SortedPairDeque sorted_pair_deque;
};

// This allows the template object to be implemented in a separate cpp file
// without receiving linker errors
#include "../src/modulus_deque.cc"

