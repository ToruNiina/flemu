#ifndef FLEMU_UTILITY_HPP
#define FLEMU_UTILITY_HPP
#include <boost/ut.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>

#include <concepts>

namespace flemu
{

// Both ends are included.
//                 7654 3210
// mask(1,3) == 0b'0000'1110;
//
template<std::unsigned_integral UInt>
constexpr UInt mask(const std::size_t x, const std::size_t y) noexcept
{
    const auto start = std::min(x, y);
    const auto stop  = std::max(x, y);

    const std::size_t width = stop - start + 1;
    if (width < 8 * sizeof(UInt))
    {
        const auto m = (UInt(1) << width) - 1;
        return m << start;
    }
    else
    {
        return std::numeric_limits<UInt>::max();
    }
}

template<std::unsigned_integral UInt>
constexpr int bit_at(const UInt x, const std::size_t i) noexcept
{
    if (i < 8 * sizeof(UInt))
    {
        return (x & mask<UInt>(i, i)) == 0 ? 0 : 1;
    }
    else
    {
        return 0;
    }
}

template<std::unsigned_integral UInt>
std::string as_bit(const UInt x) noexcept
{
    std::string str;
    const auto width = sizeof(UInt)*8;
    for(std::size_t i=0; i<width; ++i)
    {
        if(i % 4 == 0 && i != 0)
        {
            str += '\'';
        }
        str += bit_at(x, width-i-1) + '0';
    }
    return str;
}

// only g++11 supports std::bit_cast ...
template<typename T, typename U>
T bit_cast(const U& u)
{
    static_assert(sizeof(T) == sizeof(U));
    T t;
    std::memcpy(reinterpret_cast<char*>(std::addressof(t)),
                reinterpret_cast<const char*>(std::addressof(u)),
                sizeof(T));
    return t;
}

#ifdef FLEMU_ACTIVATE_UNIT_TESTS
inline boost::ut::suite tests_utility = [] {
    using namespace boost::ut::literals;

    "mask"_test = [] {
        boost::ut::expect(mask<std::uint32_t>( 1,  3) == 0b00001110);
        boost::ut::expect(mask<std::uint32_t>( 3,  1) == 0b00001110);
        boost::ut::expect(mask<std::uint32_t>( 3,  3) == 0b00001000);
        boost::ut::expect(mask<std::uint32_t>(31,  0) == 0xFFFFFFFF);
        boost::ut::expect(mask<std::uint32_t>( 0, 31) == 0xFFFFFFFF);
        boost::ut::expect(mask<std::uint32_t>(31, 31) == 0x80000000);

        boost::ut::expect(mask<std::uint64_t>(31,  0) == 0xFFFF'FFFF);
        boost::ut::expect(mask<std::uint64_t>(47, 32) == 0x0000'FFFF'0000'0000);
        boost::ut::expect(mask<std::uint64_t>(63,  0) == 0xFFFF'FFFF'FFFF'FFFFull);
        boost::ut::expect(mask<std::uint64_t>(63, 63) == 0x8000'0000'0000'0000ull);
    };
};
#endif

} // flemu
#endif// FLEMU_UTILITY_HPP
