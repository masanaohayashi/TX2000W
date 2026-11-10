// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/ の文書だけから書いたマスター EQ・システムリバーブ・システムコーラス。
// 名前は文書のまま。信号の順は文書の「1 サンプルの順」と同じ結果になるようにブロックへまとめてある。
//
// ponytail: README の「静かな間は回さない」は入れていない。止めてよいだけで、再開のときの状態が文書に無いため。

#ifndef S_MU2000_NATIVE_FX_FLOW_MASTER_H
#define S_MU2000_NATIVE_FX_FLOW_MASTER_H

#include "fx_flow_parts.h"
#include "fx_params.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <vector>

namespace smu2000::native_fx {

class master_eq_flow {
public:
	void configure(const master_eq_params &p)
	{
		for (int band = 0; band < band_count; band++) {
			band_is_flat[band] = p.b0[band] == 1.0f && p.b1[band] == -p.a1[band] && p.b2[band] == -p.a2[band];
			coefficients[band] = { scaled_coefficient(p.b0[band]), scaled_coefficient(p.b1[band]), scaled_coefficient(p.b2[band]),
				scaled_coefficient(p.a1[band]), scaled_coefficient(p.a2[band]) };
		}
		every_band_is_flat = std::all_of(band_is_flat, band_is_flat + band_count, [](bool flat) { return flat; });
	}

	void reset()
	{
		for (band_state &state : bands)
			state = {};
	}

	void process(float *left, float *right, int n)
	{
		int32_t stereo[fx_flow::max_block][2];
		for (int s = 0; s < n; s++) {
			stereo[s][0] = to_word(left[s]);
			stereo[s][1] = to_word(right[s]);
		}
		for (int band = 0; band < band_count; band++) {
			if (band_is_flat[band])
				follow_flat_band(bands[band], stereo, n);
			else
				run_band(band, stereo, n);
		}
		// どの段も平らなら語にしない
		if (every_band_is_flat)
			return;
		for (int s = 0; s < n; s++) {
			left[s] = float(stereo[s][0]) * (1.0f / word_scale);
			right[s] = float(stereo[s][1]) * (1.0f / word_scale);
		}
	}

private:
	static constexpr int band_count = 5;
	static constexpr float word_scale = 8388608.0f;
	static constexpr int32_t word_max = 0x7fffff, word_min = -0x800000;
	static constexpr int64_t coefficient_one = 0x8000;

	struct band_coefficients { int32_t b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
	struct band_state {
		int32_t x_previous[2] = {};
		int32_t x_before_previous[2] = {};
		int32_t y_previous[2] = {};
		int32_t y_before_previous[2] = {};
	};

	static int32_t scaled_coefficient(float value) { return int32_t(std::lround(double(value) * double(coefficient_one))); }
	static int32_t to_word(float value)
	{
		return int32_t(std::clamp(value * word_scale, float(word_min), float(word_max)));
	}

	void run_band(int band, int32_t (*stereo)[2], int n)
	{
		const band_coefficients c = coefficients[band];
		band_state state = bands[band];
		for (int s = 0; s < n; s++) {
			for (int ch = 0; ch < 2; ch++) {
				const int32_t x = stereo[s][ch];
				const int64_t sum = int64_t(c.b0) * x + int64_t(c.b1) * state.x_previous[ch] + int64_t(c.b2) * state.x_before_previous[ch]
					+ int64_t(c.a1) * state.y_previous[ch] + int64_t(c.a2) * state.y_before_previous[ch];
				// 整数の割り算は 0 の側へ切り捨てる
				const int32_t y = int32_t(std::clamp<int64_t>(sum / coefficient_one, word_min, word_max));
				state.x_before_previous[ch] = state.x_previous[ch];
				state.x_previous[ch] = x;
				state.y_before_previous[ch] = state.y_previous[ch];
				state.y_previous[ch] = y;
				stereo[s][ch] = y;
			}
		}
		bands[band] = state;
	}

	// 平らな段は y = x。計算はしないが、平らでなくなったときに続くよう状態だけ合わせる
	static void follow_flat_band(band_state &state, const int32_t (*stereo)[2], int n)
	{
		for (int ch = 0; ch < 2; ch++) {
			const int32_t before_previous = n >= 2 ? stereo[n - 2][ch] : state.x_previous[ch];
			state.x_before_previous[ch] = state.y_before_previous[ch] = before_previous;
			state.x_previous[ch] = state.y_previous[ch] = stereo[n - 1][ch];
		}
	}

	band_coefficients coefficients[band_count];
	bool band_is_flat[band_count] = {};
	bool every_band_is_flat = false;
	band_state bands[band_count];
};

class reverb_flow {
public:
	reverb_flow() = default;

	void configure(const reverb_params &p)
	{
		parameters = p;
		// この長さまでは、読む値がどれも前の区切りまでに書かれている。遅れ 0 は使わない線で、書いた値を読まない。
		int shortest = fx_flow::max_block;
		const auto limit_by = [&shortest](int delay) {
			if (delay >= 1)
				shortest = std::min(shortest, delay);
		};
		for (int ch = 0; ch < 2; ch++) {
			if (p.predelay_feedback[ch] != 0.0f)
				limit_by(p.late_send_delay[ch]);
			limit_by(p.diffuser_delay[ch]);
			limit_by(p.out_allpass_delay[ch][0]);
			limit_by(p.out_allpass_delay[ch][1]);
			for (int i = 0; i < comb_count; i++) {
				if (p.comb_delay[i] != 0 && p.comb_out_gain[ch][i] != 0.0f)
					limit_by(p.comb_out_delay[ch][i]);
			}
		}
		for (int i = 0; i < comb_count; i++)
			limit_by(p.comb_delay[i]);
		run_limit = shortest;
	}

	void reset()
	{
		for (auto &line : early_line)
			line.clear();
		for (auto &line : diffuser_line)
			line.clear();
		for (auto &line : comb_line)
			line.clear();
		for (auto &stages : out_allpass_line) {
			for (auto &line : stages)
				line.clear();
		}
		state = {};
	}

	void process(const float *send_left, const float *send_right, float *return_left, float *return_right, int n)
	{
		float low[fx_flow::max_block][2];
		run_inlet_filters(send_left, send_right, low, n);
		for (int done = 0; done < n;) {
			const int m = std::min(n - done, run_limit);
			process_run(low + done, return_left + done, return_right + done, m);
			done += m;
		}
	}

private:
	static constexpr int comb_count = 6;

	struct filter_state {
		uint32_t position = 0;
		float x_previous[2] = {};
		float high_previous[2] = {};
		float low_previous[2] = {};
		float mixed_previous = 0.0f;
		float filtered_previous = 0.0f;
		float r_previous[comb_count] = {};
		float damped_previous[comb_count] = {};
	};

	// 入口のフィルタは遅延線の値を使わないので、区切らずブロック全部を先に流す。
	void run_inlet_filters(const float *send_left, const float *send_right, float (*low)[2], int n)
	{
		const reverb_params &p = parameters;
		const float send_gain[2] = {p.send_left_to_left, p.send_right_to_right};
		for (int s = 0; s < n; s++) {
			low[s][0] = send_left[s];
			low[s][1] = send_right[s];
		}
		for (int s = 0; s < n; s++) {
			for (int ch = 0; ch < 2; ch++) {
				const float x = fx_flow::sat(send_gain[ch] * low[s][ch]);
				const float high = fx_flow::sat(p.highpass_current[ch] * x + p.highpass_previous[ch] * state.x_previous[ch]
					+ p.highpass_feedback[ch] * state.high_previous[ch]);
				const float lowered = fx_flow::sat(p.lowpass_current[ch] * high + p.lowpass_previous[ch] * state.high_previous[ch]
					+ p.lowpass_feedback[ch] * state.low_previous[ch]);
				state.x_previous[ch] = x;
				state.high_previous[ch] = high;
				state.low_previous[ch] = lowered;
				low[s][ch] = lowered;
			}
		}
	}

	void process_run(const float (*low)[2], float *return_left, float *return_right, int m)
	{
		float early[2][fx_flow::max_block] = {};
		float to_late[2][fx_flow::max_block] = {};
		float diffused[fx_flow::max_block];
		run_predelay_lines(low, early, to_late, m);
		run_diffuser(to_late, diffused, m);
		run_outlet(0, early[0], return_left, m);
		run_outlet(1, early[1], return_right, m);
		run_combs(diffused, m);
		state.position += uint32_t(m);
	}

	// 帰還があれば to_late を先に読む（区切りは late_send_delay 以下）。
	// 帰還が無ければ書く値は読み出しによらないので、書いてから読めばブロックより短い遅れでも 1 サンプルずつと同じになる。
	// early_delay は書いたあとに読むので、4 サンプルでも区切らなくてよい。
	void run_predelay_lines(const float (*low)[2], float (*early)[fx_flow::max_block], float (*to_late)[fx_flow::max_block], int m)
	{
		const reverb_params &p = parameters;
		float written[fx_flow::max_block];
		for (int ch = 0; ch < 2; ch++) {
			const float feedback = p.predelay_feedback[ch];
			if (feedback != 0.0f)
				early_line[ch].read(state.position, p.late_send_delay[ch], to_late[ch], m);
			for (int s = 0; s < m; s++)
				written[s] = fx_flow::word(fx_flow::sat(low[s][ch] + feedback * to_late[ch][s]));
			early_line[ch].write(state.position, written, m);
			if (feedback == 0.0f)
				early_line[ch].read(state.position, p.late_send_delay[ch], to_late[ch], m);
			for (int i = 0; i < 4; i++)
				early_line[ch].add_tap(state.position, p.early_delay[ch][i], p.early_gain[ch][i], early[ch], m);
			for (int s = 0; s < m; s++)
				early[ch][s] = fx_flow::sat(early[ch][s]);
		}
	}

	void run_diffuser(const float (*to_late)[fx_flow::max_block], float *diffused, int m)
	{
		const reverb_params &p = parameters;
		const float diffuse_left = p.diffuse_left;
		const float diffuse_right = p.diffuse_right;
		const float filter_current = p.diffuse_filter_current;
		const float filter_previous = p.diffuse_filter_previous;
		const float filter_feedback = p.diffuse_filter_feedback;
		float mixed[fx_flow::max_block];
		for (int s = 0; s < m; s++)
			mixed[s] = fx_flow::sat(diffuse_left * to_late[0][s] + diffuse_right * to_late[1][s]);
		float mixed_previous = state.mixed_previous;
		float filtered_previous = state.filtered_previous;
		for (int s = 0; s < m; s++) {
			filtered_previous = fx_flow::sat(filter_current * mixed[s] + filter_previous * mixed_previous + filter_feedback * filtered_previous);
			mixed_previous = mixed[s];
			diffused[s] = filtered_previous;
		}
		state.mixed_previous = mixed_previous;
		state.filtered_previous = filtered_previous;
		for (int stage = 0; stage < 2; stage++) {
			fx_flow::allpass_run(diffuser_line[stage], state.position, p.diffuser_delay[stage], p.diffuser_input[stage],
				p.diffuser_feedback[stage], p.diffuser_delayed_out[stage], p.diffuser_direct_out[stage], diffused, m);
		}
	}

	void run_outlet(int ch, const float *early, float *return_out, int m)
	{
		const reverb_params &p = parameters;
		float v[fx_flow::max_block];
		std::copy_n(early, m, v);
		for (int i = 0; i < comb_count; i++) {
			if (p.comb_delay[i] != 0)
				comb_line[i].add_tap(state.position, p.comb_out_delay[ch][i], p.comb_out_gain[ch][i], v, m);
			else if (p.comb_out_delay[ch][i] == 0)     // 文書の「使わない櫛形の取り出し」
				comb_line[i - 1].add_tap(state.position, p.unused_comb_distance, p.comb_out_gain[ch][i], v, m);
		}
		for (int s = 0; s < m; s++)
			v[s] = fx_flow::sat(v[s]);
		for (int stage = 0; stage < 2; stage++) {
			fx_flow::allpass_run(out_allpass_line[ch][stage], state.position, p.out_allpass_delay[ch][stage],
				p.out_allpass_input[ch][stage], p.out_allpass_feedback[ch][stage], p.out_allpass_delayed_out[ch][stage],
				p.out_allpass_direct_out[ch][stage], v, m);
		}
		const float return_gain = p.return_gain[ch];
		for (int s = 0; s < m; s++)
			return_out[s] = fx_flow::sat(return_gain * v[s]);
	}

	// 帰還はサンプル単位なので、サンプルの中で櫛形 6 本を並列に流す。
	void run_combs(const float *diffused, int m)
	{
		const reverb_params &p = parameters;
		float r[fx_flow::max_block][comb_count] = {};
		float written[fx_flow::max_block][comb_count];
		for (int i = 0; i < comb_count; i++) {
			if (p.comb_delay[i] == 0)
				continue;
			const uint32_t oldest = state.position - uint32_t(p.comb_delay[i]);
			for (int s = 0; s < m; s++)
				r[s][i] = comb_line[i].at(oldest + uint32_t(s));
		}
		for (int s = 0; s < m; s++) {
			const float diffused_now = diffused[s];
			for (int i = 0; i < comb_count; i++) {
				const float damped = fx_flow::sat(p.comb_damp_current[i] * r[s][i] + p.comb_damp_previous[i] * state.r_previous[i]
					+ p.comb_damp_feedback[i] * state.damped_previous[i]);
				state.r_previous[i] = r[s][i];
				state.damped_previous[i] = damped;
				written[s][i] = fx_flow::word(fx_flow::sat(p.comb_input[i] * diffused_now + damped));
			}
		}
		for (int i = 0; i < comb_count; i++) {
			if (p.comb_delay[i] == 0)
				continue;
			for (int s = 0; s < m; s++)
				comb_line[i].put(state.position + uint32_t(s), written[s][i]);
		}
	}

	reverb_params parameters;
	int run_limit = fx_flow::max_block;
	filter_state state;
	// 長さは表の一番長い遅れ（early 8276・late 8758・diffuser 1466・comb 4717・out allpass 1071）が入る 2 の冪。
	fx_flow::delay_line<16384> early_line[2];
	fx_flow::delay_line<2048> diffuser_line[2];
	fx_flow::delay_line<8192> comb_line[comb_count];
	fx_flow::delay_line<2048> out_allpass_line[2][2];
};

class chorus_flow {
public:
	chorus_flow() = default;

	void configure(const chorus_params &p)
	{
		parameters = p;
		stereo = p.stereo_input != 0;
		voice_used[1] = p.wet_gain[0][1] != 0.0f || p.wet_gain[1][2] != 0.0f;
		voice_used[2] = p.wet_gain[0][2] != 0.0f || p.wet_gain[1][1] != 0.0f;
		int shortest = fx_flow::max_block;
		int longest = 0;
		for (int i = 0; i < 4; i++) {
			if (voice_used[i]) {
				shortest = std::min(shortest, p.voice_base_delay[i]);
				longest = std::max(longest, p.voice_base_delay[i] + int(p.voice_depth[i]) + 1);
			}
		}
		// 書いてから読むと、輪を 1 周しかけている読み出しがこのブロックで書いた値に当たるので、そのときも区切る。
		const bool feedback = p.left_line_feedback != 0.0f || (stereo && p.right_line_feedback != 0.0f);
		const bool split = feedback || longest + fx_flow::max_block > line_length;
		run_limit = split ? std::max(shortest, 1) : fx_flow::max_block;
		write_before_reading = !split && shortest < fx_flow::max_block;
	}

	void reset()
	{
		left_line.clear();
		right_line.clear();
		state = {};
	}

	void process(const float *send_left, const float *send_right, const float *const lfo[4], float *return_left, float *return_right, int n)
	{
		float left_input[fx_flow::max_block];
		float right_input[fx_flow::max_block];
		run_shelves(send_left, send_right, left_input, right_input, n);

		// voice_3_delayed[s] が出口と帰還に出る声 3。[s + 1] へ補間した値を入れるのが図の z⁻¹。
		float voice[3][fx_flow::max_block] = {};
		float voice_3_delayed[fx_flow::max_block + 1] = {};
		voice_3_delayed[0] = state.voice_3_previous;
		for (int done = 0; done < n;) {
			const int m = std::min(n - done, run_limit);
			if (write_before_reading) {
				write_lines(left_input + done, right_input + done, voice[0] + done, voice_3_delayed + done, m);
				read_voices(lfo, done, voice, voice_3_delayed, m);
			} else {
				read_voices(lfo, done, voice, voice_3_delayed, m);
				write_lines(left_input + done, right_input + done, voice[0] + done, voice_3_delayed + done, m);
			}
			state.position += uint32_t(m);
			done += m;
		}
		state.voice_3_previous = voice_3_delayed[n];

		const auto &wet_gain = parameters.wet_gain;
		const float left_0 = wet_gain[0][0], left_1 = wet_gain[0][1], left_2 = wet_gain[0][2];
		const float right_0 = wet_gain[1][0], right_1 = wet_gain[1][1], right_2 = wet_gain[1][2];
		for (int s = 0; s < n; s++) {
			return_left[s] = fx_flow::sat(left_0 * voice[0][s] + left_1 * voice[1][s] + left_2 * voice[2][s]);
			return_right[s] = fx_flow::sat(right_0 * voice_3_delayed[s] + right_1 * voice[2][s] + right_2 * voice[1][s]);
		}
	}

private:
	static constexpr int line_length = 16384;

	struct filter_state {
		uint32_t position = 0;
		float voice_3_previous = 0.0f;
		float low_shelf_x_previous[2] = {};
		float low_shelf_y_previous[2] = {};
		float high_shelf_x_previous[2] = {};
		float high_shelf_y_previous[2] = {};
	};

	// 棚は遅延線の値を使わないので、区切らずブロック全部を先に流す。線へ入れる入力まで作る。
	void run_shelves(const float *send_left, const float *send_right, float *left_input, float *right_input, int n)
	{
		const chorus_params &p = parameters;
		float shelved[fx_flow::max_block][2];
		for (int s = 0; s < n; s++) {
			shelved[s][0] = send_left[s];
			shelved[s][1] = send_right[s];
		}
		for (int s = 0; s < n; s++) {
			for (int ch = 0; ch < 2; ch++) {
				const float x = fx_flow::sat(p.input_gain[ch] * shelved[s][ch]);
				const float low_shelf = fx_flow::sat(p.low_shelf_current[ch] * x + p.low_shelf_previous[ch] * state.low_shelf_x_previous[ch]
					+ p.low_shelf_feedback[ch] * state.low_shelf_y_previous[ch]);
				const float high_shelf = fx_flow::sat(p.high_shelf_current[ch] * low_shelf + p.high_shelf_previous[ch] * state.high_shelf_x_previous[ch]
					+ p.high_shelf_feedback[ch] * state.high_shelf_y_previous[ch]);
				state.low_shelf_x_previous[ch] = x;
				state.low_shelf_y_previous[ch] = low_shelf;
				state.high_shelf_x_previous[ch] = low_shelf;
				state.high_shelf_y_previous[ch] = high_shelf;
				shelved[s][ch] = high_shelf;
			}
		}
		const float left_line_from_left = p.left_line_from_left;
		const float left_line_from_right = p.left_line_from_right;
		const float right_line_from_right = p.right_line_from_right;
		for (int s = 0; s < n; s++) {
			left_input[s] = left_line_from_left * shelved[s][0] + left_line_from_right * shelved[s][1];
			right_input[s] = right_line_from_right * shelved[s][1];
		}
	}

	void read_voices(const float *const lfo[4], int done, float (*voice)[fx_flow::max_block], float *voice_3_delayed, int m) const
	{
		const auto &line_s = stereo ? right_line : left_line;
		read_voice(left_line, 0, lfo[0] + done, voice[0] + done, m);
		if (voice_used[1])
			read_voice(left_line, 1, lfo[1] + done, voice[1] + done, m);
		if (voice_used[2])
			read_voice(line_s, 2, lfo[2] + done, voice[2] + done, m);
		read_voice(line_s, 3, lfo[3] + done, voice_3_delayed + done + 1, m);
	}

	void read_voice(const fx_flow::delay_line<line_length> &line, int i, const float *lfo, float *out, int m) const
	{
		const float voice_depth = parameters.voice_depth[i];
		const uint32_t newest = state.position - uint32_t(parameters.voice_base_delay[i]);
		float fraction[fx_flow::max_block];
		int32_t whole[fx_flow::max_block];
		float a[fx_flow::max_block];
		float b[fx_flow::max_block];
		for (int s = 0; s < m; s++) {
			const float moved = voice_depth * lfo[s];
			whole[s] = int32_t(moved); // moved は 0 以上なので floor と同じ
			fraction[s] = moved - float(whole[s]);
		}
		for (int s = 0; s < m; s++) {
			const uint32_t index = newest + uint32_t(s) - uint32_t(whole[s]);
			a[s] = line.at(index);
			b[s] = line.at(index - 1);
		}
		for (int s = 0; s < m; s++)
			out[s] = fx_flow::sat(a[s] + fraction[s] * (b[s] - a[s]));
	}

	void write_lines(const float *left_input, const float *right_input, const float *voice_0, const float *voice_3, int m)
	{
		float written[fx_flow::max_block];
		const float left_line_feedback = parameters.left_line_feedback;
		for (int s = 0; s < m; s++)
			written[s] = fx_flow::word(fx_flow::sat(left_input[s] + left_line_feedback * voice_0[s]));
		left_line.write(state.position, written, m);
		if (!stereo)
			return;
		const float right_line_feedback = parameters.right_line_feedback;
		for (int s = 0; s < m; s++)
			written[s] = fx_flow::word(fx_flow::sat(right_input[s] + right_line_feedback * voice_3[s]));
		right_line.write(state.position, written, m);
	}

	chorus_params parameters;
	bool stereo = false;
	bool voice_used[4] = {true, false, false, true};
	bool write_before_reading = false;
	int run_limit = fx_flow::max_block;
	filter_state state;
	fx_flow::delay_line<line_length> left_line;
	fx_flow::delay_line<line_length> right_line;
};

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_MASTER_H
