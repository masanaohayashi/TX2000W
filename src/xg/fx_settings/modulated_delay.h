// license:BSD-3-Clause
// S-MU2000: CHORUS、CELESTE、FLANGER、SYMPHONIC。doc/fx-flow/parameters.md の modulated_delay。
// 遅延線は 2 本あり、声はそれぞれ近い方の線を読む。

#ifndef S_MU2000_XG_FX_SETTINGS_MODULATED_DELAY_H
#define S_MU2000_XG_FX_SETTINGS_MODULATED_DELAY_H

namespace smu2000::fx_settings {

namespace modulated_delay {

constexpr int stored_first_line_write = 20, stored_second_line_write = 21;	// ROM の初期の値の中の位置
constexpr int stored_voice_read[4] = {2, 4, 6, 10};
constexpr int tempo_flanger_make = 34;
constexpr int second_voice_spacing = 64;		// 同じ線の 2 つ目の声はこれだけ遅れる

inline void set_shared_parts(const effect_request &in, modulated_delay_params &p, int shelf_first, int dry_wet_value)
{
	const u16 *P = in.P;
	set_shelves(in.rom, p, P[shelf_first], P[shelf_first + 1], P[shelf_first + 2], P[shelf_first + 3]);
	set_mid_band(in.rom, p, P[10], P[11], P[12]);
	set_dry_wet(in, p, dry_wet_value);
}

inline void chorus_celeste(const effect_request &in, modulated_delay_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1], feedback_level = P[2], delay_offset = P[3], stereo_input = P[14];
	set_lfo_speeds(x.lfo, 3, in.rom, lfo_frequency);
	const float depth = modulation_depth((lfo_depth & 0xff) << 1);
	for (float &d : p.voice_slow_depth)
		d = depth;
	p.voice_slow_depth[3] = depth;
	p.first_line_from_left_feedback = p.second_line_from_right_feedback = scaled(centered_feedback(feedback_level, 0x178));
	const int first_write = in.stored_read_position(stored_first_line_write);
	const int second_write = stereo_input ? half_memory : 0;
	const int second_base = stereo_input ? half_memory : first_write;
	set_first_line_input(p.first_line_from_left, p.first_line_from_right, stereo_input);
	const int delay = chorus_delay_samples(in.rom, delay_offset);
	set_voice(p, 0, delay + first_write + 1, first_write, second_write);
	set_voice(p, 1, delay + first_write + 1 + second_voice_spacing, first_write, second_write);
	set_voice(p, 3, delay + second_base + 1, first_write, second_write);
	set_voice(p, 2, delay + second_base + 1 + second_voice_spacing, first_write, second_write);
	set_shared_parts(in, p, 5, P[9]);
}

inline void flanger(const effect_request &in, modulated_delay_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1], feedback_level = P[2], delay_offset = P[3], phase_difference = P[13];
	if (in.make == tempo_flanger_make) {
		const int speed = tempo_lfo_speed(in.rom, lfo_frequency);
		x.lfo[0] = u16((x.lfo[0] & 0x0f00) | 0x4000 | speed);
		set_lfo_speed_value(x.lfo[1], speed);
		int phase = s16(u16(phase_difference - 34));	// テンポ同期の位相差は音符の刻みで、半周で折り返す
		if (phase >= 60)
			phase -= 120;
		set_lfo_start_phase(x.lfo[1], in.rom, phase + 64);
	} else {
		set_lfo_speed(x.lfo[0], in.rom, lfo_frequency);
		set_lfo_speed(x.lfo[1], in.rom, lfo_frequency);
		set_lfo_start_phase(x.lfo[1], in.rom, phase_difference);
	}
	p.voice_slow_depth[0] = p.voice_slow_depth[3] = modulation_depth(lfo_depth);
	p.first_line_from_left_feedback = p.second_line_from_right_feedback = scaled(centered_feedback(feedback_level));
	const int first_write = in.stored_read_position(stored_first_line_write);
	const int delay = chorus_delay_samples(in.rom, delay_offset);
	set_voice(p, 0, first_write + delay + 1, first_write, half_memory);
	set_voice(p, 1, in.stored_read_position(stored_voice_read[1]), first_write, half_memory);
	set_voice(p, 2, in.stored_read_position(stored_voice_read[2]), first_write, half_memory);
	set_voice(p, 3, half_memory + delay + 1, first_write, half_memory);
	set_shared_parts(in, p, 5, P[9]);
}

inline void symphonic(const effect_request &in, modulated_delay_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int lfo_frequency = P[0], lfo_depth = P[1], delay_offset = P[2];
	set_lfo_speeds(x.lfo, 6, in.rom, lfo_frequency);
	const float slow = modulation_depth((lfo_depth * 5) >> 3), fast = modulation_depth((lfo_depth * 3) >> 3);
	for (int v = 0; v < 4; v++) {
		p.voice_slow_depth[v] = slow;
		p.voice_fast_depth[v] = fast;
	}
	const int first_write = in.stored_read_position(stored_first_line_write), second_write = in.stored_read_position(stored_second_line_write);
	const int read = chorus_delay_samples(in.rom, delay_offset) + first_write;
	set_voice(p, 0, read + 2, first_write, second_write);
	set_voice(p, 1, read + 2 + second_voice_spacing, first_write, second_write);
	set_voice(p, 2, in.stored_read_position(stored_voice_read[2]), first_write, second_write);
	set_voice(p, 3, read + 0x82, first_write, second_write);
	set_shared_parts(in, p, 5, P[9]);
}

// --- ディレイ（揺らさない）------------------------------------------------
constexpr int level_curve = rom_at::drive_parts_output_level_table;		// センターなどの声の大きさの曲線
constexpr int tempo_echo_make = 19;
constexpr int memory_size = 0x10000;
constexpr int high_damp_scale = 16384;

inline int stored_first_write(const effect_request &in) { return in.stored_read_position(stored_first_line_write); }
inline int stored_second_write(const effect_request &in) { return in.stored_read_position(stored_second_line_write); }
inline float voice_level(const effect_request &in, int value) { return scaled(int(tables::detail::tr(tables::detail::read_float(in.rom, level_curve + 4 * value) * 32767.0))); }

// High Damp: 今の声と 1 つ前の声を混ぜる 1 次の高域減衰（帰還フィルタ filters 本に同じ値）
inline void set_high_damp(modulated_delay_params &p, int filters, int value)
{
	using namespace tables::detail;
	const double now = f32((double(value) + 8.0) / 18.0);
	const int current = int(tr(qz(high_damp_scale, now))), previous = int(tr(qz(high_damp_scale, f32(1.0 - now))));
	for (int ch = 0; ch < filters; ch++) {
		p.feedback_filter_current[ch] = scaled(current, 2.0);
		p.feedback_filter_previous[ch] = scaled(previous, 2.0);
	}
}

// ディレイ共通の Dry/Wet と P13-16 の棚
inline void set_delay_mix_and_shelves(const effect_request &in, modulated_delay_params &p)
{
	const u16 *P = in.P;
	set_shelves(in.rom, p, P[12], P[13], P[14], P[15]);
	set_dry_wet(in, p, P[9], full_at_center_curve);
}

// 線 1 本を 4 つの声で読む種類（添え字は声の番号）
inline void set_single_line_voices(const effect_request &in, modulated_delay_params &p, const int (&delay)[4])
{
	const int first_write = stored_first_write(in), second_write = stored_second_write(in);
	for (int voice = 0; voice < 4; voice++)
		set_voice(p, voice, first_write + tenths_to_samples(delay[voice]), first_write, second_write);
}

inline void delay_lcr(const effect_request &in, modulated_delay_params &p)
{
	const u16 *P = in.P;
	const int left_delay = P[0], right_delay = P[1], center_delay = P[2], feedback_delay = P[3], feedback_level = P[4], center_level = P[5], high_damp = P[6];
	set_single_line_voices(in, p, {feedback_delay, left_delay, center_delay, right_delay});
	p.first_line_from_left_feedback = scaled(centered_feedback(feedback_level));
	p.wet_left[2] = p.wet_right[2] = voice_level(in, center_level);
	set_high_damp(p, 1, high_damp);
	set_delay_mix_and_shelves(in, p);
}

inline void delay_lr(const effect_request &in, modulated_delay_params &p)
{
	const u16 *P = in.P;
	const int left_delay = P[0], right_delay = P[1], first_feedback_delay = P[2], second_feedback_delay = P[3], feedback_level = P[4], high_damp = P[5];
	set_single_line_voices(in, p, {first_feedback_delay, left_delay, right_delay, second_feedback_delay});
	p.first_line_from_left_feedback = p.first_line_from_right_feedback = scaled(centered_feedback(feedback_level) >> 1);
	set_high_damp(p, 2, high_damp);
	set_delay_mix_and_shelves(in, p);
}

inline void echo(const effect_request &in, modulated_delay_params &p)
{
	const u16 *P = in.P;
	const int left_delay = P[0], left_feedback_level = P[1], right_delay = P[2], right_feedback_level = P[3], high_damp = P[4], left_delay2 = P[5], right_delay2 = P[6], delay2_level = P[7];
	const int first_write = stored_first_write(in);
	set_voice(p, 0, first_write + tenths_to_samples(left_delay), first_write, half_memory);
	set_voice(p, 1, first_write + tenths_to_samples(left_delay2), first_write, half_memory);
	set_voice(p, 3, half_memory + tenths_to_samples(right_delay), first_write, half_memory);
	set_voice(p, 2, half_memory + tenths_to_samples(right_delay2), first_write, half_memory);
	p.first_line_from_left_feedback = scaled(centered_feedback(left_feedback_level));
	p.second_line_from_right_feedback = scaled(centered_feedback(right_feedback_level));
	p.wet_left[1] = p.wet_right[2] = voice_level(in, delay2_level);
	set_high_damp(p, 2, high_damp);
	set_delay_mix_and_shelves(in, p);
}

// クロスディレイの線: InputSelect が L か R なら左右を混ぜて 1 本目の線へ入れる。L&R は右を 2 本目へ入れる
// R は 2 本の線の役割と帰還の戻し先を入れ替え、最初のやまびこが右でなく左から鳴る
constexpr int right_input_select = 1;
struct cross_lines { int first_write, second_write; };
inline cross_lines cross_lines_for(int input_select)
{
	return input_select == right_input_select ? cross_lines{half_memory, first_line_write} : cross_lines{first_line_write, half_memory};
}
inline cross_lines set_cross_routing(const effect_request &in, modulated_delay_params &p, int input_select, int feedback_level)
{
	const bool swapped = input_select == right_input_select;
	const float feedback = scaled(centered_feedback(feedback_level));
	set_two_line_inputs(p, input_select >= 2);
	p.first_line_from_left_feedback = p.second_line_from_right_feedback = swapped ? 0.0f : feedback;
	p.first_line_from_right_feedback = p.second_line_from_left_feedback = swapped ? feedback : 0.0f;
	const cross_lines lines = cross_lines_for(input_select);
	// 声 1、2 は Wet が 0 で帰還にも使わず、音に出ない。読む位置は初期のまま
	for (int voice : {1, 2})
		set_voice(p, voice, in.stored_read_position(stored_voice_read[voice]), lines.first_write, lines.second_write);
	return lines;
}

inline void cross_delay(const effect_request &in, modulated_delay_params &p)
{
	const u16 *P = in.P;
	const int left_to_right_delay = P[0], right_to_left_delay = P[1], feedback_level = P[2], input_select = P[3], high_damp = P[4];
	const cross_lines lines = set_cross_routing(in, p, input_select, feedback_level);
	set_voice(p, 0, half_memory + tenths_to_samples(right_to_left_delay), lines.first_write, lines.second_write);
	set_voice(p, 3, first_line_write + tenths_to_samples(left_to_right_delay), lines.first_write, lines.second_write);
	set_high_damp(p, 2, high_damp);
	set_delay_mix_and_shelves(in, p);
}

// --- テンポ同期のディレイ ---------------------------------------------------
constexpr double tenths_of_ms_per_minute = 600000.0, note_table_units_per_quarter = 24.0;	// 音符の表は 4 分音符が 24
constexpr u32 samples_per_tenth_of_ms_q12 = 44100 * 4096 / 10000;	// 4.41 サンプル（下位 12 ビットが端数）

// 音符の長さ（0.1ms）。曲のテンポは 120 に固定
inline long long note_tenths_of_ms(const effect_request &in, int note)
{
	using namespace tables::detail;
	const double tenths_per_beat = f32(tenths_of_ms_per_minute / f32(double(fixed_tempo)));
	return tr((read_float(in.rom, rom_at::tempo_lfo_table + 4 * note) / note_table_units_per_quarter) * tenths_per_beat);
}
inline int centered_tenths_of_ms(int value) { return (value - 64) * 10; }
// 長さが 0 以下の声は 1 サンプル遅れで読む
inline int tempo_voice_samples(long long tenths_of_ms) { return tenths_of_ms > 0 ? int((u32(tenths_of_ms) * samples_per_tenth_of_ms_q12) >> 12) : 1; }

inline void tempo_delay(const effect_request &in, modulated_delay_params &p)
{
	const u16 *P = in.P;
	const int note = P[0], feedback_level = P[1], high_damp = P[2], left_right_difference = centered_tenths_of_ms(P[3]), lag = centered_tenths_of_ms(P[4]);
	const bool two_lines = in.make == tempo_echo_make;	// T.ECHO は線 2 本
	const int first_write = stored_first_write(in), second_write = two_lines ? half_memory : stored_second_write(in);
	const int right_line = two_lines ? half_memory : first_write;
	const int left_limit = two_lines ? memory_size >> 1 : memory_size;
	// 線に収まらなければ音符の長さを半分にしていく
	int left_feedback_read, left_read, right_feedback_read, right_read;
	for (long long length = note_tenths_of_ms(in, note);; length /= 2) {
		left_feedback_read = first_write + tempo_voice_samples(lag + length);
		left_read = first_write + tempo_voice_samples(left_right_difference + lag + length);
		right_feedback_read = right_line + tempo_voice_samples(lag + length);
		right_read = right_line + tempo_voice_samples(lag - left_right_difference + length);
		if (left_read < left_limit && right_read < memory_size && right_feedback_read < memory_size)
			break;
	}
	set_voice(p, 0, left_feedback_read, first_write, second_write);
	set_voice(p, 1, left_read, first_write, second_write);
	set_voice(p, 2, right_read, first_write, second_write);
	set_voice(p, 3, right_feedback_read, first_write, second_write);
	set_two_line_inputs(p, two_lines);
	const float feedback = scaled(centered_feedback(feedback_level));
	p.first_line_from_left_feedback = feedback;
	p.second_line_from_right_feedback = two_lines ? feedback : 0.0f;
	p.first_line_from_right_feedback = p.second_line_from_left_feedback = 0.0f;
	set_high_damp(p, 2, high_damp);
	set_delay_mix_and_shelves(in, p);
}

// 音符の長さを線に収まるまで半分にしていき、声の読む位置を返す。長さがそのままなら whole_line から、縮めたら halved_line から測る
inline int fitted_note_read(long long length, int lag, int whole_line, int halved_line, int limit)
{
	int read = whole_line + tempo_voice_samples(lag + length);
	while (read >= limit) {
		length /= 2;
		read = halved_line + tempo_voice_samples(lag + length);
	}
	return read;
}

inline void tempo_cross_delay(const effect_request &in, modulated_delay_params &p)
{
	const u16 *P = in.P;
	const int left_to_right_note = P[0], right_to_left_note = P[1], feedback_level = P[2], input_select = P[3], high_damp = P[4], lag = centered_tenths_of_ms(P[5]);
	const cross_lines lines = set_cross_routing(in, p, input_select, feedback_level);
	// 声 0 は長さがそのままなら、入力の選択を最後に反映したときの 1 本目の線から測る
	const int held_first_line = cross_lines_for(in.held_cross_input).first_write;
	set_voice(p, 3, fitted_note_read(note_tenths_of_ms(in, left_to_right_note), lag, first_line_write, first_line_write, memory_size >> 1), lines.first_write, lines.second_write);
	set_voice(p, 0, fitted_note_read(note_tenths_of_ms(in, right_to_left_note), lag, held_first_line, half_memory, memory_size), lines.first_write, lines.second_write);
	set_high_damp(p, 2, high_damp);
	set_delay_mix_and_shelves(in, p);
}

} // namespace modulated_delay

inline bool settle(const effect_request &in, modulated_delay_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_chorus_celeste: modulated_delay::chorus_celeste(in, p, x); return true;
	case tables::effect_flanger: modulated_delay::flanger(in, p, x); return true;
	case tables::effect_symphonic: modulated_delay::symphonic(in, p, x); return true;
	case tables::effect_delay_lcr: modulated_delay::delay_lcr(in, p); return true;
	case tables::effect_delay_lr: modulated_delay::delay_lr(in, p); return true;
	case tables::effect_echo: modulated_delay::echo(in, p); return true;
	case tables::effect_cross_delay: modulated_delay::cross_delay(in, p); return true;
	case tables::effect_tempo_delay: modulated_delay::tempo_delay(in, p); return true;
	case tables::effect_tempo_cross_delay: modulated_delay::tempo_cross_delay(in, p); return true;
	default: return false;
	}
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_MODULATED_DELAY_H
