#ifndef TB_LIFE_INDICATOR_ANIMATION_H
#define TB_LIFE_INDICATOR_ANIMATION_H

namespace tb
{
class LifeIndicatorAnimation
{
public:
    void reset(int chances = 3)
    {
        _clock_ms = 0;
        _last_chances = chances;
        _broken_slot = -1;
        _broken_start_ms = -1;
    }

    bool advance(int delta_ms, int chances)
    {
        const int previous_clock_ms = _clock_ms;
        const bool broken_before = _broken_active();
        const bool warning_before = _warning_phase(previous_clock_ms, _last_chances);

        _clock_ms += delta_ms;

        if(_broken_active() && _clock_ms - _broken_start_ms >= broken_duration_ms)
        {
            _broken_slot = -1;
            _broken_start_ms = -1;
        }

        const bool chances_changed = chances != _last_chances;
        if(chances < _last_chances)
        {
            const int slot = 2 - chances;
            if(slot >= 0 && slot < 3)
            {
                _broken_slot = slot;
                _broken_start_ms = _clock_ms;
            }
        }

        _last_chances = chances;
        const bool broken_after = _broken_active();
        const bool warning_after = _warning_phase(_clock_ms, chances);
        return chances_changed || broken_before != broken_after || warning_before != warning_after;
    }

    [[nodiscard]] int frame_for_slot(int slot, int chances, int active_frame) const
    {
        const int required_chances = 3 - slot;
        if(chances >= required_chances)
        {
            if(chances == 1 && _warning_phase(_clock_ms, chances))
            {
                return warning_frame;
            }
            return active_frame;
        }

        if(slot == _broken_slot && _broken_active())
        {
            return active_frame + 1;
        }
        return exhausted_frame;
    }

private:
    static constexpr int broken_duration_ms = 100;
    static constexpr int warning_period_ms = 500;
    static constexpr int exhausted_frame = 8;
    static constexpr int warning_frame = 9;

    [[nodiscard]] bool _broken_active() const
    {
        return _broken_slot >= 0 && _broken_start_ms >= 0 &&
               _clock_ms - _broken_start_ms < broken_duration_ms;
    }

    [[nodiscard]] static bool _warning_phase(int clock_ms, int chances)
    {
        return chances == 1 && ((clock_ms / warning_period_ms) % 2) == 0;
    }

    int _clock_ms = 0;
    int _last_chances = 3;
    int _broken_slot = -1;
    int _broken_start_ms = -1;
};
}

#endif
