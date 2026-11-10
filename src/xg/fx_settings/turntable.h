// license:BSD-3-Clause
// S-MU2000: D.TURNTBL。doc/fx-flow/turntable.md、parameters.md の turntable。
// 入力に、レコードの雑音とクリックを足す。

#ifndef S_MU2000_XG_FX_SETTINGS_TURNTABLE_H
#define S_MU2000_XG_FX_SETTINGS_TURNTABLE_H

namespace smu2000::fx_settings {

namespace turntable {

constexpr int stored_table_read_center[2] = {45, 60};	// ROM の初期の値の中の位置

// 雑音を作る階段状の表
constexpr int table_words = 0x2000, table_center = 0x1000;
constexpr int table_near_center = 16;		// 中心のこの幅は階段にしない
constexpr int finest_stair_bits = 6;			// Noise Tone が 0 のときの階段の幅

inline void fill_noise_table(const effect_request &in, effect_extras &x, int noise_tone)
{
	const long long stair = 1LL << (noise_tone + finest_stair_bits);
	int held = 0;
	for (int i = 0; i < table_words; i++) {
		const int from_center = i - table_center;
		if (std::abs(from_center) < table_near_center || i % stair == 0)
			held = from_center * 8;
		write_curve_word(x, in.delay_memory_start() + i, held);
	}
}

// 種の遅れは固定
constexpr int seed_delay_samples[2] = {1, 3};
constexpr int click_b_longest_period = 16;

constexpr int seed_lfo = 1, click_lfo[2] = {3, 4};
constexpr int click_lfo_speed[2] = {16, 12};

constexpr int click_filter_frequency = 42, click_filter_gain = 69;

inline void set_clicks(const effect_request &in, turntable_params &p, effect_extras &x)
{
	const int density = in.P[0], click_level = in.P[1];
	p.click_a_increment = scaled(density + 2, 2.0);
	p.click_a_gain = scaled(0x4000, 2.0);
	p.click_b_increment = scaled(4, 2.0);
	p.click_b_gain = scaled(0x4000, 2.0);
	p.click_b_delay = (click_b_longest_period - density) & 0xffff;
	put(first_order_filter(in.rom, highpass_shape, range_2x, click_filter_frequency, click_filter_gain), p.click_filter_current, p.click_filter_previous, p.click_filter_feedback);
	for (int ch = 0; ch < 2; ch++) {
		p.click_am_center[ch] = scaled(0x1000);
		p.click_am_depth[ch] = scaled(0x7FFF);
		p.click_level[ch] = scaled(output_gain(in, click_level * 64), 4.0);
		u16 &lfo = x.lfo[click_lfo[ch]];
		set_lfo_waveform(lfo, lfo_sine);
		set_lfo_speed_range(lfo, lfo_eight_times_faster);
		set_lfo_speed(lfo, in.rom, click_lfo_speed[ch]);
	}
}

inline void set_noise(const effect_request &in, turntable_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int tone = P[2], mod_speed = P[3], mod_depth = P[4], dry_to_noise = P[5], lpf_frequency = P[6], lpf_q = P[7], noise_level = P[8];
	fill_noise_table(in, x, tone);
	set_lfo_speed(x.lfo[seed_lfo], in.rom, mod_speed);
	p.chaos_add = scaled(0x5B39, 16.0);
	p.chaos_gain = scaled(0x736F, 16.0);
	p.seed_am_center = scaled(0x7FFF - mod_depth * 10);
	p.seed_am_depth = scaled(mod_depth * 10);
	put(second_order_filter(in.rom, steep_lowpass_shape, range_2x, lpf_frequency, flat_gain, lpf_q), p.noise_lpf_current, p.noise_lpf_x1, p.noise_lpf_x2, p.noise_lpf_y1, p.noise_lpf_y2);
	for (int ch = 0; ch < 2; ch++) {
		p.seed_delay[ch] = seed_delay_samples[ch];
		p.noise_path_noise[ch] = scaled(128, 2.0);
		p.noise_path_current[ch] = p.noise_path_previous[ch] = scaled(dry_to_noise * 16, 2.0);
		p.table_read_center[ch] = float(in.stored_gain(stored_table_read_center[ch]) + in.delay_memory_start());
		p.table_gain[ch] = scaled(0x5000 - 0x800 * tone);
		p.noise_level[ch] = scaled(output_gain(in, noise_level * 129), 4.0);
	}
}

inline void set_dry(const effect_request &in, turntable_params &p)
{
	const int dry_level = in.P[10], lowpass_frequency = in.P[11];
	put(first_order_filter(in.rom, lowpass_shape, range_2x, lowpass_frequency), p.dry_lowpass_current, p.dry_lowpass_previous, p.dry_lowpass_feedback);
	p.dry_level[0] = p.dry_level[1] = scaled(dry_level * 129, 4.0);
}

} // namespace turntable

inline bool settle(const effect_request &in, turntable_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	if (in.kind != tables::effect_turntable)
		return false;
	turntable::set_clicks(in, p, x);
	turntable::set_noise(in, p, x);
	turntable::set_dry(in, p);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_TURNTABLE_H
