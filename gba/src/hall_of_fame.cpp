#include "tb/hall_of_fame.h"

namespace tb
{
namespace
{
bool allowed_name_char(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') || value == ' ' || value == '-' || value == '.';
}
void copy_default_name(std::array<char, hall_name_max_length + 1>& output)
{
    output.fill('\0');
    constexpr char default_name[] = "SUMEA";
    for(int index = 0; default_name[index] != '\0'; ++index) output[index] = default_name[index];
}
}
void normalize_hall_name(const char* input, std::array<char, hall_name_max_length + 1>& output)
{
    output.fill('\0');
    int output_index = 0;
    if(input)
    {
        for(int input_index = 0; input[input_index] != '\0' && output_index < hall_name_max_length; ++input_index)
        {
            char value = input[input_index];
            if(value >= 'a' && value <= 'z') value = char(value - 'a' + 'A');
            if(allowed_name_char(value)) output[output_index++] = value;
        }
    }
    if(output_index == 0) copy_default_name(output);
}
void reset_hall_of_fame(HallOfFameData& data)
{
    for(auto& table : data.tables) for(HallEntry& entry : table) { normalize_hall_name("SUMEA", entry.name); entry.score = 0; }
    normalize_hall_name("SUMEA", data.last_player_name);
    data.reserved.fill(0);
}
HallQualification qualify_hall_score(const HallOfFameData& data, HallTable table, uint32_t score)
{
    const auto& entries = data.tables[static_cast<int>(table)];
    if(score <= entries[hall_entries_per_table - 1].score) return {};
    for(int index = 0; index < hall_entries_per_table; ++index) if(score > entries[index].score) return {true, uint8_t(index + 1)};
    return {};
}
HallQualification insert_hall_score(HallOfFameData& data, HallTable table, uint32_t score, const char* name)
{
    const HallQualification qualification = qualify_hall_score(data, table, score);
    if(! qualification.qualifies) return qualification;
    auto& entries = data.tables[static_cast<int>(table)];
    const int insertion_index = int(qualification.position) - 1;
    for(int index = hall_entries_per_table - 1; index > insertion_index; --index) entries[index] = entries[index - 1];
    normalize_hall_name(name, entries[insertion_index].name);
    entries[insertion_index].score = score;
    return qualification;
}

bool build_city_player_registered(const HallOfFameData& data)
{
    const int position = int(data.reserved[0]);
    if(position < 1 || position > hall_entries_per_table)
    {
        return false;
    }
    return data.tables[static_cast<int>(HallTable::BuildCity)][position - 1].score > 0;
}

HallQualification insert_build_city_player_score(HallOfFameData& data, uint32_t score, const char* name)
{
    const HallQualification qualification = insert_hall_score(data, HallTable::BuildCity, score, name);
    data.reserved[0] = qualification.qualifies ? qualification.position : uint8_t(0);
    return qualification;
}

bool update_build_city_player_score(HallOfFameData& data, uint32_t score)
{
    if(! build_city_player_registered(data))
    {
        data.reserved[0] = 0;
        return false;
    }

    auto& entries = data.tables[static_cast<int>(HallTable::BuildCity)];
    const int player_index = int(data.reserved[0]) - 1;
    const HallEntry player = entries[player_index];
    if(score <= player.score)
    {
        return false;
    }

    for(int index = player_index; index < hall_entries_per_table - 1; ++index)
    {
        entries[index] = entries[index + 1];
    }
    normalize_hall_name("SUMEA", entries[hall_entries_per_table - 1].name);
    entries[hall_entries_per_table - 1].score = 0;
    data.reserved[0] = 0;

    const HallQualification qualification = insert_hall_score(data, HallTable::BuildCity, score, player.name.data());
    if(! qualification.qualifies)
    {
        return false;
    }
    data.reserved[0] = qualification.position;
    return true;
}

void forget_build_city_player(HallOfFameData& data)
{
    data.reserved[0] = 0;
}
}
