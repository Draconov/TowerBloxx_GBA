#ifndef TB_SKY_EVENT_POLICY_H
#define TB_SKY_EVENT_POLICY_H

#include <cstdint>

namespace tb
{
// Asset types from the original sky-event table. A planet or moon is a
// one-time encounter within a single construction session / Quick Game run.
[[nodiscard]] constexpr bool unique_celestial_event(int type)
{
    return type == 14 || type == 18 || type == 21 ||
            type == 24 || type == 25 || type == 26 || type == 27;
}

[[nodiscard]] constexpr uint32_t celestial_event_flag(int type)
{
    return unique_celestial_event(type) ? (uint32_t(1) << type) : uint32_t(0);
}
}

#endif
