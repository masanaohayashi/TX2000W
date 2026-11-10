// license:BSD-3-Clause
// S-MU2000: dyna 4 種類で同じ部品。doc/fx-flow/dyna.md。
// 入力の音量を追って制御信号を作る。入口の棚と音量の追従は 4 種類で同じ。

#ifndef S_MU2000_XG_FX_SETTINGS_DYNA_PARTS_H
#define S_MU2000_XG_FX_SETTINGS_DYNA_PARTS_H

namespace smu2000::fx_settings {

namespace dyna {

constexpr int level_curve = rom_at::dyna_parts_level_curve;		// 感度としきい値の値から大きさへ
constexpr int attack_time_table = rom_at::dyna_parts_attack_time_table;
constexpr int release_time_table = rom_at::dyna_parts_release_time_table;
constexpr double threshold_full_scale = 32767.0;
constexpr int level_offset_step = 129;		// LevelOfst 1 つぶん（最大で 1/2）
constexpr int feedback_step = 516;			// FB Level 1 つぶん（中央が 0）

// 感度: 表の値に、Direction ごとの目盛りを掛ける
inline float sensitivity(const effect_request &in, int curve, int value, double direction_scale)
{
	using namespace tables::detail;
	return scaled(int(tr(f32(direction_scale * read_float(in.rom, curve + 4 * value)))), 32.0);
}
inline int level_offset(int value) { return value * level_offset_step; }
inline int feedback_gain(int value) { return centered_feedback(value, feedback_step); }

// 整数部と 1/32768 の部分に分かれた速さを 1 つに
inline float split_speed(int whole, int fraction) { return float(double(scaled(whole)) + double(scaled(fraction)) / 32768.0); }

// 音量の追従
template<class Params>
inline void set_follower(const effect_request &in, Params &p, int attack_time, int release_time, int release_curve, int threshold_level)
{
	using namespace tables::detail;
	const u32 attack = read_u32(in.rom, attack_time_table + 4 * attack_time);
	const u32 release = read_u32(in.rom, release_time_table + 4 * release_time);
	p.attack_speed = split_speed(int(attack >> 16), int((attack & 0xffff) >> 1));
	p.release_speed = split_speed(int(release >> 15), int(release & 0x7fff));
	p.release_step = scaled(release_curve);
	p.threshold = scaled(int(tr(f32(threshold_full_scale * read_float(in.rom, level_curve + 4 * threshold_level)))));
}

// 制御信号の向き。音量が大きいと上がるか下がるか
template<class Params>
inline void set_direction(Params &p, bool rises_with_level)
{
	p.direction_up = scaled(rises_with_level ? 0x7fff : 0);
	p.direction_down = scaled(rises_with_level ? 0 : 0x7fff);
}

// 入口の棚（左右同じ）。パネルの位置は 4 種類で同じ
template<class Params>
inline void set_eq(const effect_request &in, Params &p) { set_first_order_shelves(in.rom, p, in.P[12], in.P[13], in.P[14], in.P[15], range_4x); }

} // namespace dyna

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_DYNA_PARTS_H
