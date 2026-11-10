// license:BSD-3-Clause
// S-MU2000: talk_mod 回路の設定。doc/fx-flow/parameters.md の talk_mod。
// 入力を歪ませて倍音を足し、母音の形の山 4 段に通す。

#ifndef S_MU2000_XG_FX_SETTINGS_TALK_MOD_H
#define S_MU2000_XG_FX_SETTINGS_TALK_MOD_H

namespace smu2000::fx_settings {

namespace talk_mod {

constexpr int vowel_table = rom_at::talk_mod_vowel_table;	// 母音ごとの山の値（4 段 × 周波数、帯域の混ぜ、減衰）
constexpr int formants = 4, values_per_formant = 3;
constexpr int fixed_filter_frequency = 44, highpass_width = 7;

// 0-127 の値を 0-1 倍へ
inline int level_of(int value) { using namespace tables::detail; return int(tr(f32(f32(double(value) * 16384.0) / 127.0))); }

inline void set_vowel(const effect_request &in, talk_mod_params &p, int vowel)
{
	using namespace tables::detail;
	const int row = vowel_table + 4 * formants * values_per_formant * vowel;
	for (int k = 0; k < formants; k++) {
		const auto vowel_value = [&](int value) { return scaled(int(tr(f32(32767.0 * read_float(in.rom, row + 4 * (values_per_formant * k + value)))))); };
		p.formant_frequency[k] = vowel_value(0);
		p.formant_band_out[k] = vowel_value(1);
		p.formant_damping[k] = vowel_value(2);
	}
}

} // namespace talk_mod

inline bool settle(const effect_request &in, talk_mod_params &p, effect_extras &x)
{
	using namespace talk_mod;
	start_settle(in, p, x);
	if (in.kind != tables::effect_talk_mod)
		return false;
	const u16 *P = in.P;
	const int vowel = P[0], drive = P[2], output_level = P[3];
	set_vowel(in, p, vowel);
	p.drive = scaled(level_of(drive), 2.0);
	for (float &gain : p.out_gain)
		gain = scaled(output_gain(in, level_of(output_level)), 4.0);
	// 入口の 2 次のハイパスと、歪みの道の 1 次のハイパス
	put(second_order_filter(in.rom, steep_highpass_shape, range_2x, fixed_filter_frequency, flat_gain, highpass_width), p.highpass_current, p.highpass_x1, p.highpass_x2, p.highpass_y1, p.highpass_y2);
	put(first_order_filter(in.rom, highpass_shape, range_2x, fixed_filter_frequency), p.shape_filter_current, p.shape_filter_previous, p.shape_filter_feedback);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_TALK_MOD_H
