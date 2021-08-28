#ifndef FLEMU_PROXY_HPP
#define FLEMU_PROXY_HPP

#include "utility.hpp"

#include <boost/ut.hpp>

#include <cassert>
#include <cstdint>

#include <compare>
#include <concepts>

namespace flemu
{

template<std::unsigned_integral Base>
struct bit_proxy
{
  public:

    using base_type = Base;

  public:

    // [start, stop] both ends are included
    constexpr bit_proxy(base_type& b, const std::size_t start, const std::size_t stop) noexcept
      : base_(std::addressof(b)), start_(std::min(start, stop)), stop_(std::max(start, stop)),
        mask_(mask<base_type>(0, start_-1) | mask<base_type>(stop_+1, sizeof(base_type)*8))
    {
        assert(stop_ < sizeof(base_type)*8);
    }

    constexpr bit_proxy& operator=(base_type i)
    {
        (*base_) &= mask_;
        (*base_) += ((i << start_) & mask<base_type>(stop_, start_));
        return *this;
    }

    constexpr explicit operator base_type() const
    {
        const base_type x = (*base_);
        return (x & mask<base_type>(stop_, start_)) >> start_;
    }

    constexpr auto operator==(const base_type& other) const
    {
        return other == base_type(*this);
    }
    constexpr auto operator<=>(const base_type& other) const
    {
        return other <=> base_type(*this);
    }

    constexpr auto operator==(const bit_proxy& other) const
    {
        return base_type(*this) == base_type(other);
    }
    constexpr auto operator<=>(const bit_proxy& other) const
    {
        return base_type(*this) <=> base_type(other);
    }

    constexpr std::size_t start() const noexcept {return start_;}
    constexpr std::size_t stop()  const noexcept {return stop_;}
    constexpr std::size_t width() const noexcept {return stop_ - start_ + 1;}

  private:
    base_type*  base_;
    std::size_t start_;
    std::size_t stop_;
    base_type   mask_;
};

template<typename charT, typename traits, typename Base>
std::basic_ostream<charT, traits>&
operator<<(std::basic_ostream<charT, traits>& os, const bit_proxy<Base>& bp)
{
    os << Base(bp);
    return os;
}

template<std::unsigned_integral Base>
struct const_bit_proxy
{
  public:

    using base_type = Base;

  public:

    // [start, stop] both ends are included
    constexpr const_bit_proxy(const base_type& b, const std::size_t start, const std::size_t stop) noexcept
      : base_(std::addressof(b)), start_(std::min(start, stop)), stop_(std::max(start, stop)),
        mask_(mask<base_type>(0, start_-1) | mask<base_type>(stop_+1, sizeof(base_type)*8))
    {
        assert(stop_ < sizeof(base_type)*8);
    }

    constexpr explicit operator base_type() const
    {
        const auto x = (*base_);
        return (x & mask<base_type>(stop_, start_)) >> start_;
    }

    constexpr auto operator==(const base_type& other) const
    {
        return other == base_type(*this);
    }
    constexpr auto operator<=>(const base_type& other) const
    {
        return other <=> base_type(*this);
    }

    constexpr auto operator==(const const_bit_proxy& other) const
    {
        return base_type(*this) == base_type(other);
    }
    constexpr auto operator<=>(const const_bit_proxy& other) const
    {
        return base_type(*this) <=> base_type(other);
    }

    constexpr std::size_t start() const noexcept {return start_;}
    constexpr std::size_t stop()  const noexcept {return stop_;}
    constexpr std::size_t width() const noexcept {return stop_ - start_ + 1;}

  private:
    base_type const* base_;
    std::size_t start_;
    std::size_t stop_;
    std::size_t width_;
    base_type   mask_;
};

template<typename charT, typename traits, typename Base>
std::basic_ostream<charT, traits>&
operator<<(std::basic_ostream<charT, traits>& os, const const_bit_proxy<Base>& bp)
{
    os << Base(bp);
    return os;
}

#ifdef FLEMU_ACTIVATE_UNIT_TESTS
inline boost::ut::suite tests_bit_proxy = []
{
    using namespace boost::ut::literals;

    "bit_proxy_substitution"_test = []
    {
        std::uint32_t u32 = 0x00FF'0F0F;
        bit_proxy proxy1(u32, 15, 0);

        boost::ut::expect(proxy1.start() ==  0);
        boost::ut::expect(proxy1.stop()  == 15);
        boost::ut::expect(proxy1.width() == 16);

        const std::uint32_t proxy1_data(proxy1);
        boost::ut::expect(proxy1      == 0x0F0F);
        boost::ut::expect(proxy1_data == 0x0F0F);

        proxy1 = 0xF0F0;
        boost::ut::expect(u32 == 0x00FF'F0F0);

        bit_proxy proxy2(u32, 23, 8);
        boost::ut::expect(proxy2 == 0xFFF0);

        proxy2 = 0x000F;
        boost::ut::expect(u32 == 0x0000'0FF0);
        boost::ut::expect(proxy2 == 0x000F);

        bit_proxy proxy3(u32, 31, 16);
        boost::ut::expect(proxy3 == 0x0000);

        proxy3 = 0xDEAD;
        boost::ut::expect(u32 == 0xDEAD'0FF0);
        boost::ut::expect(proxy3 == 0xDEAD);

        proxy1 = 0xBEEF'BEEF;
        boost::ut::expect(u32 == 0xDEAD'BEEF);

        bit_proxy proxy4(u32, 31, 31);
        boost::ut::expect(proxy4 == 1);
    };

    "bit_proxy_comparison"_test = []
    {
        std::uint32_t u32 = 0x00FF'0F0F;
        bit_proxy proxy1(u32, 15,  0); // 0F0F
        bit_proxy proxy2(u32, 23,  8); // FF0F
        bit_proxy proxy3(u32, 31, 16); // 00FF

        boost::ut::expect(proxy1 < proxy2);
        boost::ut::expect(proxy1 > proxy3);
        boost::ut::expect(proxy2 > proxy3);
    };
};
#endif

} // flemu
#endif// FLEMU_PROXY_HPP
