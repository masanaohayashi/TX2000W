// license:BSD-3-Clause
// S-MU2000: SLICE。doc/fx-flow/slice.md、parameters.md の slice。
// テンポに合わせた刻みの波で音量を切り、左右にパンする。

#ifndef S_MU2000_XG_FX_SETTINGS_SLICE_H
#define S_MU2000_XG_FX_SETTINGS_SLICE_H

namespace smu2000::fx_settings {

namespace slice {

// 波の表は 4 本、513 語ずつ、遅延メモリの持ち分の先頭から等間隔に置く
constexpr int wave_words = 512 + 1, wave_spacing = 0x400, wave_center = 256;
constexpr int wave_bytes = 2 * wave_words;
enum wave_table { divide_wave, pan_wave, aeg_wave, length_wave };

// 波の 1 周は 1 小節。DivideType の音符ごとの方形波（16 分音符は 1 周に 16 個）
constexpr int divide_16th_notes = rom_at::slice_divide_16_pulses;
constexpr int divide_8th_notes = rom_at::slice_divide_8_pulses;
constexpr int divide_4th_notes = rom_at::slice_divide_4_pulses;
// 音符ごとの、Gate Time から長さの波の位相を引く表
constexpr int gate_phase_16th_notes = rom_at::slice_gate_phase_16_pulses;
constexpr int gate_phase_8th_notes = rom_at::slice_gate_phase_8_pulses;
constexpr int gate_phase_4th_notes = rom_at::slice_gate_phase_4_pulses;
constexpr int pan_type_waves = rom_at::slice_pan_type_waves;
constexpr int aeg_type_waves = rom_at::slice_aeg_type_waves;
constexpr int aeg_phase_table = rom_at::slice_aeg_phase_table;

constexpr int inverted_gate_time = 50;			// Gate Time はここから上が正の向き
constexpr int smooth_frequency = 34;
constexpr double tempo_to_phase_increment = float(2147483648.0 / (240 * 44100));	// 2^31 / (240 × 44100)。テンポから 4 拍で 1 周の増分へ
constexpr int unity_drive_stage = 0x870, drive_stage_step = (0x7FFF - unity_drive_stage) / 127;
constexpr double unity_drive_square = double(unity_drive_stage) * unity_drive_stage;	// Drive が 0 のときの 2 段の利得の積

inline int divide_curve(int divide_type)
{
	if (divide_type >= 5 && divide_type <= 7)
		return divide_16th_notes;
	if (divide_type >= 8 && divide_type <= 10)
		return divide_8th_notes;
	return divide_type == 11 ? divide_4th_notes : 0;
}

inline void copy_wave(const effect_request &in, effect_extras &x, wave_table wave, int curve)
{
	copy_curve(in.rom, x, in.delay_memory_start() + wave_spacing * wave, curve, wave_words);
}

inline void set_waves(const effect_request &in, slice_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int divide_type = P[0], aeg_type = P[2], pan_type = P[6];
	// DivideType の範囲の外は表が決まらないので、種類の初期の値の表を使う
	const int stored_divide_type = tables::detail::read_u16(in.rom, in.record_table().defaults + 32 * in.make);
	const int curve = divide_curve(divide_type) ? divide_curve(divide_type) : divide_curve(stored_divide_type);
	if (curve) {
		copy_wave(in, x, divide_wave, curve);
		copy_wave(in, x, length_wave, curve);
	}
	copy_wave(in, x, pan_wave, pan_type_waves + wave_bytes * pan_type);
	copy_wave(in, x, aeg_wave, aeg_type_waves + wave_bytes * aeg_type);
	for (int wave = 0; wave < 4; wave++)
		p.wave_read_center[wave] = in.delay_memory_start() + wave_spacing * wave + wave_center;
}

inline void set_gate(const effect_request &in, slice_params &p)
{
	using namespace tables::detail;
	const u16 *P = in.P;
	const int divide_type = P[0], gate_time = P[1], divide_level = P[5];
	const bool positive = gate_time >= inverted_gate_time;
	const int table = divide_type >= 11 ? gate_phase_4th_notes : divide_type >= 8 ? gate_phase_8th_notes : gate_phase_16th_notes;
	p.gate_phase = scaled(read_u16(in.rom, table + 2 * (positive ? gate_time - inverted_gate_time : gate_time)));
	p.gate_from_length = scaled(positive ? 0x7FFF : 0x8000);
	p.gate_floor = scaled(read_u16(in.rom, rom_at::dry_wet_curve + 2 * (divide_level + 1)));
	const int increment = int(tr(f32(double(fixed_tempo) * tempo_to_phase_increment)));
	p.phase_increment = scaled(increment & 0x7FFF, 1.0 / 32768.0);
}

inline void set_pan_and_aeg(const effect_request &in, slice_params &p)
{
	using namespace tables::detail;
	const u16 *P = in.P;
	const int aeg_level = P[3], pan_depth = P[4], aeg_phase = P[8];
	const int aeg_swing = aeg_level * panel_value_step;
	p.aeg_depth = scaled(0x7FFF - aeg_swing);
	p.aeg_center = scaled(aeg_swing);
	p.aeg_phase = scaled(read_u16(in.rom, aeg_phase_table + 2 * aeg_phase));
	const auto [same_side, other_side] = crossfade(in.rom, pan_depth);
	p.left_from_left = p.right_from_right = scaled(same_side >> 4, 16.0);
	p.left_from_right = p.right_from_left = scaled(other_side >> 4, 16.0);
	const first_order smooth = first_order_filter(in.rom, lowpass_shape, range_2x, smooth_frequency);
	put(smooth, p.aeg_smooth_current, p.aeg_smooth_previous, p.aeg_smooth_feedback);
	put(smooth, p.pan_smooth_current, p.pan_smooth_previous, p.pan_smooth_feedback);
}

// 歪みの 2 段の利得を Drive で上げ、出口で同じだけ下げる
inline void set_drive(const effect_request &in, slice_params &p)
{
	using namespace tables::detail;
	const int stage_gain = unity_drive_stage + in.P[7] * drive_stage_step;
	const double boost = f32(f32(double(stage_gain) * double(stage_gain)) / unity_drive_square);
	const int out = int(tr(f32(32767.0 / boost)));
	for (int ch = 0; ch < 2; ch++) {
		p.drive_first[ch] = p.drive_second[ch] = scaled(stage_gain, 16.0);
		p.drive_out[ch] = scaled(out);
	}
}

} // namespace slice

inline bool settle(const effect_request &in, slice_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	if (in.kind != tables::effect_slice)
		return false;
	slice::set_waves(in, p, x);
	slice::set_gate(in, p);
	slice::set_pan_and_aeg(in, p);
	slice::set_drive(in, p);
	set_first_order_shelves(in.rom, p, in.P[12], in.P[13], in.P[14], in.P[15], range_4x);
	set_dry_wet(in, p, in.P[9]);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_SLICE_H
