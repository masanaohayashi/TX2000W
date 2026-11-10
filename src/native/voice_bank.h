// license:BSD-3-Clause
// S-MU2000: doc/voice-flow/voice.md と mixer.md の 3 章から書いた声 128 個。
// 章の番号は voice.md のもの。

#ifndef S_MU2000_NATIVE_VOICE_BANK_H
#define S_MU2000_NATIVE_VOICE_BANK_H

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace native_voice {

constexpr int max_block = 32, voice_count = 128, first_group_size = 64; // 0-63 が第 1 群、64-127 が第 2 群

enum bus_id : int { bus_dry, bus_reverb, bus_chorus, bus_variation, bus_ins1, bus_ins2, bus_ins3, bus_ins4, bus_count };
enum destination_id : int { dest_none, dest_normal, dest_variation, dest_ins1, dest_ins2, dest_ins3, dest_ins4 };

struct bus_block {
	alignas(32) float s[bus_count][2][max_block]; // [母線][0 左,1 右][サンプル]
};

// mixer.md 1 章
inline float att_gain(int a)
{
	if (a >= 255)
		return 0.0f;
	return (1.0f - float(a & 15) / 32.0f) / float(1 << (a >> 4));
}

namespace voice_flow {

// 2 章
constexpr int pitch_min = -8192, pitch_max = 8191;
constexpr int peg_rate_offset = -16;
constexpr int pitch_octave_bits = 10, pitch_fraction_mask = 1023, pitch_table_size = 1024;
constexpr double pitch_table_unit = 4096.0;
constexpr int oct_center = 8, oct_mask = 15, oct_top = 15; // oct は 0 〜 15。step の右シフトは oct_top - oct
constexpr int pitch_table_scale_bits = 10; // × 2^10
// 3 章
constexpr int32_t frac_one = 32768, frac_mask = 32767;
constexpr int frac_bits = 15;
constexpr int32_t loop_fraction_unit = 512;
constexpr int32_t default_loop_end = 1024;
// 4 章
constexpr int format_16 = 0, format_12 = 1, format_8 = 2, format_compressed = 3;
constexpr int value_bits[4] = {16, 12, 8, 8};
constexpr int word_bits = 32, word_bytes = 4, word_bit_shift = 5, byte_bits = 8;
constexpr int bits_12 = 12, scale_12_to_16_bits = 4, scale_8_to_16 = 256;
constexpr int last_value_offset = 3; // v3 は pos + 3
constexpr int expand_index_mask = 127, expand_sign = 128, expand_exponent_shift = 5, expand_mantissa_mask = 31;
constexpr int expand_offset[4] = {0, 32, 96, 224};
constexpr int compress_minus_zero = 128, compress_minus_zero_value = 136;
constexpr int compress_decay_none = 3;
constexpr int decay_numer[3] = {7, 3, 1}, decay_shift[3] = {3, 2, 1};
constexpr std::array<int, 8> compress_limit = [] {
	std::array<int, 8> limits{};
	for (int n = 0; n < 8; n++)
		limits[size_t(n)] = 32768 - (1 << n);
	return limits;
}();
constexpr int wave_min = -32768, wave_max = 32767;
// 5 章
constexpr int phase_shift = 4, phase_last = 2047, weight_bits = 10;
constexpr int weight_unit = 1 << weight_bits, weight_half = weight_unit / 2; // 重みの表は weight_unit 個の値を 2 個ずつ並べたもの
constexpr int near_zero_scale_bits = 20, near_zero_divisor = 6, main_scale_bits = 21;
constexpr float wave_unit = 1.0f / 131072.0f;
// 6 章
constexpr float filter_gain = 0.5f, filter_limit = 0.5f;
constexpr int k_base = 257, k_mantissa_mask = 255, k_exponent_shift = 8;
constexpr float k_unit = 65536.0f;
constexpr int q_offset = 4, q_exponent_shift = 3, q_mantissa_mask = 7, q_mantissa_top = 16, q_exponent_top = 4;
constexpr float q_unit = 128.0f;
// 7 章
constexpr int level_silent = 16383, level_unit = 64, attack_full_start = 128;
constexpr int attack_rate_shift = 9, attack_rate_scale = 4;
constexpr int rate_fastest = 127; // 「最速の離し」の速さ。eg_step も 120 以上はこの刻みを返す
constexpr int rate_top = 120, rate_fast = 72, rate_mid = 64;
constexpr int rate_octave_shift = 3, rate_mix_mask = 7; // 8 段で 2 倍。下 3 ビットが混ぜ方
constexpr int fast_exponent_offset = 9, fast_step_high = 4, fast_step_low = 2;
constexpr int slow_shift_top = 8, mix_clock_mask = 7, slow_mix_clock_mask = 15;
constexpr int gain_bits = 26, gain_slope_bits = 15, att_exponent_bits = 10, att_mantissa_mask = 1023;
constexpr int slot_held_level = 8192;
constexpr int fast_mix[8] = {0x00, 0x20, 0x44, 0xa2, 0x55, 0x75, 0xee, 0xfe};
constexpr int mid_mix[8] = {0x00, 0x01, 0x22, 0xa8, 0x55, 0xab, 0x77, 0xfd};
constexpr int slow_mix[8] = {0x5555, 0x5557, 0x5757, 0x5777, 0x7777, 0x777f, 0x7f7f, 0x7fff};
// 8 章
constexpr int32_t lfo_counter_mask = 0x3ffff;
constexpr int32_t lfo_jump_field = 0x3fc0, lfo_jump_at = 0x2000, lfo_jump = 64;
constexpr int32_t lfo_triangle_center = 0x10000;
constexpr int lfo_shape_center = 2048;
constexpr int lfo_saw_shift = 6, lfo_fold_shift = 5;
constexpr int32_t lfo_fold_top_bit = 0x20000, lfo_fold_mask = 0xffe;
constexpr int lfo_coarse_shift = 9, lfo_fine_shift = 12, lfo_amp_shift = 5;
// lfo_jump_field の周期（下 14 ビット）の中で、飛びの窓を掛け算のために言い直したもの
constexpr int32_t lfo_window_mask = 0x3fff, lfo_window_end = lfo_jump_at + lfo_jump, lfo_window_next = lfo_jump_at + lfo_window_mask + 1;
// 9 章
constexpr int eq_unity = 8192;
constexpr float eq_limit = 1.0f;
// 13 章
constexpr int bundle_width = 4;
// 1 サンプルで位置が進む最大は pitch_table の最後 × 2^10 ÷ 2^15 = 256、ループの端数で 1 足す
constexpr int max_position_advance = 257;
constexpr int compress_history = 4;
constexpr int max_expanded = max_block * max_position_advance + 2 * compress_history;
// mixer.md 3 章: 通常の行き先は 4 母線 × ステレオ
constexpr int max_routes = 8;

inline float sat(float v, float limit)
{
	return v < -limit ? -limit : (v > limit ? limit : v);
}

// 2 章。std::pow が constexpr でないので起動時に作る
inline const std::array<uint16_t, pitch_table_size> pitch_table = [] {
	std::array<uint16_t, pitch_table_size> table{};
	for (int i = 0; i < pitch_table_size; i++)
		table[size_t(i)] = uint16_t(std::floor(std::pow(2.0, double(i) / pitch_table_size) * pitch_table_unit));
	return table;
}();

inline int wrap14(int v)
{
	return ((v - pitch_min) & (2 * (pitch_max + 1) - 1)) + pitch_min;
}

inline int32_t pitch_step(int p)
{
	const int oct = ((p >> pitch_octave_bits) + oct_center) & oct_mask;
	return (int32_t(pitch_table[size_t(p & pitch_fraction_mask)]) << pitch_table_scale_bits) >> (oct_top - oct);
}

// 4 章
inline constexpr std::array<int16_t, 256> expand_table = [] {
	std::array<int16_t, 256> table{};
	for (int c = 0; c < 256; c++) {
		const int i = c & expand_index_mask, s = i >> expand_exponent_shift;
		const int magnitude = ((i & expand_mantissa_mask) << s) + expand_offset[s];
		table[size_t(c)] = int16_t(c == compress_minus_zero ? compress_minus_zero_value : ((c & expand_sign) ? -magnitude : magnitude));
	}
	return table;
}();

// 5 章の near_zero と main
inline constexpr std::array<int16_t, phase_last + 1> near_zero = [] {
	std::array<int16_t, phase_last + 1> table{};
	for (int64_t i = 1; i < weight_unit; i++) {
		const int16_t weight = int16_t(((i * (int64_t(1) << near_zero_scale_bits) - i * i * i) / near_zero_divisor) >> near_zero_scale_bits);
		table[size_t(2 * i - 1)] = table[size_t(2 * i)] = weight;
	}
	return table;
}();

inline constexpr std::array<int16_t, phase_last + 1> main_weight = [] {
	std::array<int16_t, phase_last + 1> table{};
	table[phase_last] = weight_unit;
	for (int64_t i = 1; i <= weight_half; i++) {
		const int16_t weight = int16_t(i + ((i * i * weight_unit - i * i * i) >> main_scale_bits));
		table[size_t(2 * i - 1)] = table[size_t(2 * i)] = weight;
	}
	for (int i = weight_half + 1; i < weight_unit; i++) {
		const int mirror = phase_last ^ (2 * i);
		const int16_t weight = int16_t(weight_unit + near_zero[size_t(2 * i)] + near_zero[size_t(mirror)] - table[size_t(mirror)]);
		table[size_t(2 * i - 1)] = table[size_t(2 * i)] = weight;
	}
	return table;
}();

// 7 章
inline int eg_step(int rate, uint32_t clock)
{
	const int mix = rate & rate_mix_mask;
	if (rate >= rate_top)
		return rate_fastest;
	if (rate >= rate_fast) {
		const int s = (rate >> rate_octave_shift) - fast_exponent_offset;
		return ((fast_mix[mix] >> (clock & mix_clock_mask)) & 1) ? (fast_step_high << s) - 1 : (fast_step_low << s) - 1;
	}
	if (rate >= rate_mid)
		return (clock & 1) ? 1 : ((mid_mix[mix] >> ((clock >> 1) & mix_clock_mask)) & 1);
	const int sh = slow_shift_top - (rate >> rate_octave_shift);
	if (clock & ((uint32_t(1) << sh) - 1))
		return 0;
	return (slow_mix[mix] >> ((clock >> sh) & slow_mix_clock_mask)) & 1;
}

inline float gain_of_att(int att)
{
	if (att >= level_silent)
		return 0.0f;
	const int e = att >> att_exponent_bits, m = att & att_mantissa_mask;
	return float(((1 << gain_bits) - (m << gain_slope_bits)) >> e) / float(1 << gain_bits);
}

// 6 章
inline float filter_k(int code)
{
	return float((k_base + (code & k_mantissa_mask)) * (1 << (code >> k_exponent_shift))) / k_unit;
}

inline float filter_lp_q(int resonance)
{
	const int e = (resonance + q_offset) >> q_exponent_shift, m = (resonance + q_offset) & q_mantissa_mask;
	return std::ldexp(float(q_mantissa_top - m), q_exponent_top - e) / q_unit;
}

// 8 章
inline int32_t lfo_next(int32_t counter, int32_t speed)
{
	counter = (counter + speed) & lfo_counter_mask;
	if ((counter & lfo_jump_field) == lfo_jump_at)
		counter += lfo_jump;
	return counter;
}

// 飛びの窓に入らないあいだは掛け算で進める
inline int32_t lfo_after(int32_t counter, int32_t speed, int samples)
{
	const int32_t low = counter & lfo_window_mask, reach = low + samples * speed;
	if (reach < lfo_jump_at || (low >= lfo_window_end && reach < lfo_window_next))
		return (counter + samples * speed) & lfo_counter_mask;
	for (int s = 0; s < samples; s++)
		counter = lfo_next(counter, speed);
	return counter;
}

inline int lfo_fold(int32_t c)
{
	return (c & lfo_fold_top_bit) ? ((~c >> lfo_fold_shift) & lfo_fold_mask) : ((c >> lfo_fold_shift) & lfo_fold_mask);
}

} // namespace voice_flow

class voice_bank {
public:
	voice_bank() { reset(); }

	// パラメータと声の状態を消す。波形メモリはそのまま
	void reset()
	{
		std::memset(static_cast<void *>(&fields), 0, sizeof(fields));
		for (int v = 0; v < voice_count; v++) {
			fields.level[v] = voice_flow::level_silent;
			fields.stage[v] = stage_release;
			set_lp_cut(v, 0);
			set_resonance(v, 0);
			set_hp_cut(v, 0);
			set_eq(v, 0, voice_flow::eq_unity, 0, 0);
			set_eq(v, 1, voice_flow::eq_unity, 0, 0);
			set_mix(v, 255, 255, 255, 255, 255, 255);
		}
		listed[0] = listed[1] = 0;
		rendered[0] = rendered[1] = 0;
	}

	// rom_bytes は 2 の冪で語の番号はそこで回り込む。語の番号が [ram_first_word, ram_first_word + ram_bytes/4) の間は ram を読む
	void set_wave_memory(const uint8_t *rom_, size_t rom_bytes, const uint8_t *ram_, uint32_t ram_first_word_, size_t ram_bytes)
	{
		rom = rom_;
		rom_words = uint32_t(rom_bytes / voice_flow::word_bytes);
		ram = ram_;
		ram_first_word = ram_first_word_;
		ram_words = uint32_t(ram_bytes / voice_flow::word_bytes);
	}

	void set_pitch_base(int v, int value) { fields.pitch_base[v] = value; }
	void set_peg_target(int v, int value)
	{
		fields.peg_target[v] = value;
		if (fields.peg_current[v] != value)
			list_voice(v); // idle でも音程の EG は動く（10 章）
	}
	void set_peg_rate(int v, int value) { fields.peg_rate[v] = value; }
	void set_wave_format(int v, int format)
	{
		fields.wave_format[v] = format;
		fields.decoded_end[v] = fields.pos[v] + voice_flow::compress_history; // 鳴っている途中で圧縮に変わっても解く範囲が溢れないように
	}
	void set_compress_decay(int v, int value) { fields.compress_decay[v] = value; }
	void set_compress_scale(int v, int value) { fields.compress_scale[v] = value; }
	void set_wave_base(int v, uint32_t word) { fields.wave_base[v] = word; }
	void set_lead_length(int v, uint32_t value) { fields.lead_length[v] = int32_t(value); }
	void set_loop_length(int v, uint32_t value) { fields.loop_length[v] = int32_t(value); }
	void set_loop_fraction(int v, int value) { fields.loop_fraction[v] = value; }
	void set_one_shot(int v, bool value) { fields.one_shot[v] = value; }
	void set_reverse(int v, bool value) { fields.reverse[v] = value; }
	void set_fine_tune(int v, int value) { fields.fine_tune[v] = value; }

	void set_lp_cut(int v, int code) { fields.lp_k[v] = voice_flow::filter_k(code); }
	void set_resonance(int v, int value) { fields.lp_q[v] = voice_flow::filter_lp_q(value); }
	void set_hp_cut(int v, int code)
	{
		fields.high_pass_on[v] = code != 0;
		fields.hp_k[v] = voice_flow::filter_k(code);
	}
	// stage 0 が低域、1 が高域。値は符号付き 16 ビットのまま
	void set_eq(int v, int stage, int current, int previous, int feedback)
	{
		// 9 章の「y は x と同じ」形。束の中では係数 (1, 0, 0) にすれば、計算しても y = x、x⁻¹ = y⁻¹ = x になる
		const bool same = current == voice_flow::eq_unity && previous == -feedback;
		fields.eq_same[v][stage] = same;
		fields.eq_current[v][stage] = same ? 1.0f : float(current) / float(voice_flow::eq_unity);
		fields.eq_previous[v][stage] = same ? 0.0f : float(previous) / float(voice_flow::eq_unity);
		fields.eq_feedback[v][stage] = same ? 0.0f : float(feedback) / float(voice_flow::eq_unity);
	}

	void set_attack(int v, int rate, int start)
	{
		fields.attack_rate[v] = rate;
		fields.attack_start[v] = start;
	}
	void set_decay1(int v, int rate, int level)
	{
		fields.decay_rate[v][0] = rate;
		fields.decay_goal[v][0] = level * voice_flow::level_unit;
	}
	void set_decay2(int v, int rate, int level)
	{
		fields.decay_rate[v][1] = rate;
		fields.decay_goal[v][1] = level * voice_flow::level_unit;
	}
	// armed が立っていれば、その時点で離す（7 章）
	void set_release(int v, int rate, bool armed, int global_att)
	{
		fields.release_rate[v] = rate;
		fields.release_armed[v] = armed;
		fields.global_att[v] = global_att;
		if (armed)
			fields.stage[v] = stage_release;
	}
	void set_lfo(int v, int wave, int speed, bool coarse, int pitch_depth)
	{
		fields.lfo_wave[v] = wave;
		fields.lfo_speed[v] = speed;
		fields.lfo_pitch_coarse[v] = coarse;
		fields.lfo_pitch_depth[v] = pitch_depth;
	}
	void set_lfo_amp_depth(int v, int depth) { fields.lfo_amp_depth[v] = depth; }

	void set_mix(int v, int pan_att_left, int pan_att_right, int dry_att, int reverb_att, int chorus_att, int variation_att)
	{
		fields.pan_att[v][0] = pan_att_left;
		fields.pan_att[v][1] = pan_att_right;
		fields.send_att[v][bus_dry] = dry_att;
		fields.send_att[v][bus_reverb] = reverb_att;
		fields.send_att[v][bus_chorus] = chorus_att;
		fields.send_att[v][bus_variation] = variation_att;
		rebuild_routes(v);
	}
	void set_destination(int v, destination_id d)
	{
		fields.destination[v] = d;
		rebuild_routes(v);
	}

	// 11 章
	void key_on(int v)
	{
		fields.pos[v] = -fields.lead_length[v] - 1;
		fields.frac[v] = 0;
		fields.first_read[v] = true;
		fields.past_loop_point[v] = false;
		fields.finished[v] = false;
		fields.accumulated[v] = 0;
		fields.momentum[v] = 0;
		fields.remainder[v] = 0;
		fields.decoded_end[v] = -fields.lead_length[v];
		std::fill_n(fields.history[v], voice_flow::compress_history, int16_t(0));

		fields.lp_low[v] = fields.lp_band[v] = fields.hp_low[v] = fields.hp_band[v] = 0.0f;
		for (int stage = 0; stage < 2; stage++)
			fields.eq_x1[v][stage] = fields.eq_y1[v][stage] = 0.0f;

		fields.level[v] = (fields.attack_start[v] != 0 ? fields.attack_start[v] : voice_flow::attack_full_start) * voice_flow::level_unit;
		fields.stage[v] = stage_attack;
		fields.lfo_counter[v] = 0;
		fields.peg_current[v] = fields.peg_target[v];
		list_voice(v);
	}

	// n サンプル（1 〜 32）作る。first と second の [0, n) を 0 にしてから、第 1 群・第 2 群の声を足す
	// ミュートした声は進み続けるが母線へ足さない（解くと途中から聞こえる）

	void render(int n, uint32_t sample_clock, bus_block &first, bus_block &second)
	{
		for (int bus = 0; bus < bus_count; bus++)
			for (int channel = 0; channel < 2; channel++) {
				std::fill_n(first.s[bus][channel], n, 0.0f);
				std::fill_n(second.s[bus][channel], n, 0.0f);
			}

		// 鳴っている声を束の形（ハイパスの有無 × EQ の有無）に分ける
		int bundle_voices[4][voice_count];
		int bundle_voices_count[4] = {0, 0, 0, 0};
		rendered[0] = rendered[1] = 0;
		for (int word = 0; word < 2; word++) {
			for (uint64_t bits = listed[word]; bits != 0; bits &= bits - 1) {
				const int v = word * first_group_size + std::countr_zero(bits);
				if (is_idle(v)) {
					move_peg(v, n, sample_clock, nullptr);
					continue;
				}
				rendered[word] |= uint64_t(1) << (v & 63);
				const int shape = (fields.high_pass_on[v] ? 1 : 0) | ((fields.eq_same[v][0] && fields.eq_same[v][1]) ? 0 : 2);
				bundle_voices[shape][bundle_voices_count[shape]++] = v;
			}
		}
		for (int i = 0; i < bundle_voices_count[0]; i += voice_flow::bundle_width)
			run_bundle<false, false>(bundle_voices[0] + i, std::min(voice_flow::bundle_width, bundle_voices_count[0] - i), n, sample_clock);
		for (int i = 0; i < bundle_voices_count[1]; i += voice_flow::bundle_width)
			run_bundle<true, false>(bundle_voices[1] + i, std::min(voice_flow::bundle_width, bundle_voices_count[1] - i), n, sample_clock);
		for (int i = 0; i < bundle_voices_count[2]; i += voice_flow::bundle_width)
			run_bundle<false, true>(bundle_voices[2] + i, std::min(voice_flow::bundle_width, bundle_voices_count[2] - i), n, sample_clock);
		for (int i = 0; i < bundle_voices_count[3]; i += voice_flow::bundle_width)
			run_bundle<true, true>(bundle_voices[3] + i, std::min(voice_flow::bundle_width, bundle_voices_count[3] - i), n, sample_clock);

		// 母線へ足す順は声の番号の順に固定する（float の和はブロックの分け方で順が変わると変わる）
		for (int word = 0; word < 2; word++) {
			bus_block &group = word == 0 ? first : second;
			for (uint64_t bits = rendered[word]; bits != 0; bits &= bits - 1) {
				const int v = word * first_group_size + std::countr_zero(bits);
				const float *output = fields.output[v];
				for (int r = 0; r < fields.route_count[v]; r++) {
					const float gain = fields.route_gain[v][r];
					float *target = group.s[fields.route_bus[v][r]][fields.route_channel[v][r]];
					for (int s = 0; s < n; s++)
						target[s] += gain * output[s];
				}
			}
		}

		for (int word = 0; word < 2; word++)
			for (uint64_t bits = listed[word]; bits != 0; bits &= bits - 1) {
				const int v = word * first_group_size + std::countr_zero(bits);
				if (is_idle(v) && fields.peg_current[v] == fields.peg_target[v])
					listed[word] &= ~(uint64_t(1) << (v & 63));
			}
	}

	int eg_level(int v) const { return fields.level[v]; }
	bool peg_reached(int v) const { return fields.peg_current[v] == fields.peg_target[v]; }
	bool sounding(int v) const { return !is_idle(v); }
	bool slot_held(int v) const { return fields.stage[v] == stage_attack || fields.level[v] < voice_flow::slot_held_level; }

	// 直前の render の声の出力（母線の単位。鳴っていない声は 0 の配列を返す）
	const float *voice_output(int v) const
	{
		return ((rendered[v / first_group_size] >> (v & 63)) & 1) ? fields.output[v] : silent_output;
	}

private:
	enum eg_stage : int { stage_attack, stage_decay1, stage_decay2, stage_release };

	// 13 章: 値ごとの配列
	struct voice_fields {
		// パラメータ
		int32_t pitch_base[voice_count], peg_target[voice_count], peg_rate[voice_count];
		int32_t wave_format[voice_count], compress_decay[voice_count], compress_scale[voice_count];
		uint32_t wave_base[voice_count];
		int32_t lead_length[voice_count], loop_length[voice_count], loop_fraction[voice_count], fine_tune[voice_count];
		uint8_t one_shot[voice_count], reverse[voice_count], high_pass_on[voice_count], eq_same[voice_count][2];
		float lp_k[voice_count], lp_q[voice_count], hp_k[voice_count];
		float eq_current[voice_count][2], eq_previous[voice_count][2], eq_feedback[voice_count][2];
		int32_t attack_rate[voice_count], attack_start[voice_count];
		int32_t decay_rate[voice_count][2], decay_goal[voice_count][2]; // [0] 減衰 1、[1] 減衰 2
		int32_t release_rate[voice_count], global_att[voice_count];
		uint8_t release_armed[voice_count];
		int32_t lfo_wave[voice_count], lfo_speed[voice_count], lfo_pitch_depth[voice_count], lfo_amp_depth[voice_count];
		uint8_t lfo_pitch_coarse[voice_count];
		int32_t pan_att[voice_count][2], send_att[voice_count][4], destination[voice_count];
		// 母線への道（書き換えのときに作る。利得が 0 の道は持たない）
		float route_gain[voice_count][voice_flow::max_routes];
		uint8_t route_bus[voice_count][voice_flow::max_routes], route_channel[voice_count][voice_flow::max_routes], route_count[voice_count];

		// 状態
		int32_t peg_current[voice_count];
		int32_t pos[voice_count], frac[voice_count];
		uint8_t first_read[voice_count], past_loop_point[voice_count], finished[voice_count];
		int32_t hold_value[voice_count];
		int32_t accumulated[voice_count], momentum[voice_count], remainder[voice_count], decoded_end[voice_count];
		int16_t history[voice_count][voice_flow::compress_history]; // 圧縮で最後に解いた 4 つ（位置 decoded_end-4 から）
		float lp_low[voice_count], lp_band[voice_count], hp_low[voice_count], hp_band[voice_count];
		float eq_x1[voice_count][2], eq_y1[voice_count][2];
		int32_t level[voice_count], stage[voice_count];
		int32_t lfo_counter[voice_count];

		alignas(32) float output[voice_count][max_block];
	};

	// 1 束ぶんのブロック。[サンプル][レーン]
	struct lane_block {
		alignas(32) float signal[max_block][voice_flow::bundle_width];
		alignas(32) float gain[max_block][voice_flow::bundle_width];
	};

	// 位置を進めた結果
	struct position_run {
		int32_t pos, frac;
		bool first_read, past_loop_point;
		int finish_sample;
		uint32_t wrap_mask; // そのサンプルの終わりでループが戻ったビット
		int32_t pos_at[max_block], phase_at[max_block];
	};

	static_assert(std::endian::native == std::endian::little, "波形メモリの語をそのまま読むためリトルエンディアンだけ"); // ponytail: ビッグエンディアンの機種が要るなら load_words の後でバイトを入れ替える

	bool is_idle(int v) const { return fields.level[v] == voice_flow::level_silent && fields.stage[v] == stage_release; }
	void list_voice(int v) { listed[v / first_group_size] |= uint64_t(1) << (v & 63); }

	void rebuild_routes(int v)
	{
		int count = 0;
		const auto add_route = [&](int bus, int att) {
			for (int channel = 0; channel < 2; channel++) {
				const float gain = att_gain(fields.pan_att[v][channel] + att);
				if (gain == 0.0f)
					continue;
				fields.route_gain[v][count] = gain;
				fields.route_bus[v][count] = uint8_t(bus);
				fields.route_channel[v][count] = uint8_t(channel);
				count++;
			}
		};
		const int variation_att = fields.send_att[v][bus_variation];
		switch (fields.destination[v]) {
		case dest_normal:
			for (int bus = bus_dry; bus <= bus_variation; bus++)
				add_route(bus, fields.send_att[v][bus]);
			break;
		case dest_variation: add_route(bus_variation, variation_att); break;
		case dest_ins1: add_route(bus_ins1, variation_att); break;
		case dest_ins2: add_route(bus_ins2, variation_att); break;
		case dest_ins3: add_route(bus_ins3, variation_att); break;
		case dest_ins4: add_route(bus_ins4, variation_att); break;
		default: break;
		}
		fields.route_count[v] = uint8_t(count);
	}

	// 2 章の音程の EG。peg_at があれば動かしたあとの値を書く
	void move_peg(int v, int n, uint32_t sample_clock, int32_t *peg_at)
	{
		int32_t peg = fields.peg_current[v];
		const int32_t target = fields.peg_target[v];
		const int rate = fields.peg_rate[v] + voice_flow::peg_rate_offset;
		for (int s = 0; s < n; s++) {
			const int r = voice_flow::eg_step(rate, sample_clock + uint32_t(s));
			peg = peg < target ? std::min(peg + r, target) : std::max(peg - r, target);
			if (peg_at != nullptr)
				peg_at[s] = peg;
		}
		fields.peg_current[v] = peg;
	}

	int32_t loop_end_of(int v) const
	{
		const bool loops = !fields.one_shot[v] && !fields.reverse[v];
		return (loops && fields.loop_length[v] == 0) ? voice_flow::default_loop_end : fields.loop_length[v];
	}

	// 3 章の「そうでなければ」。finished になったら真
	bool position_carry(int v, int32_t loop_end, int s, position_run &run) const
	{
		run.first_read = false;
		run.pos += run.frac >> voice_flow::frac_bits;
		run.frac &= voice_flow::frac_mask;
		if (run.pos >= 0)
			run.past_loop_point = true;
		if (run.pos < loop_end)
			return false;
		if (fields.one_shot[v] || fields.reverse[v]) {
			run.finish_sample = s;
			return true;
		}
		run.pos -= loop_end;
		run.frac += fields.loop_fraction[v] * voice_flow::loop_fraction_unit;
		if (run.frac >= voice_flow::frac_one)
			run.pos += 1;
		run.frac &= voice_flow::frac_mask;
		run.wrap_mask |= uint32_t(1) << s;
		return false;
	}

	void start_position_run(int v, position_run &run) const
	{
		run.pos = fields.pos[v];
		run.frac = fields.frac[v];
		run.first_read = fields.first_read[v];
		run.past_loop_point = fields.past_loop_point[v];
		run.finish_sample = -1;
		run.wrap_mask = 0;
	}

	// 音程が動くとき: 1 サンプルずつ
	void advance_position_moving(int v, int count, const int32_t *peg_at, int32_t peg_constant, const int32_t *lfo_pitch_at, int32_t lfo_pitch_constant, position_run &run) const
	{
		start_position_run(v, run);
		const int32_t loop_end = loop_end_of(v);
		for (int s = 0; s < count; s++) {
			run.pos_at[s] = run.pos;
			run.phase_at[s] = run.frac >> voice_flow::phase_shift;
			int p = voice_flow::wrap14(fields.pitch_base[v] + (peg_at != nullptr ? peg_at[s] : peg_constant))
				+ (lfo_pitch_at != nullptr ? lfo_pitch_at[s] : lfo_pitch_constant);
			if (run.past_loop_point)
				p = std::clamp(p + fields.fine_tune[v], voice_flow::pitch_min, voice_flow::pitch_max);
			run.frac += voice_flow::pitch_step(p);
			if (run.frac >= voice_flow::frac_one && position_carry(v, loop_end, s, run))
				return;
		}
	}

	// 音程が動かないとき: 位置は掛け算、区切りは割り算（3 章）。step が変わるのは past_loop_point が立つときだけ
	void advance_position_constant(int v, int count, int32_t peg, int32_t lfo_pitch, position_run &run) const
	{
		start_position_run(v, run);
		const int32_t loop_end = loop_end_of(v);
		const int p = voice_flow::wrap14(fields.pitch_base[v] + peg) + lfo_pitch;
		const int32_t step_before = voice_flow::pitch_step(p);
		const int32_t step_past = voice_flow::pitch_step(std::clamp(p + fields.fine_tune[v], voice_flow::pitch_min, voice_flow::pitch_max));
		for (int s = 0; s < count;) {
			const int32_t step = run.past_loop_point ? step_past : step_before;
			const int32_t boundary = run.past_loop_point ? loop_end : 0;
			// 区切りの検査は位置が動くときにしか起きないので、いま越えていても 1 は進む
			const int32_t needed = std::max(boundary - run.pos, 1);
			const int left = count - s;
			const int32_t reach = run.frac + left * step;
			const bool crosses = (reach >> voice_flow::frac_bits) >= needed;
			const int samples = crosses ? ((needed << voice_flow::frac_bits) - run.frac + step - 1) / step : left;
			for (int i = 0; i < samples; i++) {
				const int32_t t = run.frac + i * step;
				run.pos_at[s + i] = run.pos + (t >> voice_flow::frac_bits);
				run.phase_at[s + i] = (t & voice_flow::frac_mask) >> voice_flow::phase_shift;
			}
			const int32_t t = run.frac + samples * step;
			s += samples;
			if (crosses) {
				run.frac = t;
				if (position_carry(v, loop_end, s - 1, run))
					return;
			} else {
				if (t >= voice_flow::frac_one)
					run.first_read = false;
				run.pos += t >> voice_flow::frac_bits;
				run.frac = t & voice_flow::frac_mask;
			}
		}
	}

	// 語を並べて返す。一続きならメモリをそのまま指す
	const uint8_t *load_words(uint32_t word, uint32_t word_count)
	{
		uint8_t *scratch = reinterpret_cast<uint8_t *>(word_scratch);
		uint32_t filled = 0;
		while (filled < word_count) {
			const uint32_t left = word_count - filled;
			const uint32_t ram_offset = word - ram_first_word;
			const uint8_t *source = nullptr;
			uint32_t chunk = left;
			if (ram_offset < ram_words) {
				source = ram + size_t(ram_offset) * voice_flow::word_bytes;
				chunk = std::min(left, ram_words - ram_offset);
			} else if (rom_words != 0) {
				const uint32_t rom_offset = word & (rom_words - 1);
				source = rom + size_t(rom_offset) * voice_flow::word_bytes;
				chunk = std::min(left, rom_words - rom_offset);
				const uint32_t to_ram = ram_first_word - word;
				if (ram_words != 0 && to_ram < chunk)
					chunk = to_ram;
			}
			if (source != nullptr && chunk == word_count)
				return source;
			if (source != nullptr)
				std::memcpy(scratch + size_t(filled) * voice_flow::word_bytes, source, size_t(chunk) * voice_flow::word_bytes);
			else
				std::memset(scratch + size_t(filled) * voice_flow::word_bytes, 0, size_t(chunk) * voice_flow::word_bytes);
			filled += chunk;
			word += chunk;
		}
		return scratch;
	}

	// 位置 first から count 個のメモリを指す。bit_offset は返す指し先の中のビットの位置
	const uint8_t *load_positions(int v, int32_t first, int count, int &bit_offset)
	{
		const int bits = voice_flow::value_bits[fields.wave_format[v]];
		const int32_t memory_first = fields.reverse[v] ? 3 - first - count : first; // 4 章: 位置 k はメモリの 2-k
		const int64_t first_bit = int64_t(fields.wave_base[v]) * voice_flow::word_bits + int64_t(memory_first) * bits;
		bit_offset = int(first_bit & (voice_flow::word_bits - 1));
		// 12 ビットは語の 2 つ組で読むので 1 語余分に
		const uint32_t word_count = uint32_t((int64_t(bit_offset) + int64_t(count) * bits + voice_flow::word_bits - 1) / voice_flow::word_bits) + (bits == voice_flow::bits_12 ? 1u : 0u);
		return load_words(uint32_t(first_bit >> voice_flow::word_bit_shift), word_count);
	}

	// 4 章の線形の形式
	void load_values(int v, int32_t first, int count, int16_t *values)
	{
		int bit_offset = 0;
		const uint8_t *memory = load_positions(v, first, count, bit_offset);
		switch (fields.wave_format[v]) {
		case voice_flow::format_16:
			std::memcpy(values, memory + bit_offset / voice_flow::byte_bits, size_t(count) * sizeof(int16_t));
			break;
		case voice_flow::format_8: {
			const uint8_t *bytes = memory + bit_offset / voice_flow::byte_bits;
			for (int i = 0; i < count; i++)
				values[i] = int16_t(int8_t(bytes[i]) * voice_flow::scale_8_to_16);
			break;
		}
		default:
			for (int i = 0; i < count; i++) {
				const int bit = bit_offset + i * voice_flow::bits_12;
				uint64_t pair;
				std::memcpy(&pair, memory + (bit >> voice_flow::word_bit_shift) * voice_flow::word_bytes, sizeof(pair));
				values[i] = int16_t(uint16_t((pair >> (bit & (voice_flow::word_bits - 1))) << voice_flow::scale_12_to_16_bits)); // 符号付きの 12 ビット値 × 16
			}
			break;
		}
		if (fields.reverse[v])
			std::reverse(values, values + count);
	}

	// 4 章の圧縮。位置 through まで解いて values の先へ並べ、並べた数を返す
	int decode_compressed(int v, int32_t through, int16_t *values)
	{
		const int count = through - fields.decoded_end[v] + 1;
		if (count <= 0)
			return 0;
		int bit_offset = 0;
		// bit_offset は load_positions が決める。同じ式で読むと、コンパイラによって呼ぶ前の値になる
		const uint8_t *memory = load_positions(v, fields.decoded_end[v], count, bit_offset);
		const uint8_t *bytes = memory + bit_offset / voice_flow::byte_bits;

		const int scale = fields.compress_scale[v];
		const int decay = fields.compress_decay[v];
		const int limit = voice_flow::compress_limit[scale];
		int accumulated = fields.accumulated[v], momentum = fields.momentum[v], remainder = fields.remainder[v];
		for (int i = 0; i < count; i++) {
			const uint8_t c = bytes[fields.reverse[v] ? count - 1 - i : i];
			const int previous = accumulated >> scale;
			const int d = momentum + voice_flow::expand_table[c];
			accumulated = std::clamp((previous + d) * (1 << scale), voice_flow::wave_min, limit);
			const int moved = (accumulated >> scale) - previous;
			if (decay == voice_flow::compress_decay_none) {
				momentum = 0;
			} else {
				const int shift = voice_flow::decay_shift[decay];
				const int y = voice_flow::decay_numer[decay] * moved - remainder;
				momentum = y >> shift;
				const int t = y - momentum * (1 << shift);
				remainder = t == 0 ? 0 : (1 << shift) - t;
			}
			values[i] = int16_t(accumulated);
		}
		fields.accumulated[v] = accumulated;
		fields.momentum[v] = momentum;
		fields.remainder[v] = remainder;
		fields.decoded_end[v] = through + 1;
		return count;
	}

	// 4 章・5 章。読むものを一時の配列へ展開してから、整数で補間して signal のレーンへ入れる
	void read_wave(int v, const position_run &run, bool first_read_at_start, int live, int lane, lane_block &bundle)
	{
		int16_t expanded[voice_flow::max_expanded];
		const bool compressed = fields.wave_format[v] == voice_flow::format_compressed;
		const int32_t loop_end = loop_end_of(v);
		const int interpolate_end = run.finish_sample >= 0 ? run.finish_sample + 1 : live;
		int32_t last_out = 0;

		for (int a = 0; a < interpolate_end;) {
			// ループで戻るところで区切る（位置はそのあいだ増えるだけ）
			const uint32_t wraps = run.wrap_mask >> a;
			const int b = std::min(wraps != 0 ? a + std::countr_zero(wraps) + 1 : interpolate_end, interpolate_end);
			const int32_t through = run.pos_at[b - 1] + voice_flow::last_value_offset;

			int32_t base;
			if (compressed) {
				base = fields.decoded_end[v] - voice_flow::compress_history;
				std::copy_n(fields.history[v], voice_flow::compress_history, expanded);
				const int added = decode_compressed(v, through, expanded + voice_flow::compress_history);
				std::copy_n(expanded + added, voice_flow::compress_history, fields.history[v]);
			} else {
				base = run.pos_at[a];
				load_values(v, base, through - base + 1, expanded);
			}
			if (a == 0 && first_read_at_start)
				expanded[run.pos_at[0] - base] = 0; // first_read のあいだ位置は動かないので、ここの値は v0 にしか使われない

			for (int s = a; s < b; s++) {
				const int16_t *values = expanded + (run.pos_at[s] - base);
				const int phase = run.phase_at[s], mirror = phase ^ voice_flow::phase_last;
				const int32_t sum = -voice_flow::near_zero[size_t(mirror)] * values[0]
					+ voice_flow::main_weight[size_t(mirror)] * values[1]
					+ voice_flow::main_weight[size_t(phase)] * values[2]
					- voice_flow::near_zero[size_t(phase)] * values[3];
				last_out = std::clamp(sum >> voice_flow::weight_bits, voice_flow::wave_min, voice_flow::wave_max);
				bundle.signal[s][lane] = float(last_out) * voice_flow::wave_unit;
			}
			if (compressed && ((run.wrap_mask >> (b - 1)) & 1))
				fields.decoded_end[v] -= loop_end; // 値は連続したまま、解く位置だけ戻す
			a = b;
		}
		if (run.finish_sample >= 0) {
			fields.hold_value[v] = last_out;
			const float hold = float(fields.hold_value[v]) * voice_flow::wave_unit;
			for (int s = interpolate_end; s < live; s++)
				bundle.signal[s][lane] = hold;
		}
	}

	// 7 章。鳴るサンプル数を返す。動かない声は att_eg_at[0] だけ書いて att_eg_constant を立てる
	int run_eg(int v, int n, uint32_t sample_clock, int finish_sample, int32_t *att_eg_at, bool &att_eg_constant)
	{
		int level = fields.level[v], stage = fields.stage[v];
		const int global = fields.global_att[v] * voice_flow::level_unit;
		att_eg_constant = stage == stage_decay2 && level == fields.decay_goal[v][1] && !fields.release_armed[v] && finish_sample < 0;
		if (att_eg_constant) {
			att_eg_at[0] = level + global;
			return n;
		}
		int s = 0;
		for (; s < n; s++) {
			if (level == voice_flow::level_silent && stage == stage_release)
				break;
			if (s == finish_sample) {
				stage = stage_release;
				fields.release_armed[v] = true;
				fields.release_rate[v] = voice_flow::rate_fastest;
			}
			const uint32_t clock = sample_clock + uint32_t(s);
			att_eg_at[s] = (stage == stage_attack && fields.attack_start[v] == 0) ? global : level + global;
			switch (stage) {
			case stage_attack:
				level -= voice_flow::eg_step(fields.attack_rate[v] + (level >> voice_flow::attack_rate_shift) * voice_flow::attack_rate_scale, clock);
				if (level <= 0) {
					level = 0;
					stage = stage_decay1;
				}
				break;
			case stage_decay1:
			case stage_decay2: {
				const int which = stage == stage_decay1 ? 0 : 1;
				const int goal = fields.decay_goal[v][which];
				if (level != goal) {
					const int r = voice_flow::eg_step(fields.decay_rate[v][which], clock);
					level = level < goal ? std::min(level + r, goal) : std::max(level - r, goal);
				}
				if (level == goal) {
					if (stage == stage_decay1)
						stage = stage_decay2;
					else if (fields.release_armed[v])
						stage = stage_release;
				}
				break;
			}
			default:
				level = std::min(level + voice_flow::eg_step(fields.release_armed[v] ? fields.release_rate[v] : voice_flow::rate_fastest, clock), voice_flow::level_silent);
				break;
			}
		}
		fields.level[v] = level;
		fields.stage[v] = stage;
		return s;
	}

	int32_t lfo_pitch_of(int v, int32_t counter) const
	{
		const int shape = fields.lfo_wave[v] == 0 ? (counter >> voice_flow::lfo_saw_shift)
			: voice_flow::lfo_fold((counter + voice_flow::lfo_triangle_center) & voice_flow::lfo_counter_mask);
		return ((shape - voice_flow::lfo_shape_center) * fields.lfo_pitch_depth[v])
			>> (fields.lfo_pitch_coarse[v] ? voice_flow::lfo_coarse_shift : voice_flow::lfo_fine_shift);
	}

	int32_t lfo_amp_of(int v, int32_t counter) const
	{
		const int shape = fields.lfo_wave[v] == 0 ? (counter >> voice_flow::lfo_saw_shift) : voice_flow::lfo_fold(counter);
		return (shape * fields.lfo_amp_depth[v]) >> voice_flow::lfo_amp_shift;
	}

	// 声 1 つの整数の部分（音程・位置・波形・EG・LFO）を 1 ブロックぶん回して、レーンの signal と gain を埋める。鳴るサンプル数を返す
	int run_voice(int v, int n, uint32_t sample_clock, int lane, lane_block &bundle)
	{
		int32_t peg_at[max_block];
		const bool peg_moving = fields.peg_current[v] != fields.peg_target[v];
		if (peg_moving)
			move_peg(v, n, sample_clock, peg_at);

		const int32_t counter = fields.lfo_counter[v], speed = fields.lfo_speed[v];
		const bool lfo_moving = speed != 0 || (counter & voice_flow::lfo_jump_field) == voice_flow::lfo_jump_at;
		const bool lfo_pitch_moving = lfo_moving && fields.lfo_pitch_depth[v] != 0;
		const bool lfo_amp_moving = lfo_moving && fields.lfo_amp_depth[v] != 0;
		int32_t lfo_pitch_at[max_block], lfo_amp_at[max_block];
		if (lfo_pitch_moving || lfo_amp_moving) {
			int32_t c = counter;
			for (int s = 0; s < n; s++) {
				lfo_pitch_at[s] = lfo_pitch_of(v, c);
				lfo_amp_at[s] = lfo_amp_of(v, c);
				c = voice_flow::lfo_next(c, speed);
			}
		}
		const int32_t lfo_pitch_constant = fields.lfo_pitch_depth[v] != 0 ? lfo_pitch_of(v, counter) : 0;
		const int32_t lfo_amp_constant = fields.lfo_amp_depth[v] != 0 ? lfo_amp_of(v, counter) : 0;

		position_run run;
		const bool pitch_moving = peg_moving || lfo_pitch_moving;
		const bool was_finished = fields.finished[v];
		if (was_finished)
			start_position_run(v, run);
		else if (pitch_moving)
			advance_position_moving(v, n, peg_moving ? peg_at : nullptr, fields.peg_current[v], lfo_pitch_moving ? lfo_pitch_at : nullptr, lfo_pitch_constant, run);
		else
			advance_position_constant(v, n, fields.peg_current[v], lfo_pitch_constant, run);

		int32_t att_eg_at[max_block];
		bool att_eg_constant;
		const int live = run_eg(v, n, sample_clock, run.finish_sample, att_eg_at, att_eg_constant);
		// 途中で idle になっても位置は n まで進めたままでよい: idle の声の位置はキーオンまで誰も見ず、キーオンで初期化される

		const bool first_read_at_start = fields.first_read[v];
		fields.pos[v] = run.pos;
		fields.frac[v] = run.frac;
		fields.first_read[v] = run.first_read;
		fields.past_loop_point[v] = run.past_loop_point;
		fields.lfo_counter[v] = voice_flow::lfo_after(counter, speed, live);

		if (was_finished) {
			const float hold = float(fields.hold_value[v]) * voice_flow::wave_unit;
			for (int s = 0; s < live; s++)
				bundle.signal[s][lane] = hold;
		} else {
			read_wave(v, run, first_read_at_start, live, lane, bundle);
			fields.finished[v] = run.finish_sample >= 0;
		}

		if (att_eg_constant && !lfo_amp_moving) {
			const float gain = voice_flow::gain_of_att(att_eg_at[0] + lfo_amp_constant);
			for (int s = 0; s < live; s++)
				bundle.gain[s][lane] = gain;
		} else {
			for (int s = 0; s < live; s++) {
				const int att = att_eg_at[att_eg_constant ? 0 : s] + (lfo_amp_moving ? lfo_amp_at[s] : lfo_amp_constant);
				bundle.gain[s][lane] = voice_flow::gain_of_att(att);
			}
		}
		for (int s = live; s < n; s++)
			bundle.signal[s][lane] = bundle.gain[s][lane] = 0.0f;
		return live;
	}

	// 6 章・9 章・利得。声を bundle_width 本ずつ横に束ねて回す。余りのレーンは 0
	template <bool use_high_pass, bool use_eq>
	void run_bundle(const int *lane_voice, int lanes, int n, uint32_t sample_clock)
	{
		constexpr int width = voice_flow::bundle_width;
		lane_block bundle;
		int live[width] = {};
		for (int lane = 0; lane < width; lane++) {
			if (lane < lanes) {
				live[lane] = run_voice(lane_voice[lane], n, sample_clock, lane, bundle);
			} else {
				for (int s = 0; s < n; s++)
					bundle.signal[s][lane] = bundle.gain[s][lane] = 0.0f;
			}
		}

		alignas(32) float lp_k[width] = {}, lp_q[width] = {}, low[width] = {}, band[width] = {};
		alignas(32) float hp_k[width] = {}, low2[width] = {}, band2[width] = {};
		alignas(32) float low_current[width] = {}, low_previous[width] = {}, low_feedback[width] = {}, low_x1[width] = {}, low_y1[width] = {};
		alignas(32) float high_current[width] = {}, high_previous[width] = {}, high_feedback[width] = {}, high_x1[width] = {}, high_y1[width] = {};
		alignas(32) float entering_eq[width] = {};
		for (int lane = 0; lane < lanes; lane++) {
			const int v = lane_voice[lane];
			lp_k[lane] = fields.lp_k[v];
			lp_q[lane] = fields.lp_q[v];
			low[lane] = fields.lp_low[v];
			band[lane] = fields.lp_band[v];
			if constexpr (use_high_pass) {
				hp_k[lane] = fields.hp_k[v];
				low2[lane] = fields.hp_low[v];
				band2[lane] = fields.hp_band[v];
			}
			if constexpr (use_eq) {
				low_current[lane] = fields.eq_current[v][0];
				low_previous[lane] = fields.eq_previous[v][0];
				low_feedback[lane] = fields.eq_feedback[v][0];
				low_x1[lane] = fields.eq_x1[v][0];
				low_y1[lane] = fields.eq_y1[v][0];
				high_current[lane] = fields.eq_current[v][1];
				high_previous[lane] = fields.eq_previous[v][1];
				high_feedback[lane] = fields.eq_feedback[v][1];
				high_x1[lane] = fields.eq_x1[v][1];
				high_y1[lane] = fields.eq_y1[v][1];
			}
		}

		for (int s = 0; s < n; s++) {
			float *x = bundle.signal[s];
			const float *gain = bundle.gain[s];
			for (int lane = 0; lane < width; lane++) {
				const float high = x[lane] - low[lane] - lp_q[lane] * band[lane];
				band[lane] += lp_k[lane] * high;
				low[lane] += lp_k[lane] * band[lane];
				float y = low[lane];
				if constexpr (use_high_pass) {
					const float high2 = y - low2[lane] - band2[lane];
					band2[lane] += hp_k[lane] * high2;
					low2[lane] += hp_k[lane] * band2[lane];
					y = high2;
				}
				y = voice_flow::sat(voice_flow::filter_gain * y, voice_flow::filter_limit);
				if constexpr (use_eq) {
					const float low_stage = voice_flow::sat(low_current[lane] * y + low_previous[lane] * low_x1[lane] + low_feedback[lane] * low_y1[lane], voice_flow::eq_limit);
					low_x1[lane] = y;
					low_y1[lane] = low_stage;
					const float high_stage = voice_flow::sat(high_current[lane] * low_stage + high_previous[lane] * high_x1[lane] + high_feedback[lane] * high_y1[lane], voice_flow::eq_limit);
					high_x1[lane] = low_stage;
					high_y1[lane] = high_stage;
					y = high_stage;
				} else {
					entering_eq[lane] = y;
				}
				x[lane] = gain[lane] * y;
			}
		}

		for (int lane = 0; lane < lanes; lane++) {
			const int v = lane_voice[lane];
			fields.lp_low[v] = low[lane];
			fields.lp_band[v] = band[lane];
			if constexpr (use_high_pass) {
				fields.hp_low[v] = low2[lane];
				fields.hp_band[v] = band2[lane];
			}
			if constexpr (use_eq) {
				fields.eq_x1[v][0] = low_x1[lane];
				fields.eq_y1[v][0] = low_y1[lane];
				fields.eq_x1[v][1] = high_x1[lane];
				fields.eq_y1[v][1] = high_y1[lane];
			} else {
				// 9 章: 通らないあいだも x⁻¹ = y⁻¹ = いま段に入った値
				for (int stage = 0; stage < 2; stage++)
					fields.eq_x1[v][stage] = fields.eq_y1[v][stage] = entering_eq[lane];
			}
			float *output = fields.output[v];
			for (int s = 0; s < live[lane]; s++)
				output[s] = bundle.signal[s][lane];
			for (int s = live[lane]; s < n; s++)
				output[s] = 0.0f;
		}
	}

	voice_fields fields;
	uint64_t listed[2];   // 鳴っている声（と音程の EG が動いている声）。番号の順に回す
	uint64_t rendered[2]; // 直前の render で出力を作った声

	const uint8_t *rom = nullptr, *ram = nullptr;
	uint32_t rom_words = 0, ram_first_word = 0, ram_words = 0;
	uint32_t word_scratch[voice_flow::max_expanded / 2 + 2];

	static inline const float silent_output[max_block] = {};
};

} // namespace native_voice

#endif // S_MU2000_NATIVE_VOICE_BANK_H
