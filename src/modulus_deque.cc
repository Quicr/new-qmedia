

#ifndef MODULUS_DEQUE_CPP
#define MODULUS_DEQUE_CPP

#include "modulus_deque.hh"

template <typename T, typename U, typename Allocator>
ModulusDeque<T, U, Allocator>::ModulusDeque(bool allow_duplicates) :
    allow_duplicates(allow_duplicates)
{
}

template <typename T, typename U, typename Allocator>
ModulusDeque<T, U, Allocator>::~ModulusDeque()
{
}

template <typename T, typename U, typename Allocator>
bool ModulusDeque<T, U, Allocator>::Insert(const std::pair<T, U> &item)
{
    T delta;        // Difference in key values

    // If the dqeue buffer is empty, just insert it
    if (sorted_pair_deque.empty())
    {
        sorted_pair_deque.push_back(item);

        return true;
    }

    // Iterate over the deque and insert the {key, value} in order
    for (auto it = sorted_pair_deque.begin(); it != sorted_pair_deque.end();
         it++)
    {
        // Do we have a duplicate key?
        if (!allow_duplicates && ((*it).first == item.first))
        {
            return false;
        }

        // Compute the difference between the given key and the key
        // in the deque at this place; a delta >= midpoint_number
        // means the given key has a lesser value
        delta = item.first - (*it).first;
        if (delta >= midpoint_value)
        {
            // Insert the packet at this point in the buffer
            sorted_pair_deque.insert(it, item);

            return true;
        }
    }

    // The key must be a higher value, so just append it
    sorted_pair_deque.push_back(item);

    return true;
}

template <typename T, typename U, typename Allocator>
bool ModulusDeque<T, U, Allocator>::Insert(std::pair<T, U> &&item)
{
    T delta;        // Difference in key values

    // If the dqeue buffer is empty, just insert it
    if (sorted_pair_deque.empty())
    {
        sorted_pair_deque.push_back(std::move(item));

        return true;
    }

    // Iterate over the deque and insert the {key, value} in order
    for (auto it = sorted_pair_deque.begin(); it != sorted_pair_deque.end();
         it++)
    {
        // Do we have a duplicate key?
        if (!allow_duplicates && ((*it).first == item.first))
        {
            return false;
        }

        // Compute the difference between the given key and the key
        // in the deque at this place; a delta >= midpoint_number
        // means the given key has a lesser value
        delta = item.first - (*it).first;
        if (delta >= midpoint_value)
        {
            // Insert the packet at this point in the buffer
            sorted_pair_deque.insert(it, std::move(item));

            return true;
        }
    }

    // The key must be a higher value, so just append it
    sorted_pair_deque.push_back(std::move(item));

    return true;
}

template <typename T, typename U, typename Allocator>
bool ModulusDeque<T, U, Allocator>::AllKeysConsecutive() const
{
    bool first = true;

    // Initialize the prev to hold the value at the front of the deque
    std::uint16_t prev = sorted_pair_deque.front().first;

    // Iterate over the deque to see if values are consecutive
    for (auto &item : sorted_pair_deque)
    {
        if (first)
        {
            first = false;
            continue;
        }
        if (item.first == prev) continue;
        if (item.first != ++prev) return false;
    }

    return true;
}

template <typename T, typename U, typename Allocator>
bool ModulusDeque<T, U, Allocator>::Empty() const
{
    return sorted_pair_deque.empty();
}

template <typename T, typename U, typename Allocator>
std::size_t ModulusDeque<T, U, Allocator>::Size() const
{
    return sorted_pair_deque.size();
}

template <typename T, typename U, typename Allocator>
const typename ModulusDeque<T, U, Allocator>::SortedPairDeque &
ModulusDeque<T, U, Allocator>::GetDeque() const
{
    return sorted_pair_deque;
}

template <typename T, typename U, typename Allocator>
const std::pair<T, U> &ModulusDeque<T, U, Allocator>::Front() const
{
    return sorted_pair_deque.front();
}

template <typename T, typename U, typename Allocator>
void ModulusDeque<T, U, Allocator>::PopFront()
{
    sorted_pair_deque.pop_front();
}

#endif        // MODULUS_DEQUE_CPP
