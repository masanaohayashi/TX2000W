// license:BSD-3-Clause
// S-MU2000: ISOLATOR。doc/fx-flow/isolator.md、parameters.md の isolator。
// 低・中・高の 3 帯に分け、帯ごとの音量を変えて足す。分割の周波数は決まっている。

#ifndef S_MU2000_XG_FX_SETTINGS_ISOLATOR_H
#define S_MU2000_XG_FX_SETTINGS_ISOLATOR_H

namespace smu2000::fx_settings {

namespace isolator {

constexpr int low_split_frequency = 26, high_split_frequency = 45;
constexpr int split_width = 7;

constexpr int on_off_curve = full_at_center_curve;	// 値 0 と 127 だけを読み、0 か最大になる
constexpr int band_level_table = rom_at::isolator_band_level_table;
constexpr double low_band_trim = 0.2865, high_band_trim = 0.2942;

// 状態変数フィルタで帯を分ける部分の決まった値。G = tan(π × 周波数 / 44100) が積分の利得
// input_gain は 1 / (1 + √2 G + G²) に近い値、damping はほぼ 2√2（4 倍の目盛りで）。s1 への帰還が 2G + damping
// G は ROM の周波数の表の値そのもの
constexpr int split_damping = q15_floor(0.705), split_sign = 0x8000;
constexpr int low_split_input_gain = 0x7AF0, high_split_input_gain = 0x58E8;	// 400 Hz、3.6 kHz

inline void set_splits(const effect_request &in, isolator_params &p)
{
	const auto integrator = [&](int frequency) { return q15_floor(tables::detail::read_float(in.rom, rom_at::filter_frequency_table + 4 * frequency)); };
	const int low_integrator = integrator(low_split_frequency), high_integrator = integrator(high_split_frequency);
	for (int ch = 0; ch < 2; ch++) {
		p.low_input_gain[ch] = scaled(low_split_input_gain);
		p.low_fb_a[ch] = scaled(low_integrator, 2.0);
		p.low_stage1[ch] = p.low_stage2[ch] = scaled(low_integrator);
		p.low_fb_b[ch] = scaled(split_damping, 4.0);
		p.low_sign[ch] = scaled(split_sign);
		p.mid_input_gain[ch] = scaled(high_split_input_gain);
		p.mid_fb_a[ch] = scaled(high_integrator, 2.0);
		p.mid_stage1[ch] = p.mid_stage2[ch] = scaled(high_integrator);
		p.mid_fb_b[ch] = scaled(split_damping, 4.0);
		p.mid_sign[ch] = scaled(split_sign);
	}
}

// 帯を分けたあとの 2 次（左右同じ）
inline void set_band_filters(const effect_request &in, isolator_params &p)
{
	const auto steep = [&](int shape, int frequency) { return second_order_filter(in.rom, shape, range_2x, frequency, flat_gain, split_width); };
	put(steep(steep_highpass_shape, low_split_frequency), p.above_low_current, p.above_low_x1, p.above_low_x2, p.above_low_y1, p.above_low_y2);
	put(steep(steep_lowpass_shape, low_split_frequency), p.low_band_current, p.low_band_x1, p.low_band_x2, p.low_band_y1, p.low_band_y2);
	put(steep(steep_highpass_shape, high_split_frequency), p.high_band_current, p.high_band_x1, p.high_band_x2, p.high_band_y1, p.high_band_y2);
	// 中の帯は零点が 2 倍の値で 1 倍の目盛り
	const auto mid = tables::detail::filt2(in.rom, steep_lowpass_shape, range_2x, high_split_frequency, flat_gain, split_width);
	put(second_order{scaled(mid[0], 2.0), scaled(mid[1] * 2), scaled(mid[2] * 2), scaled(mid[3], 2.0), scaled(mid[4], 2.0)},
		p.mid_band_current, p.mid_band_x1, p.mid_band_x2, p.mid_band_y1, p.mid_band_y2);
}

inline void set_levels(const effect_request &in, isolator_params &p)
{
	using namespace tables::detail;
	const u16 *P = in.P;
	const int on = P[0] & 1, low_level = P[1], mid_level = P[2], high_level = P[3], low_mute = P[4], mid_mute = P[5], high_mute = P[6];
	const int dry = read_s16(in.rom, on_off_curve + 2 * 127 * (1 - on)) >> 4;
	const double wet = output_gain(in, read_s16(in.rom, on_off_curve + 2 * 127 * on) >> 1);
	const auto level = [&](int value) { return f32(wet * read_float(in.rom, band_level_table + 4 * value)); };
	const int low = low_mute ? 0 : int(tr(low_band_trim * level(low_level))) * 2;
	const int mid = mid_mute ? 0 : int(tr(level(mid_level))) * 2;
	const int high = high_mute ? 0 : int(tr(high_band_trim * level(high_level))) * 4;
	for (int ch = 0; ch < 2; ch++) {
		p.dry_gain[ch] = scaled(dry, 32.0);
		p.low_gain[ch] = scaled(low, 32.0);
		p.mid_gain[ch] = scaled(mid, 32.0);
		p.high_gain[ch] = scaled(high, 16.0);
	}
}

} // namespace isolator

inline bool settle(const effect_request &in, isolator_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	if (in.kind != tables::effect_isolator)
		return false;
	isolator::set_splits(in, p);
	isolator::set_band_filters(in, p);
	isolator::set_levels(in, p);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_ISOLATOR_H
