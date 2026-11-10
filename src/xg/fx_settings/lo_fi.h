// license:BSD-3-Clause
// S-MU2000: lo_fi 回路の設定。doc/fx-flow/parameters.md の lo_fi。
// 入力を間引いて保持し、1 サンプルに動ける幅を制限して追いかける。

#ifndef S_MU2000_XG_FX_SETTINGS_LO_FI_H
#define S_MU2000_XG_FX_SETTINGS_LO_FI_H

namespace smu2000::fx_settings {

namespace lo_fi {

constexpr int output_gain_table = rom_at::lo_fi_output_gain_table;		// OutputGain → 粗くしたあとの利得
constexpr int output_gain_6db_bytes = 12;				// 上の表は値 1 つで 1 dB。これだけ先で 2 倍
constexpr int filter_type_table = rom_at::lo_fi_filter_type_table;		// FltrType → 後ろのフィルタ 3 段の形
constexpr int filter_type_row_size = 10;
constexpr int input_full = 0x7fff;
// Bit Assign n で入力を 2^n 分の 1 にした分を戻す。16 倍までは後ろの利得、残りの 5、6 は上の表を先へ読む
constexpr int doubling_bit_assigns = 4, post_gain_unity = 0x800;
constexpr int emphasis_frequency = 44, emphasis_boost = flat_gain + 12, emphasis_cut = flat_gain - 12;

// WordLength → 1 サンプルに動ける幅。16 段ごとに傾きが倍になる
inline int step_of_word_length(int value)
{
	if (value < 48)
		return value + 15;
	if (value >= 127)
		return input_full;
	int step = 62, slope = 2, above = value - 47;
	while (above > 16) {
		step += 16 * slope;
		slope *= 2;
		above -= 16;
	}
	return step + above * slope;
}

inline void set_filters(const effect_request &in, lo_fi_params &p, int cutoff, int resonance, int filter_type, bool emphasis)
{
	constexpr int row_second = 3, row_third = 7;
	put(second_order_filter(in.rom, steep_lowpass_shape, range_2x, cutoff, flat_gain, resonance), p.pre_lowpass_current, p.pre_lowpass_x1, p.pre_lowpass_x2, p.pre_lowpass_y1, p.pre_lowpass_y2);

	const int row = filter_type_table + filter_type_row_size * filter_type;
	put(first_order_row(in.rom, range_2x, row), p.post_first_current, p.post_first_previous, p.post_first_feedback);
	put(second_order_row(in.rom, range_4x, row + row_second), p.post_second_current, p.post_second_x1, p.post_second_x2, p.post_second_y1, p.post_second_y2);
	put(first_order_row(in.rom, range_4x, row + row_third), p.post_third_current, p.post_third_previous, p.post_third_feedback);

	const int shape = emphasis ? high_shelf_shape : through_shape, frequency = emphasis ? emphasis_frequency : 0;
	put(first_order_filter(in.rom, shape, range_4x, frequency, emphasis ? emphasis_boost : flat_gain), p.pre_emphasis_current, p.pre_emphasis_previous, p.pre_emphasis_feedback);
	put(first_order_filter(in.rom, shape, range_4x, frequency, emphasis ? emphasis_cut : flat_gain), p.de_emphasis_current, p.de_emphasis_previous, p.de_emphasis_feedback);
}

} // namespace lo_fi

inline bool settle(const effect_request &in, lo_fi_params &p, effect_extras &x)
{
	using namespace lo_fi;
	using namespace tables::detail;
	start_settle(in, p, x);
	if (in.kind != tables::effect_lo_fi)
		return false;
	const u16 *P = in.P;
	const int sample_frequency = P[0], word_length = P[1], gain_value = P[2], cutoff = P[3], filter_type = P[4], resonance = P[5], bit_assign = P[6], input_mode = P[14];
	const bool stereo_input = input_mode == 1;

	p.counter_limit = scaled(sample_frequency);
	p.step_size = scaled(step_of_word_length(word_length));
	set_filters(in, p, cutoff, resonance, filter_type, P[7] == 1);

	int octaves_up = 0;
	if (bit_assign == 5)
		octaves_up = 1;
	else if (bit_assign == 6)
		octaves_up = 2;
	p.output_gain = scaled(int(read_u16(in.rom, output_gain_table + 2 * gain_value + octaves_up * output_gain_6db_bytes)), 16.0);
	p.post_stage_gain_2 = scaled(bit_assign < doubling_bit_assigns ? post_gain_unity << bit_assign : input_full, 16.0);

	const int input = int((long long)(input_full) >> bit_assign);
	p.input_a_from_left = scaled(stereo_input ? input : input >> 1);
	p.input_a_from_right = scaled(stereo_input ? 0 : input >> 1);
	p.input_b_from_right = scaled(input);

	const auto g = dry_wet(in, P[9]);
	p.left_from_input = p.right_from_input = scaled(g.dry, 4.0);
	p.left_from_a = scaled(g.wet, 4.0);
	p.right_from_b = scaled(stereo_input ? g.wet : 0, 4.0);
	p.right_from_a = scaled(stereo_input ? 0 : g.wet, 4.0);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_LO_FI_H
