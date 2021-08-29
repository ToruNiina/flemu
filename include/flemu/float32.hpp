#ifndef FLEMU_FLOATING_HPP
#define FLEMU_FLOATING_HPP

#include "utility.hpp"
#include "bit_proxy.hpp"

#include <boost/ut.hpp>

#include <cstdint>
#include <cstring>
#include <bit>

namespace flemu
{

template<std::size_t Exponent, std::size_t Mantissa, std::uint32_t Bias>
struct basic_float32
{
  public:

    static_assert(1 + Exponent + Mantissa == 32);

    using base_type        = std::uint32_t;
    using proxy_type       = bit_proxy<base_type>;
    using const_proxy_type = const_bit_proxy<base_type>;

  public:

    constexpr basic_float32(const base_type b) noexcept
        : value_(b)
    {}
    constexpr basic_float32(const base_type sgn, const base_type exp, const base_type man) noexcept
        : value_( ((sgn << 31)       & mask<base_type>(31, 31)) +
                  ((exp << Mantissa) & mask<base_type>(30, Mantissa)) +
                  ( man              & mask<base_type>(Mantissa-1, 0)))
    {}

    constexpr const_proxy_type sign()     const noexcept {return const_bit_proxy(value_, 31, 31);}
    constexpr const_proxy_type exponent() const noexcept {return const_bit_proxy(value_, 30, Mantissa);}
    constexpr const_proxy_type mantissa() const noexcept {return const_bit_proxy(value_, Mantissa-1, 0);}

    constexpr proxy_type sign()     noexcept {return bit_proxy(value_, 31, 31);}
    constexpr proxy_type exponent() noexcept {return bit_proxy(value_, 30, Mantissa);}
    constexpr proxy_type mantissa() noexcept {return bit_proxy(value_, Mantissa-1, 0);}

    constexpr bool is_nan() const noexcept
    {
        return this->exponent() == 0b1111'1111 && this->mantissa() != 0;
    }
    constexpr bool is_inf() const noexcept
    {
        return this->exponent() == 0b1111'1111 && this->mantissa() == 0;
    }

    constexpr std::uint32_t bias()  const noexcept {return Bias;}
    constexpr std::uint32_t base() const noexcept {return value_;}

  private:

    std::uint32_t value_;
};
using float32 = basic_float32<8, 23, 127>;

inline float to_float(const float32 x) noexcept
{
    return bit_cast<float>(x.base());
}

inline float32 to_flemu(const float x) noexcept
{
    return float32(bit_cast<std::uint32_t>(x));
}

#ifdef FLEMU_ACTIVATE_UNIT_TESTS
inline boost::ut::suite tests_basic_float32 = []
{
    using namespace boost::ut::literals;

    "float32"_test = []
    {
        float32 x1(0b0'10000000'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x1.base() == 0b0'10000000'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x1.sign() == 0);
        boost::ut::expect(x1.exponent() == 0b1000'0000);
        boost::ut::expect(x1.mantissa() == 0b1101'1011'0110'1101'1011'011);

        float32 x2(0b1'01111111'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x2.base() == 0b1'01111111'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x2.sign() == 1);
        boost::ut::expect(x2.exponent() == 0b0111'1111);
        boost::ut::expect(x2.mantissa() == 0b1101'1011'0110'1101'1011'011);

        const float32 x3(0b0'10000000'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x3.base() == 0b0'10000000'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x3.sign() == 0);
        boost::ut::expect(x3.exponent() == 0b1000'0000);
        boost::ut::expect(x3.mantissa() == 0b1101'1011'0110'1101'1011'011);

        const float32 x4(0b1'01111111'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x4.base() == 0b1'01111111'1101'1011'0110'1101'1011'011);

        boost::ut::expect(x4.sign() == 1);
        boost::ut::expect(x4.exponent() == 0b0111'1111);
        boost::ut::expect(x4.mantissa() == 0b1101'1011'0110'1101'1011'011);
    };
};
#endif

} // flemu
#endif// FLEMU_FLOAT32_HPP
