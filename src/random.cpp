// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "random.hpp"

#include <cmath>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iterator>
#include <numeric>
#include <sstream>

#include "debug.hpp"

int Range::roll() const
{
        return rnd::range(min, max);
}

bool Fraction::roll() const
{
        return rnd::fraction(num, den);
}

std::string Range::str() const
{
        const int min_actual = std::min(min, max);
        const int max_actual = std::max(min, max);

        if (min_actual == max_actual) {
                return std::to_string(min_actual);
        }
        else {
                return (
                        std::to_string(min_actual) +
                        "-" +
                        std::to_string(max_actual));
        }
}

std::string Range::str_avg() const
{
        const auto val = avg();

        auto rounded = round(val * 100.0) / 100.0;

        std::ostringstream ss;

        ss << std::fixed << std::setprecision(1) << rounded;

        return ss.str();
}

namespace rnd
{
std::mt19937 g_rng;

void seed()
{
        auto t = static_cast<uint32_t>(time(nullptr));

        std::hash<uint32_t> hasher;

        size_t hashed = hasher(t);

        seed(static_cast<uint32_t>(hashed));
}

void seed(uint32_t seed)
{
        g_rng.seed(static_cast<std::mt19937::result_type>(seed));
}

int range(const int v1, const int v2)
{
        const int min = std::min(v1, v2);
        const int max = std::max(v1, v2);

        std::uniform_int_distribution<int> dist(min, max);

        return dist(g_rng);
}

int range_binom(const int v1, const int v2, const double p)
{
        const int min = std::min(v1, v2);
        const int max = std::max(v1, v2);

        const int upper_random_value = max - min;

        std::binomial_distribution<std::mt19937::result_type>
                dist(upper_random_value, p);

        const int random_value = (int)dist(g_rng);

        return min + random_value;
}

bool coin_toss()
{
        return range(1, 2) == 2;
}

bool fraction(const int num, const int den)
{
        // Debug mode checks

        // This function should never be called with a denominator less than
        // one, since it's unclear what e.g. "N times in -1" would mean.
        ASSERT(den >= 1);

        // If the numerator is bigger than the denominator, it's likely a bug
        // (should something occur e.g. 5 times in 3 ???) - don't allow this...
        ASSERT(num <= den);

        // A negative numerator is of course nonsense
        ASSERT(num >= 0);

        // If any of the rules above are broken on a release build, we try to
        // perform the action that was probably intended.

        // NOTE: A numerator of 0 is allowed (it simply means "no chance")

        if ((num <= 0) || (den <= 0)) {
                return false;
        }

        if ((num >= den) || (den == 1)) {
                return true;
        }

        return range(1, den) <= num;
}

bool one_in(const int N)
{
        return fraction(1, N);
}

bool percent(const int pct_chance)
{
        return pct_chance >= range(1, 100);
}

int weighted_choice(const std::vector<int>& weights)
{
        ASSERT(!weights.empty());

#ifndef NDEBUG
        for (const int weight : weights) {
                ASSERT(weight > 0);
        }
#endif  // NDEBUG

        const int sum =
                std::accumulate(
                        std::begin(weights),
                        std::end(weights),
                        0);

        int rnd = rnd::range(0, sum - 1);

        for (size_t i = 0; i < weights.size(); ++i) {
                const int weight = weights[i];

                if (rnd < weight) {
                        return (int)i;
                }

                rnd -= weight;
        }

        // This point should never be reached
        ASSERT(false);

        return 0;
}

}  // namespace rnd
