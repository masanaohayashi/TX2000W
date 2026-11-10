// license:BSD-3-Clause
// S-MU2000: AUTO PAN 2。doc/fx-flow/rotary.md の AUTO PAN 2。
// 波の表を読んで音量と遅れを揺らす。表は遅延メモリに置く。

#ifndef S_MU2000_XG_FX_SETTINGS_AUTO_PAN2_H
#define S_MU2000_XG_FX_SETTINGS_AUTO_PAN2_H

namespace smu2000::fx_settings {

namespace auto_pan2 {

constexpr int triangle_curve = rom_at::auto_pan2_triangle_curve, sine_curve = rom_at::auto_pan2_sine_curve;	// 2 本ずつ置く
constexpr int wave_words = 1024 + 1, wave_spacing = 1024 + 4, wave_center = 512;
constexpr int phase_step_table = rom_at::auto_pan2_phase_step_table;
constexpr int switch_steepness_table = rom_at::auto_pan2_switch_steepness_table;	// LFO Wave → 切り替えの急さ
constexpr int output_level = 0x3FFF;
enum pan_direction { both_ways, left_to_right, right_to_left, left_turn, right_turn, switching };	// PAN Dir

// 揺れの形: 三角波を強く増幅して切り替えるか、サイン波をそのまま使うか
constexpr int sine_mix = 0x3fff, triangle_mix = 0x7fff;
// 切り替え: 三角波 a を (15a - 7) / 16 にし、a が 7/15 より小さい間は無音。左右が同時に鳴らない
constexpr int smooth_depth = 0x7fff, switch_wave_gain = 0x3C00, switch_wave_offset = 0xC800;
// 波に 2 回掛ける利得。なめらかな向きは 1 倍から、切り替えは 4/3 倍から。切り替えの表の 1 が 1/64 倍
constexpr int smooth_gain_base = 0x800, switch_gain_base = 0x8000 / 12, switch_gain_step = 32, smooth_gain_shift = 4;

// 2 組の波の位相のずれ
struct wave_phases { int first, second; };
inline wave_phases phases_for(int direction)
{
	switch (direction) {
	case left_to_right: return {0x7fff * 2 / 3, 0x7fff};
	case right_to_left: return {0x7fff, 0x7fff * 2 / 3};
	case left_turn: return {0x3fff, 0x7fff};
	case right_turn: return {0x7fff, 0x3fff};
	default: return {0x7fff, 0};
	}
}

} // namespace auto_pan2

inline bool settle(const effect_request &in, auto_pan2_params &p, effect_extras &x)
{
	using namespace auto_pan2;
	using namespace tables::detail;
	start_settle(in, p, x);
	if (in.kind != tables::effect_auto_pan2)
		return false;
	const u16 *P = in.P;
	const int lfo_frequency = P[0], left_right_depth = P[1], front_rear_depth = P[2], pan_direction = P[3], lfo_wave = P[4], stereo_input = P[14];

	const int memory = in.delay_memory_start();
	for (int w = 0; w < 4; w++) {
		const int start = memory + wave_spacing * w;
		copy_curve(in.rom, x, start, w < 2 ? triangle_curve : sine_curve, wave_words);
		p.wave_read_center[w] = start + wave_center;
	}

	const u32 step = read_u32(in.rom, phase_step_table + 4 * lfo_frequency);
	p.phase_increment_fine = float(double(scaled(int(step & 0x7fff))) / 32768.0);
	p.phase_increment = scaled(int(step >> 15));

	const auto phases = phases_for(pan_direction);
	for (int w = 0; w < 4; w++)
		p.wave_phase_offset[w] = scaled(w & 1 ? phases.second : phases.first);

	const bool switched = pan_direction >= switching;
	p.pan_gain = scaled(switched ? switch_gain_base + switch_gain_step * int(read_u16(in.rom, switch_steepness_table + 2 * lfo_wave)) : (lfo_wave << smooth_gain_shift) + smooth_gain_base, 16.0);
	if (!switched) {
		p.wave_mix_a = scaled(0);
		p.wave_mix_b = scaled(sine_mix);
	} else if (!in.insertion_part) {
		p.wave_mix_a = scaled(triangle_mix);
		p.wave_mix_b = scaled(0);
	} else if (in.pan_was_smooth) {	// インサーション 2-4 は前のなめらかな向きの混ざり方のまま
		p.wave_mix_a = scaled(0);
		p.wave_mix_b = scaled(sine_mix);
	}

	const int front_rear = pan_direction == left_turn || pan_direction == right_turn ? (front_rear_depth & 0xff) >> 2 : 0;	// 回るときだけ前後に揺らす
	const float am_depth = scaled(-((int(s8(u8(left_right_depth))) * 32768) / 127));
	const float out = scaled(output_gain(in, output_level), 4.0);
	for (int ch = 0; ch < 2; ch++) {
		p.pan_depth[ch] = scaled(switched ? switch_wave_gain : smooth_depth, 2.0);
		p.pan_center[ch] = scaled(switched ? switch_wave_offset : 0);
		p.voice_depth[ch] = modulation_depth(front_rear);
		p.voice_am_depth[ch] = am_depth;
		p.voice_base_delay[ch] = 1;
		p.out_gain[ch] = out;
	}
	set_mid_band(in.rom, p, P[10], P[11], P[12], range_4x);
	set_shelves(in.rom, p, P[5], P[6], P[7], P[8]);

	set_two_line_inputs(p, stereo_input);
	p.right_voice_on_second_line = stereo_input != 0;
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_AUTO_PAN2_H
