// license:BSD-3-Clause
// S-MU2000: MULTI COMP。doc/fx-flow/multi_comp.md。
// 3 帯に分けて帯ごとに圧縮する。Type が表の 1 行を選び、ほかのパラメーターはその行からのずれを足す。

#ifndef S_MU2000_XG_FX_SETTINGS_MULTI_COMP_H
#define S_MU2000_XG_FX_SETTINGS_MULTI_COMP_H

namespace smu2000::fx_settings {

namespace multi_comp {

constexpr int gain_curve = rom_at::multi_comp_table, gain_curve_words = 0x1000;		// 帯の利得の表。Ratio ごとに 512 語の曲線が 8 本
constexpr int soft_curve = rom_at::multi_comp_soft_curve, soft_curve_words = 4096 + 1;	// 柔らかい飽和の表
constexpr int soft_curve_center = 0x1800;	// 飽和の表の中心（遅延メモリの中）
constexpr int output_level = rom_at::multi_comp_output_level;
constexpr int band_gain_table = rom_at::multi_comp_band_gain_table;
constexpr int attack_speed_table = rom_at::multi_comp_attack_speed_table;
constexpr int release_speed_table = rom_at::multi_comp_release_speed_table;
constexpr int crossover_table = rom_at::multi_comp_crossover_table;			// 2 つの分割の周波数の番号
constexpr int crossover_speed_curve = rom_at::multi_comp_crossover_speed_curve;

// Type の行の並び: 帯ごとの 5 語のあとに分割の周波数
constexpr int type_row_bytes = 32, band_row_bytes = 10, crossover_in_row = 30;
constexpr int row_attack = 0, row_release = 2, row_threshold = 4, row_ratio_curve = 6, row_band_gain = 8;

constexpr int highpass_frequency = 0;
constexpr int detector_level = 0x1000;		// 音量検出の感度（+2 倍）
constexpr int threshold_lowest = 73, threshold_highest = 121;	// 値から 127 を引くと dB
constexpr int lowest_threshold_slope = 0x7fff >> 6;
constexpr int ratio_curve_center_shift = 9;
constexpr int band_gain_highest = 55;
constexpr double crossover_full_scale = 8.0, crossover_steps = 2048.0;
constexpr int offset_center = 64;		// パネルのずれの 0

inline int row_number(const effect_request &in, int row, int band, int field) { return int(tables::detail::read_u16(in.rom, row + band_row_bytes * band + field)); }
inline int row_offset(const effect_request &in, int row, int band, int field) { return tables::detail::read_s16(in.rom, row + band_row_bytes * band + field); }

// 利得の表の番号の目盛り: Threshold が低いほど音量を大きく読む。値 6 つで半分になる
inline int gain_read_slope(const effect_request &in, int threshold)
{
	if (threshold == threshold_lowest)
		return lowest_threshold_slope;
	return level_down(in, threshold - threshold_lowest - 1) >> 6;
}

inline float crossover_speed(const effect_request &in, int frequency)
{
	using namespace tables::detail;
	return scaled(int(tr(f32(f32(crossover_full_scale * read_float(in.rom, crossover_speed_curve + 4 * frequency)) * crossover_steps))));
}

} // namespace multi_comp

inline bool settle(const effect_request &in, multi_comp_params &p, effect_extras &x)
{
	using namespace multi_comp;
	using namespace tables::detail;
	start_settle(in, p, x);
	if (in.kind != tables::effect_multi_comp)
		return false;
	const int type = in.P[0], threshold_offset = in.P[1];
	const int band_gain_offset[3] = {in.P[2], in.P[3], in.P[4]};

	const int memory = in.delay_memory_start();
	copy_curve(in.rom, x, memory, gain_curve, gain_curve_words);
	copy_curve(in.rom, x, memory + gain_curve_words, soft_curve, soft_curve_words);

	const int row = rom_at::multi_comp_param1_table + type_row_bytes * (type & 0xff);
	for (int band = 0; band < 3; band++) {
		p.attack_speed[band] = scaled(read_s16(in.rom, attack_speed_table + 2 * row_number(in, row, band, row_attack)));
		p.release_speed[band] = scaled(read_s16(in.rom, release_speed_table + 2 * row_number(in, row, band, row_release)));
		const int ratio_curve_start = (row_number(in, row, band, row_ratio_curve) & 0xff) << ratio_curve_center_shift;
		p.gain_read_center[band] = float(double(scaled(ratio_curve_start)) * 32768.0 + memory);
		const int threshold = std::clamp(row_offset(in, row, band, row_threshold) + threshold_offset - offset_center, threshold_lowest, threshold_highest);
		p.gain_read_scale[band] = scaled(gain_read_slope(in, threshold), 32768.0);
		const int level = std::clamp(row_offset(in, row, band, row_band_gain) + band_gain_offset[band] - offset_center, 0, band_gain_highest);
		const float band_output = scaled(output_gain(in, read_s16(in.rom, band_gain_table + 2 * level) >> 1), 16.0);
		for (int ch = 0; ch < 2; ch++)
			p.band_output[ch][band] = band_output;
	}

	const int crossover = int(read_u16(in.rom, row + crossover_in_row));
	const float low_speed = crossover_speed(in, read_u8(in.rom, crossover_table + 2 * crossover));
	const float mid_speed = crossover_speed(in, read_u8(in.rom, crossover_table + 2 * crossover + 1));
	put(first_order_filter(in.rom, highpass_shape, range_2x, highpass_frequency), p.highpass_current, p.highpass_previous, p.highpass_feedback);
	const float out = scaled(int(tr(16384.0 * read_float(in.rom, output_level))), 4.0);
	const float detector = scaled(detector_level, 16.0);
	for (int ch = 0; ch < 2; ch++) {
		p.low_speed[ch] = low_speed;
		p.mid_speed[ch] = mid_speed;
		p.out_gain[ch] = out;
		p.detector_low[ch] = p.detector_mid[ch] = p.detector_high[ch] = detector;
		p.soft_read_center[ch] = memory + soft_curve_center;
	}
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_MULTI_COMP_H
