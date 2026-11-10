// license:BSD-3-Clause
// S-MU2000: D.SCRATCH、LOW RESO。doc/fx-flow/low_reso.md、parameters.md の low_reso。
// 入力を粗くする表に通し、揺らした遅延を混ぜる。

#ifndef S_MU2000_XG_FX_SETTINGS_LOW_RESO_H
#define S_MU2000_XG_FX_SETTINGS_LOW_RESO_H

namespace smu2000::fx_settings {

namespace low_reso {

constexpr int stored_crush_read_center[2] = {45, 62};	// ROM の初期の値の中の位置

// --- 粗くする表 ----------------------------------------------------------
constexpr int curve_words = 0x2000, curve_center = 0x1000;
constexpr int curve_near_center = 16;		// 中心のこの幅は 1 語ずつ値を持つ
constexpr int curve_step = 32;				// それより外は刻みごとに値が変わり、間は前の値のまま
constexpr int scratch_soft_zone_span = 0x400;	// D.SCRATCH の、小さい音を絞る幅。その外は素通し
constexpr int scratch_soft_zone_near = rom_at::low_reso_scratch_soft_zone_near;	// 8 ビット、中心からの距離で引く
constexpr int scratch_soft_zone_steps = rom_at::low_reso_scratch_soft_zone_steps;	// 刻みごと
constexpr int resolution_curve_near = rom_at::low_reso_resolution_curve_near;	// 中心の近く
constexpr int resolution_curve_steps = rom_at::low_reso_resolution_curve_steps;	// 刻みごと
constexpr int resolution_curve_offset = 0x4000;
constexpr int resolution_curve_top = 0x1BC0, resolution_curve_top_value = 0x7FFF;	// ここから上は最大で頭打ち

// 中心の近くの小さい音を絞った、左右対称の素通し
inline void fill_scratch_curve(const effect_request &in, effect_extras &x)
{
	using namespace tables::detail;
	int held = 0;
	for (int i = 0; i < curve_words; i++) {
		const int from_center = i - curve_center, distance = std::abs(from_center);
		const int sign = from_center < 0 ? -1 : 1;
		if (distance <= curve_near_center)
			held = sign * s8(u8(read_u8(in.rom, scratch_soft_zone_near + distance)));
		else if (i % curve_step == 0)
			held = distance < scratch_soft_zone_span ? sign * read_s16(in.rom, scratch_soft_zone_steps + 2 * (distance / curve_step)) : from_center * 8;
		write_curve_word(x, in.delay_memory_start() + i, held);
	}
}

// Resoltn が 1 増えるごとに階段の幅が 2 倍になる
inline void fill_resolution_curve(const effect_request &in, effect_extras &x, int resolution)
{
	using namespace tables::detail;
	const long long stair = (1LL << resolution) * curve_step;
	int held = 0;
	for (int i = 0; i < curve_words; i++) {
		const int from_center = i - curve_center;
		if (std::abs(from_center) <= curve_near_center)
			held = read_s16(in.rom, resolution_curve_near + 2 * (from_center + curve_near_center)) + resolution_curve_offset;
		else if (i % stair == 0)
			held = i >= resolution_curve_top ? resolution_curve_top_value : read_s16(in.rom, resolution_curve_steps + 2 * (i / curve_step)) + resolution_curve_offset;
		write_curve_word(x, in.delay_memory_start() + i, held);
	}
}

// --- フィルタ ------------------------------------------------------------
constexpr int scratch_highpass_width = 9;
// LOW RESO では入口のフィルタの形も表のあとの EQ も決まっている
constexpr int resolution_input_frequency = 25, resolution_input_gain = 62, resolution_input_width = 10;
// 表のあとの 2 次の高域シェルフ（9 kHz）と、声の帯の 1 次のハイパス（400 Hz）。どちらも ROM の表から求める
constexpr int resolution_post_frequency = 53, resolution_voice_damp_frequency = 26, resolution_fixed_filter_gain = 70;

inline void set_input_filter(low_reso_params &p, const second_order &f) { put(f, p.low_shelf_current, p.low_shelf_x1, p.low_shelf_x2, p.low_shelf_y1, p.low_shelf_y2); }
inline void set_post_filter(low_reso_params &p, const second_order &f) { put(f, p.post_current, p.post_x1, p.post_x2, p.post_y1, p.post_y2); }

// --- 種類 ---------------------------------------------------------------
// 表は遅延メモリの持ち分の先頭に置く
inline void set_shared_parts(const effect_request &in, low_reso_params &p, int voice_delay)
{
	for (int ch = 0; ch < 2; ch++) {
		p.crush_read_center[ch] = float(in.stored_gain(stored_crush_read_center[ch]) + in.delay_memory_start());
		p.voice_base_delay[ch] = voice_delay & 0xffff;
	}
}

constexpr int scratch_lfo = 0, auto_pan_lfo = 2;

inline void dj_scratch(const effect_request &in, low_reso_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int input_level = P[0], init_delay = P[1], scratch_speed = P[2], scratch_depth = P[3], auto_pan_speed = P[4], auto_pan_depth = P[5];
	const int eq_frequency = P[6], eq_gain = P[7], eq_width = P[8], dry_wet_value = P[9], highpass_cutoff = P[10];
	fill_scratch_curve(in, x);
	set_lfo_waveform(x.lfo[scratch_lfo], lfo_sine);
	set_lfo_speed_range(x.lfo[scratch_lfo], lfo_eight_times_faster);
	set_lfo_waveform(x.lfo[auto_pan_lfo], lfo_sine);
	set_lfo_speed(x.lfo[scratch_lfo], in.rom, scratch_speed);
	set_lfo_speed(x.lfo[auto_pan_lfo], in.rom, auto_pan_speed);
	const int pan_swing = auto_pan_depth * panel_value_step;
	p.am_center[0] = scaled(0x7FFF - pan_swing);	// 左右の音量を逆向きに揺らす
	p.am_center[1] = scaled(0x7FFF);
	p.am_depth[0] = scaled(pan_swing);
	p.am_depth[1] = scaled(-pan_swing);
	for (int ch = 0; ch < 2; ch++) {
		p.line_from_crushed[ch] = scaled(0x4000, 2.0);
		p.mix_from_crushed[ch] = scaled(0, 2.0);
		p.mix_from_voice[ch] = scaled(0x6000, 2.0);
		p.crush_drive[ch] = scaled(input_level * 225, 16.0);
		p.crush_gain[ch] = scaled(0x15E0 - 32 * input_level, 2.0);
		p.voice_depth[ch] = scaled(scratch_depth * 96, 32768.0);
	}
	set_post_filter(p, second_order_filter(in.rom, peaking_shape, range_2x, eq_frequency, eq_gain, eq_width));
	set_input_filter(p, second_order_filter(in.rom, steep_highpass_shape, range_2x, highpass_cutoff, flat_gain, scratch_highpass_width));
	set_dry_wet(in, p, dry_wet_value);
	set_shared_parts(in, p, tenths_to_samples(init_delay));
}

inline void low_resolution(const effect_request &in, low_reso_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int mod_depth = P[0], mod_delay_offset = P[1], mod_feedback = P[2], resolution = P[3], mod_mix = P[4], phase_invert = P[5], dry_wet_value = P[9];
	fill_resolution_curve(in, x, resolution);
	set_input_filter(p, second_order_filter(in.rom, steep_low_shelf_shape, range_2x, resolution_input_frequency, resolution_input_gain, resolution_input_width));
	set_post_filter(p, second_order_filter(in.rom, steep_high_shelf_shape, range_2x, resolution_post_frequency, resolution_fixed_filter_gain));
	put(first_order_filter(in.rom, highpass_shape, range_2x, resolution_voice_damp_frequency, resolution_fixed_filter_gain), p.voice_damp_current, p.voice_damp_previous, p.voice_damp_feedback);
	const auto [from_crushed, from_voice] = crossfade(in.rom, mod_mix);
	for (int ch = 0; ch < 2; ch++) {
		p.crushed_gain[ch] = scaled(0x2000, 2.0);
		p.crush_drive[ch] = scaled(0x2000, 16.0);
		p.voice_depth[ch] = scaled(mod_depth * 2, 32768.0);
		p.line_feedback[ch] = scaled(centered_feedback(mod_feedback, 129), 2.0);
		p.crush_gain[ch] = scaled(0x4000 - 0x700 * resolution, 2.0);
		p.mix_from_crushed[ch] = scaled(from_crushed >> 2, 2.0);
		p.mix_from_voice[ch] = scaled(from_voice >> 2, 2.0);
	}
	// PhasInv: 0 は反転なし、1 は右の濡れた音、2 は右の乾いた音と濡れた音を反転する
	const auto g = dry_wet(in, dry_wet_value);
	p.dry_gain[0] = scaled(g.dry, 4.0);
	p.dry_gain[1] = scaled(phase_invert == 2 ? -g.dry : g.dry, 4.0);
	p.wet_gain[0] = scaled(g.wet, 4.0);
	p.wet_gain[1] = scaled(phase_invert == 0 ? g.wet : -g.wet, 4.0);
	set_shared_parts(in, p, mod_delay_offset + 1);
}

} // namespace low_reso

inline bool settle(const effect_request &in, low_reso_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_dj_scratch: low_reso::dj_scratch(in, p, x); return true;
	case tables::effect_low_reso: low_reso::low_resolution(in, p, x); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_LOW_RESO_H
