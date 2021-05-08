// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "time.hpp"

#include <ctime>

std::string TimeData::time_str(
        const TimeType lowest,
        const bool add_separators) const
{
        std::string ret = std::to_string(year);

        const std::string month_str =
                (month < 10 ? "0" : "") + std::to_string(month);

        const std::string day_str =
                (day < 10 ? "0" : "") + std::to_string(day);

        const std::string hour_str =
                (hour < 10 ? "0" : "") + std::to_string(hour);

        const std::string minute_str =
                (minute < 10 ? "0" : "") + std::to_string(minute);

        const std::string second_str =
                (second < 10 ? "0" : "") + std::to_string(second);

        if (lowest >= TimeType::month)
        {
                ret += "-" + month_str;
        }

        if (lowest >= TimeType::day)
        {
                ret += "-" + day_str;
        }

        if (lowest >= TimeType::hour)
        {
                ret += (add_separators ? " " : "_") + hour_str;
        }

        if (lowest >= TimeType::minute)
        {
                ret += (add_separators ? ":" : "-") + minute_str;
        }

        if (lowest >= TimeType::second)
        {
                ret += (add_separators ? ":" : "-") + second_str;
        }

        return ret;
}

TimeData current_time()
{
        time_t t = time(nullptr);

        struct tm* now = localtime(&t);

        const TimeData d(
                now->tm_year + 1900,
                now->tm_mon + 1,
                now->tm_mday,
                now->tm_hour,
                now->tm_min,
                now->tm_sec);

        return d;
}
