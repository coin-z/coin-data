/**
 * @file allocator.hpp
 * @author zhoutong (zhoutotong@live.cn)
 * @brief 
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <utility>
#include <alloca.h>

#include <coin-commons/utils/utils.hpp>

namespace coin::data::__inner
{
template<typename T, typename MemManagerT>
class Allocator
{
    static_assert(not std::is_polymorphic<T>::value, "you can only allocator object that is not polymorphic.");

public:
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using const_pointer = const T*;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    template<typename _Tp1>
	struct rebind
	{ typedef Allocator<_Tp1, MemManagerT> other; };

public:
    Allocator() noexcept {}
    Allocator(const Allocator& __a) noexcept {}
    template<typename U>
    Allocator(const Allocator<U, MemManagerT>&) noexcept {}
    template<typename U>
    Allocator(Allocator<U, MemManagerT>&) noexcept {}
    
    ~Allocator() noexcept {}

    Allocator& operator = (const Allocator&) = default;

    bool operator == (const Allocator&) const noexcept { return true; }
    bool operator != (const Allocator&) const noexcept { return false; }

    pointer       address ( reference x )       const noexcept { return &x; }
    const_pointer address ( const_reference x ) const noexcept { return &x; }

    pointer allocate (size_type n)
    {
        pointer ptr = (pointer)MemManagerT::instance().mem().malloc(n * sizeof(T));
        if(not ptr)
        {
            coin::Print::error("allocate memory failed.");
            abort();
        }
        return ptr;
    }

    void deallocate (pointer p, size_type n)
    {
        MemManagerT::instance().mem().free(p);
    }

    size_type max_size() const noexcept { return __LONG_MAX__; }

    template <class U, class... Args>
    void construct (U* p, Args&&... args)
    {
        new (p)U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy (U* p)
    {
        p->~U();
    }
};
} // namespace coin::data::__inner
