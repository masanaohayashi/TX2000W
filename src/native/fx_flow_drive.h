// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/ のフローだけから書いた歪み系エフェクト。
// distortion.md、auto_wah.md、amp_simulator.md、talk_mod.md。

#ifndef S_MU2000_NATIVE_FX_FLOW_DRIVE_H
#define S_MU2000_NATIVE_FX_FLOW_DRIVE_H

#include "fx_flow_parts.h"
#include "fx_params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace smu2000::native_fx {

namespace drive_detail {

using fx_flow::sat;
using fx_flow::word;

constexpr int B = fx_flow::max_block;
// 最長 65531 サンプル + ブロック 32 を、書いてから読む順でも壊さず入れる長さ
constexpr uint32_t delay_length = 1u << 17;

// ---- フィルタ（共通の形。出力は [sat]、状態は飽和後の値） ----

struct bq_coef {
	float b0, b1, b2, a1, a2;
};
struct bq_state {
	float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

inline void run_bq(const bq_coef &c, bq_state &st, float *sig, int n)
{
	const float b0 = c.b0, b1 = c.b1, b2 = c.b2, a1 = c.a1, a2 = c.a2;
	float x1 = st.x1, x2 = st.x2, y1 = st.y1, y2 = st.y2;
	for (int s = 0; s < n; s++) {
		const float x = sig[s];
		const float y = sat(b0 * x + b1 * x1 + b2 * x2 + a1 * y1 + a2 * y2);
		x2 = x1;
		x1 = x;
		y2 = y1;
		y1 = y;
		sig[s] = y;
	}
	st.x1 = x1;
	st.x2 = x2;
	st.y1 = y1;
	st.y2 = y2;
}

struct op_coef {
	float cur, prev, fb;
};
struct op_state {
	float x1 = 0, y1 = 0;
};

inline void run_op(const op_coef &c, op_state &st, float *sig, int n)
{
	const float cur = c.cur, prev = c.prev, fb = c.fb;
	float x1 = st.x1, y1 = st.y1;
	for (int s = 0; s < n; s++) {
		const float x = sig[s];
		const float y = sat(cur * x + prev * x1 + fb * y1);
		x1 = x;
		y1 = y;
		sig[s] = y;
	}
	st.x1 = x1;
	st.y1 = y1;
}

// ---- 音量検出（README の形）。ならした値 smooth を返す ----

struct det_coef {
	float sensitivity, peak_input, peak_release, smooth_input, smooth_from_peak, smooth_from_input, smooth_feedback;
};
struct det_state {
	float peak = 0, smooth = 0;
};

inline void run_detector(const det_coef &c, det_state &st, const float *sig, float *smooth_out, int n)
{
	float e[B];
	for (int s = 0; s < n; s++)
		e[s] = std::min(fx_flow::sat_max, std::fabs(c.sensitivity * sig[s]));
	const float pi = c.peak_input, pr = c.peak_release, si = c.smooth_input, sp = c.smooth_from_peak, sx = c.smooth_from_input, sf = c.smooth_feedback;
	float peak = st.peak, smooth = st.smooth;
	for (int s = 0; s < n; s++) {
		peak = fx_flow::settled_24bit(sat(pi * e[s] + fx_flow::positive(pr * peak - e[s])));
		smooth = fx_flow::settled_24bit(sat(si * e[s] + fx_flow::positive(sp * peak - sx * e[s] + sf * smooth)));
		smooth_out[s] = smooth;
	}
	st.peak = peak;
	st.smooth = smooth;
}

// ---- 折れ線の利得 ----

struct knee_coef {
	float offset[8], slope[8], base, gain_current, gain_previous, gain_feedback;
};
struct knee_state {
	float target1 = 0, target2 = 0, g1 = 0;
};

inline void run_knee(const knee_coef &c, knee_state &st, const float *level, float *g, int n)
{
	float acc[B], target[B + 2];
	for (int s = 0; s < n; s++)
		acc[s] = c.base;
	for (int i = 0; i < 8; i++) {
		const float offset = c.offset[i], slope = c.slope[i];
		for (int s = 0; s < n; s++)
			acc[s] += slope * fx_flow::positive(offset + level[s]);
	}
	// target[s + 2] はこのサンプルの target。g は 1 つ前・2 つ前の target を使う
	target[0] = st.target2;
	target[1] = st.target1;
	for (int s = 0; s < n; s++)
		target[s + 2] = fx_flow::positive(acc[s]);
	const float gc = c.gain_current, gp = c.gain_previous, gf = c.gain_feedback;
	float feed[B];
	for (int s = 0; s < n; s++)
		feed[s] = gc * target[s + 1] + gp * target[s];
	float prev = st.g1;
	for (int s = 0; s < n; s++) {
		prev = sat(feed[s] + gf * prev);
		g[s] = prev;
	}
	st.target2 = target[n];
	st.target1 = target[n + 1];
	st.g1 = prev;
}

// ---- 飽和の段。stages = 8 なら段 3..7、7 なら段 2..6 を clip_mix[0..4] で混ぜる ----

template <int stages>
inline void run_clip(const float *gain, const float *mix, const float *in, float *out, int n)
{
	constexpr int first = stages - 5;
	float t[B], acc[B];
	for (int s = 0; s < n; s++) {
		t[s] = sat(gain[0] * in[s]);
		acc[s] = 0.0f;
	}
	for (int i = 1; i < stages; i++) {
		const float g = gain[i];
		for (int s = 0; s < n; s++)
			t[s] = sat(g * t[s]);
		if (i >= first) {
			const float m = mix[i - first];
			for (int s = 0; s < n; s++)
				acc[s] += m * t[s];
		}
	}
	for (int s = 0; s < n; s++)
		out[s] = sat(acc[s]);
}

// 1 サンプル遅らせた列を作る。last は呼ぶ側の 1 サンプル前の値
inline void delayed_by_one(const float *in, float *out, float &last, int n)
{
	out[0] = last;
	for (int s = 1; s < n; s++)
		out[s] = in[s - 1];
	last = in[n - 1];
}

// ---- 遅れ（帰還つき、左右の読み出し）。v_distortion、distortion_delay、wah_distortion_delay で共通 ----

struct delay_coef {
	int feedback_time, left_time, right_time;
	float damp_current, damp_previous, damp_feedback, feedback, from_left, from_right;
	float tap_gain[2], wet_gain[2], dry_gain[2];
};

template <class P>
inline delay_coef make_delay_coef(const P &p)
{
	return {p.delay_feedback_time, p.delay_left_time, p.delay_right_time, p.delay_damp_current, p.delay_damp_previous,
		p.delay_damp_feedback, p.delay_feedback, p.delay_from_left, p.delay_from_right, {p.delay_tap_gain[0], p.delay_tap_gain[1]},
		{p.wet_gain[0], p.wet_gain[1]}, {p.dry_gain[0], p.dry_gain[1]}};
}

class delay_stage {
public:
	void reset()
	{
		line_.clear();
		position_ = 0;
		previous_read_ = damped_ = 0.0f;
	}

	// mixed_left, mixed_right: 遅れの前の左右。out は m と別の配列
	void run(const delay_coef &c, const float *mixed_left, const float *mixed_right, float *out_left, float *out_right, int n)
	{
		if (c.feedback_time <= 0 && c.left_time <= 0 && c.right_time <= 0) {
			// 遅れを使わない種類。線には書かない
			for (int s = 0; s < n; s++) {
				out_left[s] = sat(c.dry_gain[0] * mixed_left[s]);
				out_right[s] = sat(c.dry_gain[1] * mixed_right[s]);
			}
			position_ += uint32_t(n);
			return;
		}
		const int feedback_time = std::clamp(c.feedback_time, 1, 65531);
		const int left_time = std::clamp(c.left_time, 1, 65531);
		const int right_time = std::clamp(c.right_time, 1, 65531);
		const float wet_left = c.wet_gain[0] * c.tap_gain[0], wet_right = c.wet_gain[1] * c.tap_gain[1];
		const float dc = c.damp_current, dp = c.damp_previous, df = c.damp_feedback;
		const float fb = c.feedback, fl = c.from_left, fr = c.from_right;
		int done = 0;
		while (done < n) {
			// 帰還の遅れより長い塊は、書く前に読めない
			const int m = std::min(n - done, feedback_time);
			float r[B], w[B], tap_left[B], tap_right[B];
			line_.read(position_, feedback_time, r, m);
			float previous = previous_read_, damped = damped_;
			for (int s = 0; s < m; s++) {
				damped = sat(dc * r[s] + dp * previous + df * damped);
				previous = r[s];
				w[s] = word(sat(fb * damped + fl * mixed_left[done + s] + fr * mixed_right[done + s]));
			}
			previous_read_ = previous;
			damped_ = damped;
			line_.write(position_, w, m);
			// 帰還の無い読み出しは書いたあと
			line_.read(position_, left_time, tap_left, m);
			line_.read(position_, right_time, tap_right, m);
			for (int s = 0; s < m; s++) {
				out_left[done + s] = sat(wet_left * tap_left[s] + c.dry_gain[0] * mixed_left[done + s]);
				out_right[done + s] = sat(wet_right * tap_right[s] + c.dry_gain[1] * mixed_right[done + s]);
			}
			position_ += uint32_t(m);
			done += m;
		}
	}

private:
	fx_flow::delay_line<delay_length> line_;
	uint32_t position_ = 0;
	float previous_read_ = 0.0f, damped_ = 0.0f;
};

// ---- アンプの歪みの部品（amp_simulator.md） ----

struct tube_coef {
	float overdrive, tube_bias, tube_smooth, hard_mix, soft_input_gain;
};

inline void run_tube(const tube_coef &c, float &dc_state, const float *x, float *z, int n)
{
	float hard[B], square[B], ac[B];
	for (int s = 0; s < n; s++) {
		hard[s] = sat(16.0f * sat(16.0f * sat(c.overdrive * x[s])));
		const float u = sat(c.tube_bias + hard[s]);
		square[s] = sat(u * u);
	}
	float dc = dc_state;
	const float ts = c.tube_smooth;
	for (int s = 0; s < n; s++) {
		dc = dc + ts * (square[s] - dc);
		ac[s] = square[s] - dc;
	}
	dc_state = dc;
	for (int s = 0; s < n; s++) {
		const float tone = sat(hard[s] + c.hard_mix * (ac[s] - hard[s]));
		z[s] = sat(c.soft_input_gain * tone);
	}
}

} // namespace drive_detail

// ======================================================================
// DISTORTION、OVERDRIVE、AMP SIM、CMP+DT、HM ENHNCER、COMPRESSOR、NOISE GATE
// ======================================================================
class distortion_flow {
public:
	distortion_flow() = default;
	void configure(const distortion_params &p) { p_ = p; }
	void reset() { st_ = state{}; }

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *left_out,
		float *right_out, int n)
	{
		using namespace drive_detail;
		(void)lfo;
		(void)table;
		const distortion_params &p = p_;
		float *const out_left = p.swap_sides ? right_out : left_out;
		float *const out_right = p.swap_sides ? left_out : right_out;
		float inl[B], inr[B];
		std::copy_n(in_left, n, inl);
		std::copy_n(in_right, n, inr);

		float a[B], b[B];
		for (int s = 0; s < n; s++) {
			a[s] = sat(p.input_a_from_right * inr[s] + p.input_a_from_left * inl[s]);
			b[s] = sat(p.input_b_from_left * inl[s]);
		}
		run_bq({p.highpass_a_current, p.highpass_a_x1, p.highpass_a_x2, p.highpass_a_y1, p.highpass_a_y2}, st_.hp_a, a, n);
		run_bq({p.highpass_b_current, p.highpass_b_x1, p.highpass_b_x2, p.highpass_b_y1, p.highpass_b_y2}, st_.hp_b, b, n);
		for (int s = 0; s < n; s++) {
			a[s] = sat(p.drive_a * a[s]);
			b[s] = sat(p.drive_b * b[s]);
		}

		// 音量検出（この種類は peak_input が無い形）
		float e[B], level[B], g[B];
		for (int s = 0; s < n; s++)
			e[s] = std::min(fx_flow::sat_max, std::fabs(p.detector_a * a[s] + p.detector_b * b[s]));
		{
			float peak = st_.peak, smooth = st_.smooth;
			for (int s = 0; s < n; s++) {
				peak = fx_flow::settled_24bit(sat(std::max(e[s], p.peak_release * peak)));
				smooth = fx_flow::settled_24bit(sat(std::max(e[s], p.smooth_input * peak + p.smooth_release * smooth)));
				level[s] = sat(p.threshold_gain * smooth);
			}
			st_.peak = peak;
			st_.smooth = smooth;
		}
		knee_coef kc{};
		std::copy_n(p.knee_offset, 8, kc.offset);
		std::copy_n(p.knee_slope, 8, kc.slope);
		kc.base = p.knee_base;
		kc.gain_current = p.gain_current;
		kc.gain_previous = p.gain_previous;
		kc.gain_feedback = p.gain_feedback;
		run_knee(kc, st_.knee, level, g, n);

		// 形を作る → 1 次フィルタ
		float fa[B], fb[B];
		for (int s = 0; s < n; s++) {
			const float ga = g[s] * a[s], gb = g[s] * b[s];
			fa[s] = sat(p.shape_gain_a * ga + p.shape_dry_a * a[s] + p.shape_square_a * ga * a[s]);
			fb[s] = sat(p.shape_gain_b * gb + p.shape_dry_b * b[s] + p.shape_square_b * gb * b[s]);
		}
		run_op({p.out_filter_a_current, p.out_filter_a_previous, p.out_filter_a_feedback}, st_.fo_a, fa, n);
		run_op({p.out_filter_b_current, p.out_filter_b_previous, p.out_filter_b_feedback}, st_.fo_b, fb, n);

		// z⁻¹: 飽和の段と出口は 1 サンプル前の a_out、b_out
		float a_out[B], b_out[B], post[B];
		delayed_by_one(fa, a_out, st_.a_prev, n);
		delayed_by_one(fb, b_out, st_.b_prev, n);

		run_clip<8>(p.clip_gain, p.clip_mix, a_out, post, n);
		for (int s = 0; s < n; s++)
			post[s] = sat(p.post_gain * post[s]);
		run_bq({p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2}, st_.lp, post, n);
		run_bq({p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2}, st_.mid, post, n);
		run_bq({p.post_low_current, p.post_low_x1, p.post_low_x2, p.post_low_y1, p.post_low_y2}, st_.low, post, n);

		// 乾いた音は 1 サンプル前の入力。左右が入れ替わる
		float dry_r[B], dry_l[B];
		delayed_by_one(inr, dry_r, st_.r_prev, n);
		delayed_by_one(inl, dry_l, st_.l_prev, n);
		for (int s = 0; s < n; s++) {
			out_left[s] = sat(p.left_from_post * post[s] + 4.0f * p.dry_to_left * dry_r[s]);
			out_right[s] = sat(p.right_from_post * post[s] + p.right_from_b * b_out[s] + 4.0f * p.dry_to_right * dry_l[s]);
		}
	}

private:
	struct state {
		drive_detail::bq_state hp_a, hp_b, lp, mid, low;
		drive_detail::op_state fo_a, fo_b;
		drive_detail::knee_state knee;
		float peak = 0, smooth = 0;
		float a_prev = 0, b_prev = 0, l_prev = 0, r_prev = 0;
	};
	distortion_params p_{};
	state st_{};
};

// ======================================================================
// DT+DELAY、OD+DELAY、CMP+DT+DLY、CMP+OD+DLY
// ======================================================================
class distortion_delay_flow {
public:
	distortion_delay_flow() = default;
	void configure(const distortion_delay_params &p) { p_ = p; }
	void reset()
	{
		st_ = state{};
		delay_.reset();
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *out_left,
		float *out_right, int n)
	{
		using namespace drive_detail;
		(void)lfo;
		(void)table;
		const distortion_delay_params &p = p_;
		float inl[B], inr[B];
		std::copy_n(in_left, n, inl);
		std::copy_n(in_right, n, inr);

		float a[B];
		for (int s = 0; s < n; s++)
			a[s] = sat(p.input_from_left * inl[s] + p.input_from_right * inr[s]);
		run_op({p.highpass_current, p.highpass_previous, p.highpass_feedback}, st_.hp, a, n);
		for (int s = 0; s < n; s++)
			a[s] = sat(p.drive * a[s]);

		float smooth[B], level[B], g[B];
		run_detector({p.detector, p.peak_input, p.peak_release, p.smooth_input, p.smooth_from_peak, p.smooth_from_input, p.smooth_feedback},
			st_.det, a, smooth, n);
		for (int s = 0; s < n; s++)
			level[s] = sat(p.threshold_gain * smooth[s]);
		knee_coef kc{};
		std::copy_n(p.knee_offset, 8, kc.offset);
		std::copy_n(p.knee_slope, 8, kc.slope);
		kc.base = p.knee_base;
		kc.gain_current = p.gain_current;
		kc.gain_previous = p.gain_previous;
		kc.gain_feedback = p.gain_feedback;
		run_knee(kc, st_.knee, level, g, n);

		float y[B];
		for (int s = 0; s < n; s++)
			y[s] = sat(p.shape_gain * (g[s] * a[s]) + p.shape_dry * a[s]);
		run_op({p.shape_filter_current, p.shape_filter_previous, p.shape_filter_feedback}, st_.shape, y, n);

		float a_out[B], post[B];
		delayed_by_one(y, a_out, st_.a_prev, n);
		run_clip<8>(p.clip_gain, p.clip_mix, a_out, post, n);
		for (int s = 0; s < n; s++)
			post[s] = sat(p.post_gain * post[s]);
		run_bq({p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2}, st_.lp, post, n);
		run_bq({p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2}, st_.mid, post, n);
		run_op({p.post_low_current, p.post_low_previous, p.post_low_feedback}, st_.low, post, n);

		float mixed_left[B], mixed_right[B];
		for (int s = 0; s < n; s++) {
			mixed_left[s] = sat(p.mix_left_from_input * inl[s] + p.mix_left_from_post * post[s]);
			mixed_right[s] = sat(p.mix_right_from_input * inr[s] + p.mix_right_from_post * post[s]);
		}
		delay_.run(make_delay_coef(p), mixed_left, mixed_right, out_left, out_right, n);
	}

private:
	struct state {
		drive_detail::bq_state lp, mid;
		drive_detail::op_state hp, shape, low;
		drive_detail::det_state det;
		drive_detail::knee_state knee;
		float a_prev = 0;
	};
	distortion_delay_params p_{};
	state st_{};
	drive_detail::delay_stage delay_;
};

// ======================================================================
// STREO DT、STREO OD、STREO AMP
// ======================================================================
class stereo_distortion_flow {
public:
	stereo_distortion_flow() = default;
	void configure(const stereo_distortion_params &p) { p_ = p; }
	void reset() { st_[0] = st_[1] = channel_state{}; }

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *out_left,
		float *out_right, int n)
	{
		(void)lfo;
		(void)table;
		float inl[drive_detail::B], inr[drive_detail::B];
		std::copy_n(in_left, n, inl);
		std::copy_n(in_right, n, inr);
		channel(0, inl, out_left, n);
		channel(1, inr, out_right, n);
	}

private:
	struct channel_state {
		drive_detail::op_state first, second;
		drive_detail::bq_state lp, mid, low;
	};

	void channel(int ch, const float *in, float *out, int n)
	{
		using namespace drive_detail;
		const stereo_distortion_params &p = p_;
		channel_state &st = st_[ch];
		float x[B], post[B];
		for (int s = 0; s < n; s++)
			x[s] = sat(p.input_gain[ch] * in[s]);
		run_op({p.pre_first_current[ch], p.pre_first_previous[ch], p.pre_first_feedback[ch]}, st.first, x, n);
		run_op({p.pre_second_current[ch], p.pre_second_previous[ch], p.pre_second_feedback[ch]}, st.second, x, n);
		run_clip<8>(p.clip_gain[ch], p.clip_mix[ch], x, post, n);
		for (int s = 0; s < n; s++)
			post[s] = sat(p.post_gain[ch] * post[s]);
		run_bq({p.post_lowpass_current[ch], p.post_lowpass_x1[ch], p.post_lowpass_x2[ch], p.post_lowpass_y1[ch], p.post_lowpass_y2[ch]},
			st.lp, post, n);
		run_bq({p.post_mid_current[ch], p.post_mid_x1[ch], p.post_mid_x2[ch], p.post_mid_y1[ch], p.post_mid_y2[ch]}, st.mid, post, n);
		run_bq({p.post_low_current[ch], p.post_low_x1[ch], p.post_low_x2[ch], p.post_low_y1[ch], p.post_low_y2[ch]}, st.low, post, n);
		for (int s = 0; s < n; s++)
			out[s] = sat(p.dry_gain[ch] * in[s] + p.wet_gain[ch] * post[s]);
	}

	stereo_distortion_params p_{};
	channel_state st_[2]{};
};

// ======================================================================
// AUTO WAH、A-WAH+DT/OD、TOUCH WAH 1/2、T-WAH+DIST/ODRV
// ======================================================================
class auto_wah_flow {
public:
	auto_wah_flow() = default;
	void configure(const auto_wah_params &p) { p_ = p; }
	void reset() { st_ = state{}; }

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *out_left,
		float *out_right, int n)
	{
		using namespace drive_detail;
		(void)table;
		const auto_wah_params &p = p_;
		float a[B], b[B];
		for (int s = 0; s < n; s++) {
			a[s] = sat(p.input_gain[0] * in_left[s]);
			b[s] = sat(p.input_gain[1] * in_right[s]);
		}
		run_op({p.low_shelf_current[0], p.low_shelf_previous[0], p.low_shelf_feedback[0]}, st_.low_shelf[0], a, n);
		run_op({p.high_shelf_current[0], p.high_shelf_previous[0], p.high_shelf_feedback[0]}, st_.high_shelf[0], a, n);
		run_op({p.low_shelf_current[1], p.low_shelf_previous[1], p.low_shelf_feedback[1]}, st_.low_shelf[1], b, n);
		run_op({p.high_shelf_current[1], p.high_shelf_previous[1], p.high_shelf_feedback[1]}, st_.high_shelf[1], b, n);

		// 音量検出 → 遮断の係数 t
		float d[B], smooth[B], t[B];
		for (int s = 0; s < n; s++)
			d[s] = sat(p.detector_from_left * a[s] + p.detector_from_right * b[s]);
		run_op({p.detector_filter_current, p.detector_filter_previous, p.detector_filter_feedback}, st_.detector_filter, d, n);
		run_detector({p.sensitivity, p.peak_input, p.peak_release, p.smooth_input, p.smooth_from_peak, p.smooth_from_input, p.smooth_feedback},
			st_.det, d, smooth, n);
		run_op({p.envelope_filter_current, p.envelope_filter_previous, p.envelope_filter_feedback}, st_.envelope_filter, smooth, n);
		for (int s = 0; s < n; s++) {
			const float u = sat(p.cutoff_base + p.cutoff_from_lfo * lfo[0][s] + p.cutoff_from_envelope * smooth[s]);
			const float u2 = sat(fx_flow::stepped(u) * u);
			t[s] = fx_flow::stepped(sat(p.cutoff_floor + fx_flow::stepped(u2) * u2));
		}

		// ワウ（左右）
		float wah0[B], wah1[B];
		{
			float band0 = st_.band[0], low0 = st_.low[0], band1 = st_.band[1], low1 = st_.low[1];
			for (int s = 0; s < n; s++) {
				const float ts = t[s];
				const float high0 = sat(a[s] + p.resonance[0] * band0 - p.low_feedback[0] * low0);
				band0 = sat(band0 + ts * high0);
				low0 = sat(low0 + ts * band0);
				wah0[s] = sat(p.wah_from_eq[0] * a[s] + p.wah_from_band[0] * band0);
				const float high1 = sat(b[s] + p.resonance[1] * band1 - p.low_feedback[1] * low1);
				band1 = sat(band1 + ts * high1);
				low1 = sat(low1 + ts * band1);
				wah1[s] = sat(p.wah_from_eq[1] * b[s] + p.wah_from_band[1] * band1);
			}
			st_.band[0] = band0;
			st_.low[0] = low0;
			st_.band[1] = band1;
			st_.low[1] = low1;
		}

		// 飽和の道と出口は前のサンプルの wah から
		float wp0[B], wp1[B], x[B], post[B];
		delayed_by_one(wah0, wp0, st_.wah_prev[0], n);
		delayed_by_one(wah1, wp1, st_.wah_prev[1], n);
		for (int s = 0; s < n; s++)
			x[s] = sat(p.drive_from_left * wp0[s] + p.drive_from_right * wp1[s]);
		run_op({p.drive_low_current, p.drive_low_previous, p.drive_low_feedback}, st_.drive_low, x, n);
		run_op({p.drive_high_current, p.drive_high_previous, p.drive_high_feedback}, st_.drive_high, x, n);
		for (int s = 0; s < n; s++)
			x[s] = sat(p.clip_input_gain * x[s]);
		run_clip<7>(p.clip_gain, p.clip_mix, x, post, n);
		for (int s = 0; s < n; s++)
			post[s] = sat(p.post_gain * post[s]);
		run_bq({p.post_lowpass_current, p.post_lowpass_x1, p.post_lowpass_x2, p.post_lowpass_y1, p.post_lowpass_y2}, st_.lp, post, n);
		run_bq({p.post_mid_current, p.post_mid_x1, p.post_mid_x2, p.post_mid_y1, p.post_mid_y2}, st_.mid, post, n);
		run_bq({p.post_low_current, p.post_low_x1, p.post_low_x2, p.post_low_y1, p.post_low_y2}, st_.low_bq, post, n);
		for (int s = 0; s < n; s++) {
			out_left[s] = sat(p.left_from_post * post[s] + p.left_from_wah * wp0[s]);
			out_right[s] = sat(p.right_from_post * post[s] + p.right_from_wah * wp1[s]);
		}
	}

private:
	struct state {
		drive_detail::op_state low_shelf[2], high_shelf[2], detector_filter, envelope_filter, drive_low, drive_high;
		drive_detail::bq_state lp, mid, low_bq;
		drive_detail::det_state det;
		float band[2] = {}, low[2] = {}, wah_prev[2] = {};
	};
	auto_wah_params p_{};
	state st_{};
};

// ======================================================================
// WAH+DT+DLY、WAH+OD+DLY
// ======================================================================
class wah_distortion_delay_flow {
public:
	wah_distortion_delay_flow() = default;
	void configure(const wah_distortion_delay_params &p) { p_ = p; }
	void reset()
	{
		st_ = state{};
		delay_.reset();
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *out_left,
		float *out_right, int n)
	{
		using namespace drive_detail;
		(void)lfo;
		(void)table;
		const wah_distortion_delay_params &p = p_;
		float a[B], b[B];
		for (int s = 0; s < n; s++) {
			a[s] = sat(p.input_gain[0] * in_left[s]);
			b[s] = sat(p.input_gain[1] * in_right[s]);
		}
		run_op({p.low_shelf_current[0], p.low_shelf_previous[0], p.low_shelf_feedback[0]}, st_.low_shelf[0], a, n);
		run_op({p.high_shelf_current[0], p.high_shelf_previous[0], p.high_shelf_feedback[0]}, st_.high_shelf[0], a, n);
		run_op({p.low_shelf_current[1], p.low_shelf_previous[1], p.low_shelf_feedback[1]}, st_.low_shelf[1], b, n);
		run_op({p.high_shelf_current[1], p.high_shelf_previous[1], p.high_shelf_feedback[1]}, st_.high_shelf[1], b, n);

		float w[B], d[B], smooth[B], t[B];
		for (int s = 0; s < n; s++) {
			w[s] = sat(p.wah_from_left * a[s] + p.wah_from_right * b[s]);
			d[s] = w[s];
		}
		run_op({p.detector_filter_current, p.detector_filter_previous, p.detector_filter_feedback}, st_.detector_filter, d, n);
		run_detector({p.sensitivity, p.peak_input, p.peak_release, p.smooth_input, p.smooth_from_peak, p.smooth_from_input, p.smooth_feedback},
			st_.det, d, smooth, n);
		run_op({p.envelope_filter_current, p.envelope_filter_previous, p.envelope_filter_feedback}, st_.envelope_filter, smooth, n);
		for (int s = 0; s < n; s++) {
			const float u = sat(p.cutoff_base + p.cutoff_from_envelope * smooth[s]);
			const float u2 = sat(fx_flow::stepped(u) * u);
			t[s] = fx_flow::stepped(sat(p.cutoff_floor + fx_flow::stepped(u2) * u2));
		}

		float wah0[B], wah1[B];
		{
			float band = st_.band, low = st_.low;
			for (int s = 0; s < n; s++) {
				const float high = sat(w[s] + p.resonance * band - p.low_feedback * low);
				band = sat(band + t[s] * high);
				low = sat(low + t[s] * band);
				wah0[s] = sat(p.wah_left_from_eq * a[s] + p.wah_left_from_band * band);
				wah1[s] = sat(p.wah_right_from_eq * b[s] + p.wah_right_from_band * band);
			}
			st_.band = band;
			st_.low = low;
		}

		float wp0[B], wp1[B], x[B], post[B], post_d[B];
		delayed_by_one(wah0, wp0, st_.wah_prev[0], n);
		delayed_by_one(wah1, wp1, st_.wah_prev[1], n);
		for (int s = 0; s < n; s++)
			x[s] = sat(p.drive_from_left * wp0[s] + p.drive_from_right * wp1[s]);
		run_op({p.drive_low_current, p.drive_low_previous, p.drive_low_feedback}, st_.drive_low, x, n);
		run_op({p.drive_high_current, p.drive_high_previous, p.drive_high_feedback}, st_.drive_high, x, n);
		for (int s = 0; s < n; s++)
			x[s] = sat(p.clip_input_gain * x[s]);
		run_clip<7>(p.clip_gain, p.clip_mix, x, post, n);
		for (int s = 0; s < n; s++)
			post[s] = sat(p.post_gain * post[s]);
		run_op({p.post_first_current, p.post_first_previous, p.post_first_feedback}, st_.post_first, post, n);
		run_bq({p.post_second_current, p.post_second_x1, p.post_second_x2, p.post_second_y1, p.post_second_y2}, st_.post_second, post, n);
		run_op({p.post_third_current, p.post_third_previous, p.post_third_feedback}, st_.post_third, post, n);
		delayed_by_one(post, post_d, st_.post_prev, n);

		float mixed_left[B], mixed_right[B];
		for (int s = 0; s < n; s++) {
			mixed_left[s] = sat(p.mix_left_from_wah * wp0[s] + p.mix_left_from_post * post_d[s]);
			mixed_right[s] = sat(p.mix_right_from_wah * wp1[s] + p.mix_right_from_post * post_d[s]);
		}
		delay_.run(make_delay_coef(p), mixed_left, mixed_right, out_left, out_right, n);
	}

private:
	struct state {
		drive_detail::op_state low_shelf[2], high_shelf[2], detector_filter, envelope_filter, drive_low, drive_high, post_first, post_third;
		drive_detail::bq_state post_second;
		drive_detail::det_state det;
		float band = 0, low = 0, wah_prev[2] = {}, post_prev = 0;
	};
	wah_distortion_delay_params p_{};
	state st_{};
	drive_detail::delay_stage delay_;
};

// ======================================================================
// V DT HARD、V DT H+DLY、V DT SOFT、V DT S+DLY
// ======================================================================
class v_distortion_flow {
public:
	v_distortion_flow() = default;
	void configure(const v_distortion_params &p) { p_ = p; }
	void reset()
	{
		st_ = state{};
		delay_.reset();
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *out_left,
		float *out_right, int n)
	{
		using namespace drive_detail;
		(void)lfo;
		(void)table;
		const v_distortion_params &p = p_;
		float inl[B], inr[B];
		std::copy_n(in_left, n, inl);
		std::copy_n(in_right, n, inr);

		float x[B], z[B], shaped[B], c[B], amp[B];
		for (int s = 0; s < n; s++)
			x[s] = sat(p.input_from_left * inl[s] + p.input_from_right * inr[s]);
		run_op({p.pre_first_current, p.pre_first_previous, p.pre_first_feedback}, st_.pre_first, x, n);
		run_bq({p.pre_second_current, p.pre_second_x1, p.pre_second_x2, p.pre_second_y1, p.pre_second_y2}, st_.pre_second, x, n);
		run_op({p.pre_third_current, p.pre_third_previous, p.pre_third_feedback}, st_.pre_third, x, n);
		run_tube({p.overdrive, p.tube_bias, p.tube_smooth, p.hard_mix, p.soft_input_gain}, st_.dc, x, z, n);

		for (int s = 0; s < n; s++) {
			const float soft = sat(p.soft_scale * z[s]);
			shaped[s] = sat(p.soft_linear * soft + p.soft_cubic * soft * soft * soft);
		}
		delayed_by_one(shaped, c, st_.shaped_prev, n);
		for (int s = 0; s < n; s++)
			c[s] = p.cabinet_input_gain * c[s]; // 段 0 の入口は飽和しない
		run_bq({p.cabinet_0_current, p.cabinet_0_x1, p.cabinet_0_x2, p.cabinet_0_y1, p.cabinet_0_y2}, st_.cab_bq[0], c, n);
		run_bq({p.cabinet_1_current, p.cabinet_1_x1, p.cabinet_1_x2, p.cabinet_1_y1, p.cabinet_1_y2}, st_.cab_bq[1], c, n);
		run_op({p.cabinet_2_current, p.cabinet_2_previous, p.cabinet_2_feedback}, st_.cab_op[0], c, n);
		run_op({p.cabinet_3_current, p.cabinet_3_previous, p.cabinet_3_feedback}, st_.cab_op[1], c, n);
		run_op({p.cabinet_4_current, p.cabinet_4_previous, p.cabinet_4_feedback}, st_.cab_op[2], c, n);
		run_op({p.cabinet_5_current, p.cabinet_5_previous, p.cabinet_5_feedback}, st_.cab_op[3], c, n);
		run_bq({p.cabinet_6_current, p.cabinet_6_x1, p.cabinet_6_x2, p.cabinet_6_y1, p.cabinet_6_y2}, st_.cab_bq[2], c, n);
		run_op({p.cabinet_7_current, p.cabinet_7_previous, p.cabinet_7_feedback}, st_.cab_op[4], c, n);
		for (int s = 0; s < n; s++)
			amp[s] = sat(p.cabinet_output_gain * c[s]);

		float mixed_left[B], mixed_right[B];
		for (int s = 0; s < n; s++) {
			mixed_left[s] = sat(p.mix_left_from_input * inl[s] + p.mix_from_amp * amp[s]);
			mixed_right[s] = sat(p.mix_right_from_input * inr[s] + p.mix_from_amp * amp[s]);
		}
		delay_.run(make_delay_coef(p), mixed_left, mixed_right, out_left, out_right, n);
	}

private:
	struct state {
		drive_detail::op_state pre_first, pre_third, cab_op[5];
		drive_detail::bq_state pre_second, cab_bq[3];
		float dc = 0, shaped_prev = 0;
	};
	v_distortion_params p_{};
	state st_{};
	drive_detail::delay_stage delay_;
};

// ======================================================================
// AMP SIM 2
// ======================================================================
class amp_sim2_flow {
public:
	amp_sim2_flow()
	{
		// sin(π/2 × u) を語に丸めた 4097 語（u = -1 〜 +1、2048 が u = 0）。補間で 1 語先を読むので 1 語余分に持つ
		for (int i = 0; i <= 4096; i++)
			sine_[i] = fx_flow::word(float(std::sin(1.5707963267948966 * ((i - 2048) / 2048.0))));
		sine_[4097] = sine_[4096];
	}
	void configure(const amp_sim2_params &p) { p_ = p; }
	void reset() { st_ = state{}; }

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *out_left,
		float *out_right, int n)
	{
		using namespace drive_detail;
		(void)lfo;
		(void)table;
		const amp_sim2_params &p = p_;
		float inl[B], inr[B];
		std::copy_n(in_left, n, inl);
		std::copy_n(in_right, n, inr);

		float x[B], z[B], zd[B], c[B];
		for (int s = 0; s < n; s++)
			x[s] = sat(p.input_from_left * inl[s] + p.input_from_right * inr[s]);
		run_op({p.pre_first_current, p.pre_first_previous, p.pre_first_feedback}, st_.pre_first, x, n);
		run_op({p.pre_second_current, p.pre_second_previous, p.pre_second_feedback}, st_.pre_second, x, n);
		run_tube({p.overdrive, 0.0f, p.tube_smooth, p.hard_mix, p.soft_input_gain}, st_.dc, x, z, n);

		delayed_by_one(z, zd, st_.z_prev, n);
		for (int s = 0; s < n; s++)
			c[s] = sat(p.cabinet_input_gain * fx_flow::table_read(sine_.data(), 2048.0f + 2048.0f * zd[s]));
		run_op({p.cabinet_0_current, p.cabinet_0_previous, p.cabinet_0_feedback}, st_.cab_op, c, n);
		run_bq({p.cabinet_1_current, p.cabinet_1_x1, p.cabinet_1_x2, p.cabinet_1_y1, p.cabinet_1_y2}, st_.cab_bq[0], c, n);
		run_bq({p.cabinet_2_current, p.cabinet_2_x1, p.cabinet_2_x2, p.cabinet_2_y1, p.cabinet_2_y2}, st_.cab_bq[1], c, n);
		run_bq({p.cabinet_3_current, p.cabinet_3_x1, p.cabinet_3_x2, p.cabinet_3_y1, p.cabinet_3_y2}, st_.cab_bq[2], c, n);
		for (int s = 0; s < n; s++) {
			const float amp = sat(p.cabinet_output_gain * c[s]);
			out_left[s] = sat(p.dry_gain[0] * inl[s] + p.wet_gain * amp);
			out_right[s] = sat(p.dry_gain[1] * inr[s] + p.wet_gain * amp);
		}
	}

private:
	struct state {
		drive_detail::op_state pre_first, pre_second, cab_op;
		drive_detail::bq_state cab_bq[3];
		float dc = 0, z_prev = 0;
	};
	amp_sim2_params p_{};
	state st_{};
	std::vector<float> sine_ = std::vector<float>(4098);
};

// ======================================================================
// TALK MOD
// ======================================================================
class talk_mod_flow {
public:
	talk_mod_flow() = default;
	void configure(const talk_mod_params &p) { p_ = p; }
	void reset() { st_ = state{}; }

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table, float *out_left,
		float *out_right, int n)
	{
		using namespace drive_detail;
		(void)lfo;
		(void)table;
		const talk_mod_params &p = p_;
		float inl[B], inr[B];
		std::copy_n(in_left, n, inl);
		std::copy_n(in_right, n, inr);

		float x[B];
		for (int s = 0; s < n; s++)
			x[s] = sat(p.input_from_left * inl[s] + p.input_from_right * inr[s]);
		run_bq({p.highpass_current, p.highpass_x1, p.highpass_x2, p.highpass_y1, p.highpass_y2}, st_.hp, x, n);
		for (int s = 0; s < n; s++)
			x[s] = sat(p.drive * x[s]);

		float smooth[B], level[B], g[B], y[B], sd[B], v[B];
		run_detector({p.detector, p.peak_input, p.peak_release, p.smooth_input, p.smooth_from_peak, p.smooth_from_input, p.smooth_feedback},
			st_.det, x, smooth, n);
		for (int s = 0; s < n; s++)
			level[s] = sat(p.threshold_gain * smooth[s]);
		knee_coef kc{};
		std::copy_n(p.knee_offset, 8, kc.offset);
		std::copy_n(p.knee_slope, 8, kc.slope);
		kc.base = p.knee_base;
		kc.gain_current = p.gain_current;
		kc.gain_previous = p.gain_previous;
		kc.gain_feedback = p.gain_feedback;
		run_knee(kc, st_.knee, level, g, n);
		for (int s = 0; s < n; s++)
			y[s] = sat(p.shape_dry * x[s] + p.shape_square * (g[s] * x[s]) * x[s]);
		run_op({p.shape_filter_current, p.shape_filter_previous, p.shape_filter_feedback}, st_.shape, y, n);
		delayed_by_one(y, sd, st_.shape_prev, n);

		for (int s = 0; s < n; s++)
			v[s] = sat(p.chain_from_left * inl[s] + p.chain_from_right * inr[s] + p.chain_from_shape * sd[s]);
		formant(0, v, n);
		formant(1, v, n);
		// 山 1 と山 2 の間に 1 サンプルの遅れ
		float shifted[B];
		delayed_by_one(v, shifted, st_.between, n);
		formant(2, shifted, n);
		formant(3, shifted, n);
		for (int s = 0; s < n; s++) {
			out_left[s] = sat(p.out_gain[0] * shifted[s]);
			out_right[s] = sat(p.out_gain[1] * shifted[s]);
		}
	}

private:
	struct state {
		drive_detail::bq_state hp;
		drive_detail::op_state shape;
		drive_detail::det_state det;
		drive_detail::knee_state knee;
		float band[4] = {}, low[4] = {};
		float shape_prev = 0, between = 0;
	};

	void formant(int i, float *sig, int n)
	{
		using drive_detail::sat;
		const talk_mod_params &p = p_;
		const float input = p.formant_input[i], damping = p.formant_damping[i], frequency = p.formant_frequency[i];
		const float band_state = p.formant_band_state[i], low_state = p.formant_low_state[i], band_out = p.formant_band_out[i];
		float band = st_.band[i], low = st_.low[i];
		for (int s = 0; s < n; s++) {
			const float high = sat(input * sig[s] - damping * band - low);
			band = sat(frequency * high + band_state * band);
			low = sat(frequency * band + low_state * low);
			sig[s] = sat(low + band_out * band + high);
		}
		st_.band[i] = band;
		st_.low[i] = low;
	}

	talk_mod_params p_{};
	state st_{};
};

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_DRIVE_H
