#ifndef FLEMU_ADDER_HPP
#define FLEMU_ADDER_HPP

#include "float32.hpp"

#include <boost/ut.hpp>

#include <tuple>

namespace flemu
{

inline float32 add(const float32& x_, const float32& y_) noexcept
{
    // TODO: consider denormalized number

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
        else
        {
            return x;
        }
    }

    // ------------------------------------------------------------------------
    // align mantissa (always x.exp <= y.exp)

    const auto [xman_aligned, yman_aligned] = [&]
    {
        std::uint32_t xman_aligned = (1 << 23); // add implicit 1
        std::uint32_t yman_aligned = (1 << 23);
        xman_aligned += std::uint32_t(xman);
        yman_aligned += std::uint32_t(yman);
        xman_aligned <<= 3;
        yman_aligned <<= 3;

        if(xexp == yexp)
        {
            return std::make_tuple(xman_aligned, yman_aligned);
        }
        // exponent is different
        const std::uint32_t expdiff = std::uint32_t(yexp) - std::uint32_t(xexp);

        //         mantissa      additional bits
        //    .---------------. .---.
        // y:| 1.xxxxxxxxxxxxxx|0|0|0|
        // x:     | 1.xxxxxxxxx|x|x|x|x|x| >> expdiff == 5
        //                      | | '----'
        //                      | |  sticky region == 3 bit
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
                const auto sticky_region = mask<std::uint32_t>(expdiff - 2, 0);
                sticky = (sticky_region == 0) ? 0 : 1;
            }
            xman_aligned >>= expdiff;
            xman_aligned |= sticky;
        }
        return std::make_tuple(xman_aligned, yman_aligned);
    }();

    // ------------------------------------------------------------------------
    // add/sub mantissa and round

    const auto [zsgn, zexp, zman] = [&]
    {
        //              25       22 ...  03 02 01 00
        // y: | 0...| 1| z| z| z| z|... | z| 0| 0| 0|
        // x: | 0...| 0| 0| 0| 1| z|... | z| z| z| z|
        //             '-------------------' |  |  +- sticky
        //                   mantissa        |  +---- round
        //                                   +------- guard

        std::uint32_t sgn(ysgn);
        std::uint32_t exp(yexp);
        if(xsgn != ysgn) // subtract. always x < y, so the sign is y.
        {
            std::uint32_t res = yman_aligned - xman_aligned;

            if(bit_at(res, 26) == 0)
            {
                exp  -= 1;
                res <<= 1;
            }
            // consider nearest-even rounding only
            if(bit_at(res, 2) == 1)
            {
                if(bit_at(res, 1) == 0 && bit_at(res, 0) == 0) // to even
                {
                    if(bit_at(res, 3) == 0) // already even. do nothing.
                    {
                    }
                    else // its odd.
                    {
                        res += 0b1000;
                    }
                }
                else // to nearest (upper)
                {
                    res += 0b1000;
                }
            }

            // check carry-up by rounding (1.11111 -> 10.0000)
            // 10.0000e+2 == 1.0000e+3
            if(bit_at(res, 27) == 1)
            {
                exp  += 1;
                res >>= 1;
            }
            assert(bit_at(res, 26) == 1); // normalized?

            return std::make_tuple(sgn, exp, std::uint32_t(bit_proxy(res, 25, 3)));
        }
        else // add.
        {
            std::uint32_t res = yman_aligned + xman_aligned;
            // check carry-up by addition
            if(bit_at(res, 27) == 1)
            {
                exp  += 1;
                res >>= 1;
            }
            if(bit_at(res, 2) == 1) // need to round up.
            {
                if(bit_at(res, 1) == 0 && bit_at(res, 0) == 0) // to even
                {
                    if(bit_at(res, 3) == 0)
                    {
                        // already even. do nothing.
                    }
                    else
                    {
                        res += 0b1000;
                    }
                }
                else // to nearest (upper)
                {
                    res += 0b1000;
                }
            }

            // check carry-up by rounding.
            if(bit_at(res, 27) == 1)
            {
                exp  += 1;
                res >>= 1;
            }
            assert(bit_at(res, 26) == 1); // normalized?

            return std::make_tuple(sgn, exp, std::uint32_t(bit_proxy(res, 25, 3)));
        }
    }();

    return float32(zsgn, zexp, zman);
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
    };
};
#endif



} // flemu
#endif // FLEMU_ADDER_HPP
