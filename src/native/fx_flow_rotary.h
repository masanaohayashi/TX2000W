// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/rotary.md, ring_mod.md, low_reso.md, turntable.md のフローから書いたエフェクト。

#ifndef S_MU2000_NATIVE_FX_FLOW_ROTARY_H
#define S_MU2000_NATIVE_FX_FLOW_ROTARY_H

#include "fx_flow_parts.h"
#include "fx_params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smu2000::native_fx {

namespace rotary_detail {

using fx_flow::max_block;
using fx_flow::sat;
using fx_flow::word;

// 遅延線の語に使う長さ（2 の冪）。揺れを含めた最長の遅れより大きい。
using long_line = fx_flow::delay_line<65536>;
using mid_line = fx_flow::delay_line<8192>;
using tiny_line = fx_flow::delay_line<128>;

// 1 次フィルタ: y = [sat] (current × x + previous × x⁻¹ + feedback × y⁻¹)
struct first_order {
	float current = 0, previous = 0, feedback = 0;
};
struct first_state {
	float x1 = 0, y1 = 0;
};

// 2 次フィルタ: y = [sat] (current × x + x1 × x⁻¹ + x2 × x⁻² + y1 × y⁻¹ + y2 × y⁻²)
struct second_order {
	float current = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};
struct second_state {
	float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

inline void run(const first_order &c, first_state &st, float *x, int n)
{
	const first_order k = c;
	float x1 = st.x1, y1 = st.y1;
	for (int s = 0; s < n; s++) {
		const float in = x[s];
		const float y = sat(k.current * in + k.previous * x1 + k.feedback * y1);
		x1 = in;
		y1 = y;
		x[s] = y;
	}
	st.x1 = x1;
	st.y1 = y1;
}

inline void run(const second_order &c, second_state &st, float *x, int n)
{
	const second_order k = c;
	float x1 = st.x1, x2 = st.x2, y1 = st.y1, y2 = st.y2;
	for (int s = 0; s < n; s++) {
		const float in = x[s];
		const float y = sat(k.current * in + k.x1 * x1 + k.x2 * x2 + k.y1 * y1 + k.y2 * y2);
		x2 = x1;
		x1 = in;
		y2 = y1;
		y1 = y;
		x[s] = y;
	}
	st.x1 = x1;
	st.x2 = x2;
	st.y1 = y1;
	st.y2 = y2;
}

// 1 サンプル遅らせる（z⁻¹）。out[s] は in[s-1]、out[0] は state。
inline void unit_delay(float &state, const float *in, float *out, int n)
{
	out[0] = state;
	for (int s = 1; s < n; s++)
		out[s] = in[s - 1];
	state = in[n - 1];
}

// LFO を整える
struct shaper {
	float offset = -1, input_gain = 1, linear = 0.5f, cubic = 0, center = 0.5f;
	bool identity = true;
};
inline shaper make_shaper(float offset, float input_gain, float linear, float cubic, float center)
{
	shaper r{offset, input_gain, linear, cubic, center, false};
	r.identity = offset == -1.0f && input_gain == 1.0f && linear == 0.5f && cubic == 0.0f && center == 0.5f;
	return r;
}
inline void shape(const shaper &c, const float *lfo, float *out, int n)
{
	if (c.identity) {
		std::copy_n(lfo, n, out);
		return;
	}
	const shaper k = c;
	for (int s = 0; s < n; s++) {
		const float x = sat(k.input_gain * sat(k.offset + 2.0f * lfo[s]));
		out[s] = fx_flow::positive(k.center + k.cubic * x * x * x + sat(k.linear * x));
	}
}

// 揺らす読み出し。swing[s] = 揺れ幅 × LFO（0 以上）。サンプル s は position + s に書く語の手前を読む。
template <uint32_t length>
inline void read_swing(const fx_flow::delay_line<length> &line, uint32_t position, int base, const float *swing,
	float *out, int n)
{
	int32_t whole[max_block];
	float fraction[max_block];
	for (int s = 0; s < n; s++) {
		const float w = std::floor(swing[s]);
		whole[s] = int32_t(w);
		fraction[s] = swing[s] - w;
	}
	for (int s = 0; s < n; s++) {
		const uint32_t a_index = position + uint32_t(s) - uint32_t(base + whole[s]);
		const float a = line.at(a_index);
		const float b = line.at(a_index - 1u);
		out[s] = sat(a + fraction[s] * (b - a));
	}
}

// 揺れが 0 の読み出し（端数の計算が要らない）
template <uint32_t length>
inline void read_fixed(const fx_flow::delay_line<length> &line, uint32_t position, int base, float *out, int n)
{
	line.read(position, base, out, n);
	for (int s = 0; s < n; s++)
		out[s] = sat(out[s]);
}

// 揺れ幅が 0 なら固定の読み出し、そうでなければ depth × amount[s] で揺らす
template <uint32_t length>
inline void read_voice(const fx_flow::delay_line<length> &line, uint32_t position, int base, float depth,
	const float *amount, float *out, int n)
{
	if (depth == 0.0f) {
		read_fixed(line, position, base, out, n);
		return;
	}
	float swing[max_block];
	for (int s = 0; s < n; s++)
		swing[s] = depth * amount[s];
	read_swing(line, position, base, swing, out, n);
}

} // namespace rotary_detail

// ---------------------------------------------------------------------------------------------------------------
// ROTARY SP、TREMOLO、AUTO PAN、2WAY ROTRY、AMBIENCE
// ---------------------------------------------------------------------------------------------------------------
class rotary_flow {
public:
	rotary_flow() { reset(); }

	void configure(const rotary_params &p)
	{
		using namespace rotary_detail;
		p_ = p;
		for (int ch = 0; ch < 2; ch++) {
			low_[ch] = {p.low_shelf_current[ch], p.low_shelf_previous[ch], p.low_shelf_feedback[ch]};
			mid_[ch] = {p.mid_current[ch], p.mid_x1[ch], p.mid_x2[ch], p.mid_y1[ch], p.mid_y2[ch]};
			high_[ch] = {p.high_shelf_current[ch], p.high_shelf_previous[ch], p.high_shelf_feedback[ch]};
		}
		for (int i = 0; i < 4; i++)
			shaper_[i] = make_shaper(p.shaper_lfo_offset[i], p.shaper_input_gain[i], p.shaper_linear[i],
				p.shaper_cubic[i], p.shaper_center[i]);
		// 出口へ混ぜない声と、遅れが 0（使わない）の声は計算しない
		const bool used[4] = {p.wet_left[0] != 0.0f, p.wet_left[1] != 0.0f || p.wet_right[1] != 0.0f,
			p.wet_left[2] != 0.0f || p.wet_right[2] != 0.0f, p.wet_right[3] != 0.0f};
		for (int i = 0; i < 4; i++)
			active_[i] = used[i] && p.voice_base_delay[i] > 0;
	}

	void reset()
	{
		using namespace rotary_detail;
		first_.clear();
		second_.clear();
		for (int ch = 0; ch < 2; ch++) {
			low_state_[ch] = {};
			mid_state_[ch] = {};
			high_state_[ch] = {};
		}
		for (float &v : shaped_)
			v = 0.0f;
		position_ = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace rotary_detail;
		(void)table;
		const rotary_params &p = p_;

		// 入力 → EQ
		float a[max_block], b[max_block];
		{
			const float first_from_left = p.input_from_left[0], first_from_right = p.input_from_right[0];
			const float second_from_left = p.input_from_left[1], second_from_right = p.input_from_right[1];
			for (int s = 0; s < n; s++) {
				a[s] = sat(first_from_left * in_left[s] + first_from_right * in_right[s]);
				b[s] = sat(second_from_left * in_left[s] + second_from_right * in_right[s]);
			}
		}
		float *const eq_signal[2] = {a, b};
		for (int ch = 0; ch < 2; ch++) {
			run(low_[ch], low_state_[ch], eq_signal[ch], n);
			run(mid_[ch], mid_state_[ch], eq_signal[ch], n);
			run(high_[ch], high_state_[ch], eq_signal[ch], n);
		}

		// 帰還が無いので、線へ先にブロック全部を書く
		{
			float first_words[max_block], second_words[max_block];
			const float fl = p.first_line_from_left, fr = p.first_line_from_right, sr = p.second_line_from_right;
			for (int s = 0; s < n; s++) {
				first_words[s] = word(sat(fl * a[s] + fr * b[s]));
				second_words[s] = word(sat(sr * b[s]));
			}
			first_.write(position_, first_words, n);
			second_.write(position_, second_words, n);
		}

		// 整えた LFO（y[i][s] はサンプル s の 1 サンプル前の値）
		static constexpr int lfo_of_voice[4] = {0, 2, 3, 1};
		float y[4][max_block + 1];
		for (int i = 0; i < 4; i++) {
			y[i][0] = shaped_[i];
			shape(shaper_[i], lfo[lfo_of_voice[i]], y[i] + 1, n);
			shaped_[i] = y[i][n];
		}

		// 声
		float voice[4][max_block];
		for (int i = 0; i < 4; i++) {
			if (!active_[i]) {
				std::fill_n(voice[i], n, 0.0f);
				continue;
			}
			float v[max_block];
			const int base = p.voice_base_delay[i];
			if (p.voice_on_second_line[i])
				read_voice(second_, position_, base, p.voice_depth[i], y[i], v, n);
			else
				read_voice(first_, position_, base, p.voice_depth[i], y[i], v, n);
			const float am = p.voice_am_depth[i];
			for (int s = 0; s < n; s++)
				voice[i][s] = sat(v[s] + am * y[i][s] * v[s]);
		}

		// 濡れた音と出口
		const float wl0 = p.wet_left[0], wl1 = p.wet_left[1], wl2 = p.wet_left[2];
		const float wr1 = p.wet_right[1], wr2 = p.wet_right[2], wr3 = p.wet_right[3];
		const float dry_l = p.dry_gain[0], dry_r = p.dry_gain[1], wet_l = p.wet_gain[0], wet_r = p.wet_gain[1];
		for (int s = 0; s < n; s++) {
			const float wet_left = sat(wl0 * voice[0][s] + wl1 * voice[1][s] + wl2 * voice[2][s]);
			const float wet_right = sat(wr1 * voice[1][s] + wr2 * voice[2][s] + wr3 * voice[3][s]);
			out_left[s] = sat(dry_l * a[s] + wet_l * wet_left);
			out_right[s] = sat(dry_r * b[s] + wet_r * wet_right);
		}
		position_ += uint32_t(n);
	}

private:
	rotary_params p_;
	rotary_detail::first_order low_[2], high_[2];
	rotary_detail::second_order mid_[2];
	rotary_detail::shaper shaper_[4];
	bool active_[4] = {};
	rotary_detail::long_line first_, second_;
	rotary_detail::first_state low_state_[2], high_state_[2];
	rotary_detail::second_state mid_state_[2];
	float shaped_[4] = {};
	uint32_t position_ = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// DUAL ROTR 1/2
// ---------------------------------------------------------------------------------------------------------------
class dual_rotary_flow {
public:
	dual_rotary_flow() { reset(); }

	void configure(const dual_rotary_params &p)
	{
		using namespace rotary_detail;
		p_ = p;
		rotor_first_ = {p.rotor_first_current, p.rotor_first_previous, p.rotor_first_feedback};
		rotor_second_ = {p.rotor_second_current, p.rotor_second_previous, p.rotor_second_feedback};
		rotor_third_ = {p.rotor_third_current, p.rotor_third_previous, p.rotor_third_feedback};
		low_first_ = {p.horn_first_current, p.horn_first_x1, p.horn_first_x2, p.horn_first_y1,
			p.horn_first_y2};
		low_second_ = {p.horn_second_current, p.horn_second_x1, p.horn_second_x2, p.horn_second_y1,
			p.horn_second_y2};
		low_third_ = {p.horn_third_current, p.horn_third_previous, p.horn_third_feedback};
	}

	void reset()
	{
		high_line_.clear();
		low_line_.clear();
		rotor_first_state_ = rotor_second_state_ = rotor_third_state_ = {};
		low_first_state_ = low_second_state_ = {};
		low_third_state_ = {};
		for (auto &h : lfo_history_)
			h[0] = h[1] = 0.0f;
		for (float &v : position_state_)
			v = 0.0f;
		for (float &v : voice_state_)
			v = 0.0f;
		position_ = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace rotary_detail;
		(void)table;
		const dual_rotary_params &p = p_;

		// 入力 → 高い帯、低い帯 → 線へ（帰還が無いので先にブロック全部を書く）
		float mono[max_block];
		{
			const float l = p.input_from_left, r = p.input_from_right;
			for (int s = 0; s < n; s++)
				mono[s] = sat(l * in_left[s] + r * in_right[s]);
		}
		{
			float band[max_block], words[max_block];
			std::copy_n(mono, n, band);
			run(rotor_first_, rotor_first_state_, band, n);
			run(rotor_second_, rotor_second_state_, band, n);
			run(rotor_third_, rotor_third_state_, band, n);
			const float g = p.high_line_input;
			for (int s = 0; s < n; s++)
				words[s] = word(sat(g * band[s]));
			high_line_.write(position_, words, n);

			std::copy_n(mono, n, band);
			run(low_first_, low_first_state_, band, n);
			run(low_second_, low_second_state_, band, n);
			run(low_third_, low_third_state_, band, n);
			const float h = p.low_line_input;
			for (int s = 0; s < n; s++)
				words[s] = word(sat(h * band[s]));
			low_line_.write(position_, words, n);
		}

		// LFO の履歴。e[l][2 + s - k] が LFO l の k サンプル前
		float e[6][max_block + 2];
		for (int l = 0; l < 6; l++) {
			e[l][0] = lfo_history_[l][0];
			e[l][1] = lfo_history_[l][1];
			std::copy_n(lfo[l], n, e[l] + 2);
			lfo_history_[l][0] = e[l][n];
			lfo_history_[l][1] = e[l][n + 1];
		}
		const float *const now[6] = {e[0] + 2, e[1] + 2, e[2] + 2, e[3] + 2, e[4] + 2, e[5] + 2};
		const float *const back1[6] = {e[0] + 1, e[1] + 1, e[2] + 1, e[3] + 1, e[4] + 1, e[5] + 1};
		const float *const back2[6] = {e[0], e[1], e[2], e[3], e[4], e[5]};

		const float sa = p.invert_slow_a, sb = p.invert_slow_b, fa = p.invert_fast_a, fb = p.invert_fast_b;
		const float *k = p.position_scale;

		// position[i]（P[i][s] はサンプル s の 1 サンプル前の値）と am[i]
		float P[8][max_block + 1];
		float am[8][max_block];
		for (int i = 0; i < 8; i++)
			P[i][0] = position_state_[i];
		for (int s = 0; s < n; s++) {
			// 読み出しが P[i][s] を使うので、ここでは文書の表より 1 サンプル新しい LFO から作る
			const float l3 = back1[3][s], l4 = back1[4][s], l5 = back1[5][s];
			P[0][s + 1] = k[0] * l3;
			P[1][s + 1] = k[1] * (sb + sa * l3) + k[0] * l3;
			P[2][s + 1] = k[2] * l4;
			P[3][s + 1] = k[3] * l5 + k[2] * l4;
			P[4][s + 1] = k[4] * now[0][s];
			P[5][s + 1] = k[5] * (fb + fa * now[0][s]);
			P[6][s + 1] = k[6] * now[1][s];
			P[7][s + 1] = k[7] * now[2][s];

			am[0][s] = back1[3][s];
			am[1][s] = sb + sa * back1[3][s];
			am[2][s] = now[4][s];
			am[3][s] = now[5][s];
			am[4][s] = now[0][s];
			am[5][s] = fb + fa * now[0][s];
			am[6][s] = now[1][s];
			am[7][s] = now[2][s];
		}
		for (int i = 0; i < 8; i++)
			position_state_[i] = P[i][n];

		// 声（V[i][s] はサンプル s の 1 サンプル前の声）
		float V[8][max_block + 1];
		for (int i = 0; i < 8; i++) {
			float v[max_block];
			const int base = std::max(p.voice_base_delay[i], 1);
			if (i < 4)
				read_voice(high_line_, position_, base, p.voice_depth[i], P[i], v, n);
			else
				read_voice(low_line_, position_, base, p.voice_depth[i], P[i], v, n);
			const float depth = p.voice_am_depth[i];
			V[i][0] = voice_state_[i];
			for (int s = 0; s < n; s++)
				V[i][s + 1] = v[s] + depth * am[i][s] * v[s];
			voice_state_[i] = V[i][n];
		}

		const float *m = p.left_mix, *q = p.right_mix;
		const float gl = p.out_gain[0], gr = p.out_gain[1];
		for (int s = 0; s < n; s++) {
			const float wet_left =
				sat(m[0] * V[0][s] + m[1] * V[1][s] + m[2] * V[2][s] + m[3] * V[4][s] + m[4] * V[5][s] + m[5] * V[6][s]);
			const float wet_right =
				sat(q[0] * V[0][s] + q[1] * V[1][s] + q[2] * V[3][s] + q[3] * V[4][s] + q[4] * V[5][s] + q[5] * V[7][s]);
			out_left[s] = sat(gl * wet_left);
			out_right[s] = sat(gr * wet_right);
		}
		position_ += uint32_t(n);
	}

private:
	dual_rotary_params p_;
	rotary_detail::first_order rotor_first_, rotor_second_, rotor_third_, low_third_;
	rotary_detail::second_order low_first_, low_second_;
	rotary_detail::mid_line high_line_, low_line_;
	rotary_detail::first_state rotor_first_state_, rotor_second_state_, rotor_third_state_, low_third_state_;
	rotary_detail::second_state low_first_state_, low_second_state_;
	float lfo_history_[6][2] = {};
	float position_state_[8] = {};
	float voice_state_[8] = {};
	uint32_t position_ = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// DT/OD/AMP +RTRY、+2RTRY
// ---------------------------------------------------------------------------------------------------------------
class distortion_rotary_flow {
public:
	distortion_rotary_flow() { reset(); }

	void configure(const distortion_rotary_params &p)
	{
		using namespace rotary_detail;
		p_ = p;
		pre_first_ = {p.pre_first_current, p.pre_first_previous, p.pre_first_feedback};
		pre_second_ = {p.pre_second_current, p.pre_second_previous, p.pre_second_feedback};
		post_lowpass_ = {p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1,
			p.post_lowpass_y2};
		post_mid_ = {p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2};
		post_low_ = {p.post_low_current, p.post_low_x1, p.post_low_x2, p.post_low_y1, p.post_low_y2};
		first_band_a_ = {p.first_band_a_current, p.first_band_a_x1, p.first_band_a_x2, p.first_band_a_y1,
			p.first_band_a_y2};
		first_band_b_ = {p.first_band_b_current, p.first_band_b_x1, p.first_band_b_x2, p.first_band_b_y1,
			p.first_band_b_y2};
		second_band_a_ = {p.second_band_a_current, p.second_band_a_x1, p.second_band_a_x2, p.second_band_a_y1,
			p.second_band_a_y2};
		second_band_b_ = {p.second_band_b_current, p.second_band_b_x1, p.second_band_b_x2, p.second_band_b_y1,
			p.second_band_b_y2};
	}

	void reset()
	{
		first_.clear();
		second_.clear();
		pre_first_state_ = pre_second_state_ = {};
		post_lowpass_state_ = post_mid_state_ = post_low_state_ = {};
		first_band_a_state_ = first_band_b_state_ = second_band_a_state_ = second_band_b_state_ = {};
		post_previous_ = 0.0f;
		for (float &v : lfo_previous_)
			v = 0.0f;
		position_ = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace rotary_detail;
		(void)table;
		const distortion_rotary_params &p = p_;

		// 入力 → 歪みの前の 1 次 2 つ
		float x[max_block];
		{
			const float l = p.input_from_left, r = p.input_from_right;
			for (int s = 0; s < n; s++)
				x[s] = sat(l * in_left[s] + r * in_right[s]);
		}
		run(pre_first_, pre_first_state_, x, n);
		run(pre_second_, pre_second_state_, x, n);

		// 飽和の段: 前の段の出力に clip_gain を掛けて飽和。段 2..6 を clip_mix で混ぜる
		float mix[max_block];
		{
			const float g = p.clip_input_gain;
			for (int s = 0; s < n; s++) {
				x[s] = sat(g * x[s]);
				mix[s] = 0.0f;
			}
			for (int stage = 0; stage < 7; stage++) {
				const float gain = p.clip_gain[stage];
				for (int s = 0; s < n; s++)
					x[s] = sat(gain * x[s]);
				if (stage >= 2) {
					const float m = p.clip_mix[stage - 2];
					for (int s = 0; s < n; s++)
						mix[s] += m * x[s];
				}
			}
			const float post_gain = p.post_gain;
			for (int s = 0; s < n; s++)
				x[s] = sat(post_gain * mix[s]);
		}
		run(post_lowpass_, post_lowpass_state_, x, n);
		run(post_mid_, post_mid_state_, x, n);
		run(post_low_, post_low_state_, x, n);

		// 帯域分割（入口は post⁻¹）
		float post_delayed[max_block];
		unit_delay(post_previous_, x, post_delayed, n);
		float band[2][max_block];
		{
			const float g0 = p.split_gain[0], g1 = p.split_gain[1];
			for (int s = 0; s < n; s++) {
				band[0][s] = sat(g0 * post_delayed[s]);
				band[1][s] = sat(g1 * post_delayed[s]);
			}
		}
		run(first_band_a_, first_band_a_state_, band[0], n);
		run(first_band_b_, first_band_b_state_, band[0], n);
		run(second_band_a_, second_band_a_state_, band[1], n);
		run(second_band_b_, second_band_b_state_, band[1], n);

		// 線へ書く（帰還が無いので先にブロック全部を書く）
		{
			float words[max_block];
			const float g0 = p.first_line_input, g1 = p.second_line_input;
			for (int s = 0; s < n; s++)
				words[s] = word(sat(g0 * band[0][s]));
			first_.write(position_, words, n);
			for (int s = 0; s < n; s++)
				words[s] = word(sat(g1 * band[1][s]));
			second_.write(position_, words, n);
		}

		// 声。遅れの揺れは LFO[i]⁻¹、音量の揺れは LFO[i]
		float voice[4][max_block];
		for (int i = 0; i < 4; i++) {
			float previous[max_block + 1];
			previous[0] = lfo_previous_[i];
			std::copy_n(lfo[i], n, previous + 1);
			lfo_previous_[i] = previous[n];

			float v[max_block];
			const int base = std::max(p.voice_base_delay[i], 1);
			if (p.voice_on_second_line[i])
				read_voice(second_, position_, base, p.voice_depth[i], previous, v, n);
			else
				read_voice(first_, position_, base, p.voice_depth[i], previous, v, n);
			const float am = p.voice_am_depth[i];
			for (int s = 0; s < n; s++)
				voice[i][s] = v[s] + am * lfo[i][s] * v[s];
		}

		const float *lv = p.left_from_voice, *rv = p.right_from_voice;
		const float lb = p.left_from_first_band, rb = p.right_from_second_band;
		for (int s = 0; s < n; s++) {
			out_left[s] = sat(lv[0] * voice[0][s] + lv[2] * voice[2][s] + lv[3] * voice[3][s] + lb * band[0][s]);
			out_right[s] = sat(rv[1] * voice[1][s] + rv[3] * voice[3][s] + rv[2] * voice[2][s] + rb * band[1][s]);
		}
		position_ += uint32_t(n);
	}

private:
	distortion_rotary_params p_;
	rotary_detail::first_order pre_first_, pre_second_;
	rotary_detail::second_order post_lowpass_, post_mid_, post_low_;
	rotary_detail::second_order first_band_a_, first_band_b_, second_band_a_, second_band_b_;
	rotary_detail::mid_line first_, second_;
	rotary_detail::first_state pre_first_state_, pre_second_state_;
	rotary_detail::second_state post_lowpass_state_, post_mid_state_, post_low_state_;
	rotary_detail::second_state first_band_a_state_, first_band_b_state_, second_band_a_state_, second_band_b_state_;
	float post_previous_ = 0.0f;
	float lfo_previous_[4] = {};
	uint32_t position_ = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// AUTO PAN 2
// ---------------------------------------------------------------------------------------------------------------
class auto_pan2_flow {
public:
	auto_pan2_flow() { reset(); }

	void configure(const auto_pan2_params &p)
	{
		using namespace rotary_detail;
		p_ = p;
		for (int ch = 0; ch < 2; ch++) {
			low_[ch] = {p.low_shelf_current[ch], p.low_shelf_previous[ch], p.low_shelf_feedback[ch]};
			mid_[ch] = {p.mid_current[ch], p.mid_x1[ch], p.mid_x2[ch], p.mid_y1[ch], p.mid_y2[ch]};
			high_[ch] = {p.high_shelf_current[ch], p.high_shelf_previous[ch], p.high_shelf_feedback[ch]};
		}
	}

	void reset()
	{
		first_.clear();
		second_.clear();
		for (int ch = 0; ch < 2; ch++) {
			low_state_[ch] = {};
			mid_state_[ch] = {};
			high_state_[ch] = {};
			pan_state_[ch] = 0.0f;
		}
		phase_ = {};
		position_ = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace rotary_detail;
		(void)lfo;
		const auto_pan2_params &p = p_;

		// 入力 → EQ
		float x[2][max_block];
		{
			const float g0 = p.input_gain[0], g1 = p.input_gain[1];
			for (int s = 0; s < n; s++) {
				x[0][s] = sat(g0 * in_left[s]);
				x[1][s] = sat(g1 * in_right[s]);
			}
		}
		for (int ch = 0; ch < 2; ch++) {
			run(low_[ch], low_state_[ch], x[ch], n);
			run(mid_[ch], mid_state_[ch], x[ch], n);
			run(high_[ch], high_state_[ch], x[ch], n);
		}

		// 線へ（帰還が無いので先にブロック全部を書く）
		{
			float words[max_block];
			const float fl = p.first_line_from_left, fr = p.first_line_from_right, sr = p.second_line_from_right;
			for (int s = 0; s < n; s++)
				words[s] = word(sat(fl * x[0][s] + fr * x[1][s]));
			first_.write(position_, words, n);
			for (int s = 0; s < n; s++)
				words[s] = word(sat(sr * x[1][s]));
			second_.write(position_, words, n);
		}

		// 位相。サンプル s の波は φ⁻¹（そのサンプルの前の位相）で読む
		float phase[max_block];
		{
			fx_flow::phase_word ph = phase_;
			const double step = double(p.phase_increment) + double(p.phase_increment_fine);
			for (int s = 0; s < n; s++) {
				phase[s] = ph.value();
				ph.add(step);
			}
			phase_ = ph;
		}

		// 波 0..3
		float wave[4][max_block];
		for (int i = 0; i < 4; i++) {
			const float offset = p.wave_phase_offset[i];
			const float center = float(p.wave_read_center[i]);
			const float scale = p.wave_read_scale;
			for (int s = 0; s < n; s++)
				wave[i][s] = fx_flow::table_read(table, center + scale * fx_flow::wrap(phase[s] - offset));
		}

		// 揺れ y[ch]（Y[ch][s] はサンプル s の 1 サンプル前の値）
		constexpr float below_one = 32767.0f / 32768.0f;
		float Y[2][max_block + 1];
		{
			const float mix_a = p.wave_mix_a, mix_b = p.wave_mix_b, gain = p.pan_gain;
			Y[0][0] = pan_state_[0];
			Y[1][0] = pan_state_[1];
			for (int s = 0; s < n; s++) {
				const float a = sat(mix_a * wave[0][s] + mix_b * wave[2][s]);
				const float b = sat(mix_a * wave[1][s] + mix_b * wave[3][s]);
				const float swing[2] = {a, b};
				for (int ch = 0; ch < 2; ch++) {
					const float t1 = fx_flow::positive(p.pan_center[ch] + p.pan_depth[ch] * swing[ch]);
					const float t2 = fx_flow::positive(std::min(below_one, gain * t1));
					Y[ch][s + 1] = fx_flow::positive(std::min(below_one, gain * t2));
				}
			}
			pan_state_[0] = Y[0][n];
			pan_state_[1] = Y[1][n];
		}

		// 声と出口
		float *const out[2] = {out_left, out_right};
		for (int ch = 0; ch < 2; ch++) {
			float v[max_block];
			const int base = std::max(p.voice_base_delay[ch], 1);
			if (ch == 1 && p.right_voice_on_second_line)
				read_voice(second_, position_, base, p.voice_depth[ch], Y[ch], v, n);
			else
				read_voice(first_, position_, base, p.voice_depth[ch], Y[ch], v, n);
			const float am = p.voice_am_depth[ch], g = p.out_gain[ch];
			for (int s = 0; s < n; s++)
				out[ch][s] = sat(g * sat(v[s] + am * Y[ch][s] * v[s]));
		}
		position_ += uint32_t(n);
	}

private:
	auto_pan2_params p_;
	rotary_detail::first_order low_[2], high_[2];
	rotary_detail::second_order mid_[2];
	rotary_detail::mid_line first_, second_;
	rotary_detail::first_state low_state_[2], high_state_[2];
	rotary_detail::second_state mid_state_[2];
	float pan_state_[2] = {};
	fx_flow::phase_word phase_;
	uint32_t position_ = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// RING MOD
// ---------------------------------------------------------------------------------------------------------------
class ring_mod_flow {
public:
	ring_mod_flow() { reset(); }

	void configure(const ring_mod_params &p)
	{
		using namespace rotary_detail;
		p_ = p;
		for (int ch = 0; ch < 2; ch++) {
			low_[ch] = {p.low_shelf_current[ch], p.low_shelf_previous[ch], p.low_shelf_feedback[ch]};
			high_[ch] = {p.high_shelf_current[ch], p.high_shelf_previous[ch], p.high_shelf_feedback[ch]};
			band_[ch] = {p.band_current[ch], p.band_x1[ch], p.band_x2[ch], p.band_y1[ch], p.band_y2[ch]};
			tone_[ch] = {p.band_tone_current[ch], p.band_tone_previous[ch], p.band_tone_feedback[ch]};
		}
		lfo_smooth_ = {p.lfo_smooth_current, p.lfo_smooth_previous, p.lfo_smooth_feedback};
		carrier_smooth_ = {p.carrier_smooth_current, p.carrier_smooth_previous, p.carrier_smooth_feedback};
	}

	void reset()
	{
		carrier_line_.clear();
		for (int ch = 0; ch < 2; ch++) {
			low_state_[ch] = high_state_[ch] = tone_state_[ch] = {};
			band_state_[ch] = {};
		}
		lfo_smooth_state_ = carrier_smooth_state_ = {};
		phase_ = {};
		position_ = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace rotary_detail;
		const ring_mod_params &p = p_;

		// 入力 → d、x
		const float *const in[2] = {in_left, in_right};
		float d[2][max_block], x[2][max_block];
		for (int ch = 0; ch < 2; ch++) {
			const float g = p.input_gain[ch], dry = p.dry_gain[ch], band_gain = p.band_gain[ch];
			for (int s = 0; s < n; s++)
				d[ch][s] = sat(g * in[ch][s]);
			run(low_[ch], low_state_[ch], d[ch], n);
			run(high_[ch], high_state_[ch], d[ch], n);
			for (int s = 0; s < n; s++) {
				d[ch][s] = sat(dry * d[ch][s]);
				x[ch][s] = d[ch][s];
			}
			run(band_[ch], band_state_[ch], x[ch], n);
			run(tone_[ch], tone_state_[ch], x[ch], n);
			for (int s = 0; s < n; s++)
				x[ch][s] = sat(band_gain * x[ch][s]);
		}

		// 搬送波。サンプル s の表の位置は φ⁻¹（そのサンプルの前の位相）から
		{
			float phase[max_block], words[max_block];
			fx_flow::phase_word ph = phase_;
			const double step = p.carrier_increment;
			for (int s = 0; s < n; s++) {
				phase[s] = ph.value();
				ph.add(step);
			}
			phase_ = ph;
			const float center = float(p.carrier_read_center);
			const float scale = p.carrier_table_scale * 32768.0f;
			for (int s = 0; s < n; s++)
				words[s] = word(fx_flow::table_read(table, center + scale * phase[s]));
			carrier_line_.write(position_, words, n);
		}

		// 遅れの揺れ（LFO 1）→ 搬送波の読み出し → ならし。遅れは 4096 以上なので書いた後に読んでよい
		float c[max_block];
		{
			float m[max_block], swing[max_block];
			const float depth = p.lfo_depth, swing_gain = p.carrier_swing_gain;
			for (int s = 0; s < n; s++)
				m[s] = sat(depth * lfo[1][s]);
			{
				// ゆっくり動く状態（README）: 1 LSB に満たない動きは消える
				float x1 = lfo_smooth_state_.x1, y1 = lfo_smooth_state_.y1;
				for (int s = 0; s < n; s++) {
					const float x = m[s];
					y1 = fx_flow::settled_24bit(sat(lfo_smooth_.current * x + lfo_smooth_.previous * x1 + lfo_smooth_.feedback * y1));
					x1 = x;
					m[s] = y1;
				}
				lfo_smooth_state_.x1 = x1;
				lfo_smooth_state_.y1 = y1;
			}
			for (int s = 0; s < n; s++)
				swing[s] = swing_gain * m[s];
			read_swing(carrier_line_, position_, p.carrier_center_delay, swing, c, n);
			run(carrier_smooth_, carrier_smooth_state_, c, n);
		}

		float *const out[2] = {out_left, out_right};
		for (int ch = 0; ch < 2; ch++) {
			const float mix = p.wet_mix[ch], wet = p.wet_gain[ch];
			for (int s = 0; s < n; s++)
				out[ch][s] = sat(mix * d[ch][s] + wet * sat(fx_flow::stepped(c[s]) * x[ch][s]));
		}
		position_ += uint32_t(n);
	}

private:
	ring_mod_params p_;
	rotary_detail::first_order low_[2], high_[2], tone_[2], lfo_smooth_, carrier_smooth_;
	rotary_detail::second_order band_[2];
	rotary_detail::long_line carrier_line_;
	rotary_detail::first_state low_state_[2], high_state_[2], tone_state_[2], lfo_smooth_state_, carrier_smooth_state_;
	rotary_detail::second_state band_state_[2];
	fx_flow::phase_word phase_;
	uint32_t position_ = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// LOW RESO、D.SCRATCH
// ---------------------------------------------------------------------------------------------------------------
class low_reso_flow {
public:
	low_reso_flow() { reset(); }

	void configure(const low_reso_params &p)
	{
		using namespace rotary_detail;
		p_ = p;
		for (int ch = 0; ch < 2; ch++) {
			low_shelf_[ch] = {p.low_shelf_current[ch], p.low_shelf_x1[ch], p.low_shelf_x2[ch], p.low_shelf_y1[ch],
				p.low_shelf_y2[ch]};
			post_[ch] = {p.post_current[ch], p.post_x1[ch], p.post_x2[ch], p.post_y1[ch], p.post_y2[ch]};
			damp_[ch] = {p.voice_damp_current[ch], p.voice_damp_previous[ch], p.voice_damp_feedback[ch]};
		}
	}

	void reset()
	{
		for (int ch = 0; ch < 2; ch++) {
			line_[ch].clear();
			low_shelf_state_[ch] = post_state_[ch] = {};
			damp_state_[ch] = {};
			input_delay_[ch] = 0.0f;
			crushed_delay_[ch] = 0.0f;
		}
		position_ = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		const float *const in[2] = {in_left, in_right};
		float *const out[2] = {out_left, out_right};
		for (int ch = 0; ch < 2; ch++)
			channel(ch, in[ch], lfo, table, out[ch], n);
		position_ += uint32_t(n);
	}

private:
	void channel(int ch, const float *in, const float *const lfo[6], const float *table, float *out, int n)
	{
		using namespace rotary_detail;
		const low_reso_params &p = p_;

		// 入力 → z⁻¹ → ハイパス → 表で粗く → 2 次 → z⁻¹ → r
		float r[max_block];
		{
			float x[max_block], z[max_block];
			const float g = p.input_gain[ch];
			for (int s = 0; s < n; s++)
				x[s] = sat(g * in[s]);
			unit_delay(input_delay_[ch], x, z, n);
			run(low_shelf_[ch], low_shelf_state_[ch], z, n);

			const float drive = p.crush_drive[ch], scale = p.crush_read_scale[ch], center = p.crush_read_center[ch];
			const float crush_gain = p.crush_gain[ch];
			for (int s = 0; s < n; s++) {
				const float d = sat(drive * z[s]);
				const float c = fx_flow::table_read(table, center + scale * d);
				x[s] = sat(crush_gain * c);
			}
			run(post_[ch], post_state_[ch], x, n);
			const float crushed = p.crushed_gain[ch];
			for (int s = 0; s < n; s++)
				x[s] = sat(crushed * x[s]);
			unit_delay(crushed_delay_[ch], x, r, n);
		}

		// 声と帰還。帰還があり遅れは voice_base_delay 以上なので、その長さに区切る
		float v[max_block];
		{
			const int base = std::max(p.voice_base_delay[ch], 1);
			const float depth = p.voice_depth[ch];
			const float *voice_lfo = lfo[0];
			const float from_crushed = p.line_from_crushed[ch], feedback = p.line_feedback[ch];
			for (int start = 0; start < n;) {
				const int m = std::min(n - start, base);
				read_voice(line_[ch], position_ + uint32_t(start), base, depth, voice_lfo + start, v + start, m);
				float words[max_block];
				std::copy_n(v + start, m, words);
				run(damp_[ch], damp_state_[ch], words, m);
				for (int s = 0; s < m; s++)
					words[s] = word(sat(from_crushed * r[start + s] + feedback * words[s]));
				line_[ch].write(position_ + uint32_t(start), words, m);
				start += m;
			}
		}

		// 濡れた音と出口
		{
			const float mc = p.mix_from_crushed[ch], mv = p.mix_from_voice[ch];
			const float am_center = p.am_center[ch], am_depth = p.am_depth[ch];
			const float dry = p.dry_gain[ch], wet = p.wet_gain[ch];
			for (int s = 0; s < n; s++) {
				float w = sat(mc * r[s] + mv * v[s]);
				w = w * (am_center + am_depth * lfo[2][s]);
				out[s] = sat(dry * in[s] + wet * w);
			}
		}
	}

	low_reso_params p_;
	rotary_detail::second_order low_shelf_[2], post_[2];
	rotary_detail::first_order damp_[2];
	rotary_detail::long_line line_[2];
	rotary_detail::second_state low_shelf_state_[2], post_state_[2];
	rotary_detail::first_state damp_state_[2];
	float input_delay_[2] = {};
	float crushed_delay_[2] = {};
	uint32_t position_ = 0;
};

// ---------------------------------------------------------------------------------------------------------------
// D.TURNTBL
// ---------------------------------------------------------------------------------------------------------------
class turntable_flow {
public:
	turntable_flow() { reset(); }

	void configure(const turntable_params &p)
	{
		using namespace rotary_detail;
		p_ = p;
		for (int ch = 0; ch < 2; ch++) {
			dry_lowpass_[ch] = {p.dry_lowpass_current[ch], p.dry_lowpass_previous[ch], p.dry_lowpass_feedback[ch]};
			noise_lpf_[ch] = {p.noise_lpf_current[ch], p.noise_lpf_x1[ch], p.noise_lpf_x2[ch], p.noise_lpf_y1[ch],
				p.noise_lpf_y2[ch]};
		}
		click_filter_ = {p.click_filter_current, p.click_filter_previous, p.click_filter_feedback};
	}

	void reset()
	{
		seed_line_.clear();
		click_line_[0].clear();
		click_line_[1].clear();
		for (int ch = 0; ch < 2; ch++) {
			dry_lowpass_state_[ch] = {};
			noise_lpf_state_[ch] = {};
			input_previous_[ch] = 0.0f;
			hiss_delay_[ch] = 0.0f;
		}
		click_filter_state_ = {};
		chaos_ = 0.0f;
		for (float &v : click_a_)
			v = 0.0f;
		for (float &v : click_b_)
			v = 0.0f;
		position_ = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace rotary_detail;
		const turntable_params &p = p_;

		// 雑音の種。入力によらない
		{
			float z[max_block], words[max_block];
			float zs = chaos_;
			const float cg = p.chaos_gain, ca = p.chaos_add;
			for (int s = 0; s < n; s++) {
				zs = fx_flow::wrap(cg * zs + ca);
				z[s] = zs;
			}
			chaos_ = zs;
			const float seed_gain = p.seed_gain, am_center = p.seed_am_center, am_depth = p.seed_am_depth;
			for (int s = 0; s < n; s++) {
				const float u = seed_gain * z[s];
				words[s] = word(u * sat(am_center + am_depth * lfo[1][s]));
			}
			seed_line_.write(position_, words, n);
		}

		// クリック。位相 A は 5 サンプル前、B は click_b_delay サンプル前から作る
		float click[max_block];
		{
			const int b_delay = std::clamp(p.click_b_delay, 1, 31);
			const float ag = p.click_a_gain, ai = p.click_a_increment, bg = p.click_b_gain, bi = p.click_b_increment;
			float mixed[max_block];
			const float ma = p.click_mix_a, mb = p.click_mix_b;
			for (int s = 0; s < n; s++) {
				const uint32_t now = position_ + uint32_t(s);
				const float a = fx_flow::wrap(ag * click_a_[(now - 5u) & 7u] + ai);
				const float b = fx_flow::wrap(bg * click_b_[(now - uint32_t(b_delay)) & 31u] + bi);
				click_a_[now & 7u] = a;
				click_b_[now & 31u] = b;
				mixed[s] = sat(ma * a + mb * b);
			}
			run(click_filter_, click_filter_state_, mixed, n);
			std::copy_n(mixed, n, click);
		}
		{
			float words[max_block];
			const float *const am_lfo[2] = {lfo[3], lfo[4]};
			for (int ch = 0; ch < 2; ch++) {
				const float center = p.click_am_center[ch], depth = p.click_am_depth[ch];
				for (int s = 0; s < n; s++)
					words[s] = word(click[s] * sat(center + depth * am_lfo[ch][s]));
				click_line_[ch].write(position_, words, n);
			}
		}

		const float *const in[2] = {in_left, in_right};
		float *const out[2] = {out_left, out_right};
		for (int ch = 0; ch < 2; ch++) {
			// x と x⁻¹
			float xe[max_block + 1];
			float *const x = xe + 1;
			{
				const float g = p.input_gain[ch];
				xe[0] = input_previous_[ch];
				for (int s = 0; s < n; s++)
					x[s] = g * in[ch][s];
				input_previous_[ch] = x[n - 1];
			}

			// 乾いた音
			float dry[max_block];
			std::copy_n(x, n, dry);
			run(dry_lowpass_[ch], dry_lowpass_state_[ch], dry, n);

			// 雑音: 種[ch]⁻¹ は、種の線の (seed_delay + 1) サンプル前に seed_out を掛けたもの
			float hiss[max_block];
			{
				const int seed_delay = std::clamp(p.seed_delay[ch], 1, 31);
				float seed[max_block];
				seed_line_.read(position_, seed_delay + 1, seed, n);
				const float seed_out = p.seed_out[ch];
				const float pc = p.noise_path_current[ch], pp = p.noise_path_previous[ch], pn = p.noise_path_noise[ch];
				const float drive = p.table_drive[ch], scale = p.table_read_scale[ch], center = p.table_read_center[ch];
				const float tg = p.table_gain[ch];
				float noise[max_block];
				for (int s = 0; s < n; s++) {
					const float nz = sat(pc * x[s] + pp * xe[s] + pn * (seed[s] * seed_out));
					const float d = sat(drive * nz);
					const float c = fx_flow::table_read(table, center + scale * d);
					noise[s] = tg * c;
				}
				run(noise_lpf_[ch], noise_lpf_state_[ch], noise, n);
				unit_delay(hiss_delay_[ch], noise, hiss, n);
			}

			// クリックの線 ch の click_delay サンプル前
			float cl[max_block];
			click_line_[ch].read(position_, std::clamp(p.click_delay[ch], 1, 31), cl, n);

			const float dl = p.dry_level[ch], cv = p.click_level[ch], nl = p.noise_level[ch], og = p.out_gain[ch];
			for (int s = 0; s < n; s++)
				out[ch][s] = sat(og * sat(dl * dry[s] + cv * cl[s] + nl * hiss[s]));
		}
		position_ += uint32_t(n);
	}

private:
	turntable_params p_;
	rotary_detail::first_order dry_lowpass_[2], click_filter_;
	rotary_detail::second_order noise_lpf_[2];
	rotary_detail::tiny_line seed_line_, click_line_[2];
	rotary_detail::first_state dry_lowpass_state_[2], click_filter_state_;
	rotary_detail::second_state noise_lpf_state_[2];
	float input_previous_[2] = {};
	float hiss_delay_[2] = {};
	float chaos_ = 0.0f;
	float click_a_[8] = {};
	float click_b_[32] = {};
	uint32_t position_ = 0;
};

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_ROTARY_H
