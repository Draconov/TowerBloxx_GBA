#ifndef TB_HALL_OF_FAME_H
#define TB_HALL_OF_FAME_H

#include <array>
#include <cstdint>

namespace tb
{
enum class HallTable : uint8_t { BuildCity = 0, QuickGame = 1 };
inline constexpr int hall_table_count = 2;
inline constexpr int hall_entries_per_table = 3;
inline constexpr int hall_name_max_length = 8;
struct HallEntry { std::array<char, hall_name_max_length + 1> name{}; uint32_t score = 0; };
struct HallOfFameData { std::array<std::array<HallEntry, hall_entries_per_table>, hall_table_count> tables{}; std::array<char, hall_name_max_length + 1> last_player_name{}; std::array<uint8_t, 3> reserved{}; };
struct HallQualification { bool qualifies = false; uint8_t position = 0; };
void reset_hall_of_fame(HallOfFameData& data);
HallQualification qualify_hall_score(const HallOfFameData& data, HallTable table, uint32_t score);
HallQualification insert_hall_score(HallOfFameData& data, HallTable table, uint32_t score, const char* name);
void normalize_hall_name(const char* input, std::array<char, hall_name_max_length + 1>& output);
}
#endif
