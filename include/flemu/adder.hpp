#ifndef FLEMU_ADDER_HPP
#define FLEMU_ADDER_HPP

#include "float32.hpp"

#include <boost/ut.hpp>

#include <random>
#include <tuple>

namespace flemu
{

inline float32 add(const float32& x_, const float32& y_) noexcept
{
    // ------------------------------------------------------------------------
    // always make x <= y
    const auto [x, y] = [&]
    {
        if(x_.exponent() == y_.exponent())
        {
            if(x_.mantissa() < y_.mantissa())
            {
                return std::make_pair(x_, y_);
            }
            else
            {
                return std::make_pair(y_, x_);
            }
        }
        else
        {
            if(x_.exponent() < y_.exponent())
            {
                return std::make_pair(x_, y_);
            }
            else
            {
                return std::make_pair(y_, x_);
            }
        }
    }();

    const std::uint32_t xsgn(x.sign());
    const std::uint32_t xexp(x.exponent());
    const std::uint32_t xman(x.mantissa());

    const std::uint32_t ysgn(y.sign());
    const std::uint32_t yexp(y.exponent());
    const std::uint32_t yman(y.mantissa());

    // ------------------------------------------------------------------------
    // check special values
    const auto xinf  = (xexp == 0b1111'1111) && (xman == 0);
    const auto yinf  = (yexp == 0b1111'1111) && (yman == 0);
    const auto xnan  = (xexp == 0b1111'1111) && (xman != 0);
    const auto ynan  = (yexp == 0b1111'1111) && (yman != 0);
    const auto xzero = (xexp == 0) && (xman == 0);
    const auto yzero = (yexp == 0) && (yman == 0);
    const auto xdenorm = (xexp == 0) && (xman != 0);
    const auto ydenorm = (yexp == 0) && (yman != 0);

    if(xnan || ynan) // z + nan == nan,  nan + z == nan
    {
        return float32(0b0, 0b1111'1111, 0b1);
    }
    else if (xinf || yinf)
    {
        if(xinf && yinf)
        {
            if(xsgn != ysgn) // inf - inf == nan, -inf + inf == nan
            {
                return float32(0b0, 0b1111'1111, 0b1);
            }
            else // inf + inf == inf, -inf + (-inf) = -inf
            {
                return float32(std::uint32_t(xsgn), 0b1111'1111, 0b0);
            }
        }
        else if((xinf && xsgn == 0) || (yinf && ysgn == 0)) // inf + * == inf
        {
            return float32(0b0, 0b1111'1111, 0b0);
        }
        else  // -inf + * == -inf
        {
            return float32(0b1, 0b1111'1111, 0b0);
        }
    }
    else if (xzero || yzero)
    {
        if(xzero && yzero)
        {
            // we here consider only nearest-(even)-rounding.
            // in case of negative-inf-rounding, (+0) + (-0) should be (-0).
            return float32(0);
        }
        else if (xzero)
        {
            return y;
        }
        else // yzero
        {
            return x;
        }
    }

    // ------------------------------------------------------------------------
    // align mantissa (always x.exp <= y.exp)

    const auto [xman_aligned, yman_aligned, xexp_norm, yexp_norm] = [&]
    {
        std::uint32_t xexp_norm = xexp;
        std::uint32_t yexp_norm = yexp;
        std::uint32_t xman_aligned = (1 << 23);
        std::uint32_t yman_aligned = (1 << 23);

        if(xdenorm)
        {
            xman_aligned = 0; // remove implicit 1
            xexp_norm   += 1;
        }
        if(ydenorm)
        {
            yman_aligned = 0;
            yexp_norm   += 1;
        }

        xman_aligned += std::uint32_t(xman);
        yman_aligned += std::uint32_t(yman);
        xman_aligned <<= 3;
        yman_aligned <<= 3;

        if(xexp_norm == yexp_norm)
        {
            return std::make_tuple(xman_aligned, yman_aligned, xexp_norm, yexp_norm);
        }
        // exponent is different
        const std::uint32_t expdiff = std::uint32_t(yexp_norm) - std::uint32_t(xexp_norm);

        //         mantissa      additional bits
        //    .---------------. .---.
        // y:| 1.xxxxxxxxxxxxxx|0|0|0|
        // x:     | 1.xxxxxxxxx|x|x|x|x|x|0|0|0| >> expdiff == e.g. 5
        //                      | | | '-------'
        //                      | | |  sticky region
        //                      | | + sticky bit
        //                      | + round bit
        //                      + guard bit

        if(expdiff >= 27) // 1 + 23 + 3
        {
            xman_aligned = 0;
        }
        else
        {
            std::uint32_t sticky = 0;
            if(expdiff > 3)
            {
                const auto sticky_region = xman_aligned & mask<std::uint32_t>(expdiff - 1, 0);
                sticky = (sticky_region == 0) ? 0 : 1;
            }
            xman_aligned >>= expdiff;
            xman_aligned |= sticky;
        }
        return std::make_tuple(xman_aligned, yman_aligned, xexp_norm, yexp_norm);
    }();

//     std::cerr << "xman_aligned = " << as_bit(xman_aligned) << std::endl;
//     std::cerr << "yman_aligned = " << as_bit(yman_aligned) << std::endl;

    // ------------------------------------------------------------------------
    // add/sub mantissa and round

    //           26 25       22 ...  03 02 01 00
    // y: | 0...| 1| z| z| z| z|... | z| 0| 0| 0|
    // x: | 0...| 0| 0| 0| 1| z|... | z| z| z| z|
    //             '-------------------' |  |  +- sticky
    //                   mantissa        |  +---- round
    //                                   +------- guard

    std::uint32_t zsgn(ysgn);
    std::uint32_t zexp(yexp_norm);
    if(xsgn != ysgn) // subtract. always abs(x) < abs(y), so the sign is y.
    {
        std::uint32_t zman = yman_aligned - xman_aligned;

        if(zman == 0)
        {
            // zero cannot be normalized
            return float32(zsgn, 0u, 0u);
        }

        while(bit_at(zman, 26) == 0)
        {
            zexp  -= 1;
            if(zexp == 0)
            {
                // since it becomes denormalized number, we don't need to
                // normalize it.
                break;
            }
            zman <<= 1;
        }

        if(zexp == 0)
        {
            // if it is 0.111...111, then it will be 1.00 after rounding and
            // will become normalized.
            if((zman & mask<std::uint32_t>(25, 2)) >> 2 == (1<<24)-1)
            {
                return float32(zsgn, std::uint32_t(1), std::uint32_t(0));
            }
        }

        // consider nearest-even rounding only
        if(bit_at(zman, 2) == 1)
        {
            if(bit_at(zman, 1) == 0 && bit_at(zman, 0) == 0) // to even
            {
                if(bit_at(zman, 3) == 0)
                {
                    // already even. do nothing.
                }
                else // its odd.
                {
                    zman += 0b1000;
                }
            }
            else // to nearest (upper)
            {
                zman += 0b1000;
            }
        }

        // check carry-up by rounding (1.11111 -> 10.0000)
        // 10.0000e+2 == 1.0000e+3
        if(bit_at(zman, 27) == 1)
        {
            zexp  += 1;
            zman >>= 1;
        }
        assert(bit_at(zman, 26) == 1 || zexp == 0); // normalized?

        if(zexp == 0b1111'1111)
        {
            // it was not nan, so here it should be inf
            zman = 0;
        }
        return float32(zsgn, zexp, std::uint32_t(bit_proxy(zman, 25, 3)));
    }
    else // add.
    {
        assert(bit_at(xman_aligned, 27) == 0);
        assert(bit_at(yman_aligned, 27) == 0);
        std::uint32_t zman = yman_aligned + xman_aligned;

        // check carry-up by addition
        if(bit_at(zman, 27) == 1)
        {
            // if we shift before checking round, the sticky bit will be lost.

            if(bit_at(zman, 3) == 1) // need to round up.
            {
                if(bit_at(zman, 2) == 0 && bit_at(zman, 1) == 0 && bit_at(zman, 0) == 0) // to even
                {
                    if(bit_at(zman, 4) == 0)
                    {
                        // already even. do nothing.
                    }
                    else
                    {
                        zman += 0b1'0000;
                    }
                }
                else // to nearest (upper)
                {
                    zman += 0b1'0000;
                }
            }
            if(bit_at(zman, 28) == 1)
            {
                zexp  += 2;
                zman >>= 2;
            }
            else if(bit_at(zman, 27) == 1)
            {
                zexp  += 1;
                zman >>= 1;
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            if(bit_at(zman, 2) == 1) // need to round up.
            {
                if(bit_at(zman, 1) == 0 && bit_at(zman, 0) == 0) // to even
                {
                    if(bit_at(zman, 3) == 0)
                    {
                        // already even. do nothing.
                    }
                    else
                    {
                        zman += 0b1000;
                    }
                }
                else // to nearest (upper)
                {
                    zman += 0b1000;
                }
            }
            // check carry-up by rounding.
            if(bit_at(zman, 27) == 1)
            {
                zexp  += 1;
                zman >>= 1;
            }
        }
        assert(bit_at(zman, 26) == 1); // normalized?

        if(zexp == 0b1111'1111)
        {
            // it was not nan, so here it should be inf
            zman = 0;
        }
        return float32(zsgn, zexp, std::uint32_t(bit_proxy(zman, 25, 3)));
    }
}

#ifdef FLEMU_ACTIVATE_UNIT_TESTS
inline boost::ut::suite tests_adder = []
{
    using namespace boost::ut::literals;

    "add(float32, float32)"_test = []
    {
        const auto x1 = to_flemu(1.0f);
        const auto y1 = to_flemu(1.0f);
        const auto z1 = add(x1, y1);

        boost::ut::expect(to_float(z1) == 2.0f);

//         std::cout << to_float(x1) << " + " << to_float(y1) << " = " << to_float(z1) << " != 2.0f"<< std::endl;
//         std::cout << z1.sign() << "|" << z1.exponent() << "|" << z1.mantissa() << std::endl;
//         std::cout << "========================================================================" << std::endl;

        const auto x2 = to_flemu( 1.0f);
        const auto y2 = to_flemu(10.0f);
        const auto z2 = add(x2, y2);

        boost::ut::expect(to_float(z2) == 11.0f);

//         std::cout << to_float(x2) << " + " << to_float(y2) << " = " << to_float(z2) << " != 11.0f"<< std::endl;
//         std::cout << z2.sign() << "|" << z2.exponent() << "|" << z2.mantissa() << std::endl;
//         std::cout << "========================================================================" << std::endl;

        const auto x3 = to_flemu(1.0e-30f);
        const auto y3 = to_flemu(1.0e+30f);
        const auto z3 = add(x3, y3);

        boost::ut::expect(to_float(z3) == 1.0e+30f);

//         std::cout << to_float(x3) << " + " << to_float(y3) << " = " << to_float(z3) << " != 1.0e+30f"<< std::endl;
//         std::cout << z3.sign() << "|" << z3.exponent() << "|" << z3.mantissa() << std::endl;
//         std::cout << "========================================================================" << std::endl;

        std::mt19937 rng(123456789);

        std::uniform_int_distribution<std::uint32_t> sgn(0,   1);
        std::uniform_int_distribution<std::uint32_t> exp(0, 255); // not including denormalized
        std::uniform_int_distribution<std::uint32_t> man(0, 0x007F'FFFF);

        const std::size_t N = 10000;
        for(std::size_t i=0; i<N; ++i)
        {
            const std::uint32_t xi = (sgn(rng) << 31) + (exp(rng) << 23) + man(rng);
            const std::uint32_t yi = (sgn(rng) << 31) + (exp(rng) << 23) + man(rng);
//             const std::uint32_t xi = (0 << 31) + (exp(rng) << 23) + man(rng);
//             const std::uint32_t yi = (0 << 31) + (exp(rng) << 23) + man(rng);
//             const std::uint32_t xi = 0b1101'1100'0000'1001'0010'1110'0100'0000;
//             const std::uint32_t yi = 0b1101'1010'0011'0100'0101'1100'0010'1001;
//             const std::uint32_t xi = 0b1100'1101'1111'1110'1111'1100'1100'1101;
//             const std::uint32_t yi = 0b1100'1011'0010'1101'1011'0101'0000'0100;

            boost::ut::log << "----------------------------------------------";
            boost::ut::log << "     sxxx'xxxx'xmmm'mmmm'mmmm'mmmm'mmmm'mmmm";
            boost::ut::log << "xr =" << as_bit(xi);
            boost::ut::log << "yr =" << as_bit(yi);

            const float xr = bit_cast<float>(xi);
            const float yr = bit_cast<float>(yi);
            const float zr = xr + yr;

            boost::ut::log << "zr =" << as_bit(bit_cast<std::uint32_t>(zr));

            const auto x = to_flemu(xr);
            const auto y = to_flemu(yr);
            const auto z = add(x, y);

            boost::ut::log << "z  =" << as_bit(z.base());

            if(z.is_nan())
            {
                boost::ut::expect(std::isnan(zr))
                    << "z = " << as_bit(z.base()) << " is NaN but "
                    << "zr = " << as_bit(bit_cast<std::uint32_t>(zr)) << " is not nan";
            }
            else
            {
                boost::ut::expect(to_float(z) == zr)
                    << as_bit(z.base()) << " != " << as_bit(bit_cast<std::uint32_t>(zr));
            }
        }
    };
};
#endif



} // flemu
#endif // FLEMU_ADDER_HPP
