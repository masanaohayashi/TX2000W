// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/ のフローから書いたエフェクト（ISOLATOR、MULTI COMP、LO-FI、SLICE）。

#ifndef S_MU2000_NATIVE_FX_FLOW_MISC_H
#define S_MU2000_NATIVE_FX_FLOW_MISC_H

#include "fx_flow_parts.h"
#include "fx_params.h"

namespace smu2000::native_fx {

namespace misc_detail {

using fx_flow::sat;

// README の 1 次フィルタ
struct first_order {
	float x1 = 0.0f, y1 = 0.0f;
	inline float step(float current, float previous, float feedback, float x)
	{
		const float y = sat(current * x + previous * x1 + feedback * y1);
		x1 = x;
		y1 = y;
		return y;
	}
};

// README の 2 次フィルタ
struct second_order {
	float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
	inline float step(float current, float cx1, float cx2, float cy1, float cy2, float x)
	{
		const float y = sat(current * x + cx1 * x1 + cx2 * x2 + cy1 * y1 + cy2 * y2);
		x2 = x1;
		x1 = x;
		y2 = y1;
		y1 = y;
		return y;
	}
};

} // namespace misc_detail

// ---------------------------------------------------------------- ISOLATOR
class isolator_flow {
public:
	isolator_flow() { reset(); }
	void configure(const isolator_params &params)
	{
		p = params;
		for (int c = 0; c < 2; c++) {
			const float d = c == 0 ? p.low_band_delay : p.right_low_band_delay;
			delay[c] = std::clamp(int(std::floor(d + 0.5f)), 1, line_length - 1);
		}
	}
	void reset()
	{
		state[0] = {};
		state[1] = {};
	}

	void process(const float *in_left, const float *in_right, const float *const[6], const float *, float *out_left,
		float *out_right, int n)
	{
		run(0, in_left, out_left, n);
		run(1, in_right, out_right, n);
	}

private:
	static constexpr int line_length = 16;
	struct split_state {
		float s1 = 0.0f, s2 = 0.0f;
	};
	struct channel_state {
		split_state low_split, high_split;
		misc_detail::second_order above_low, low_band, high_band;
		float mid_w1 = 0.0f, mid_w2 = 0.0f;
		float line[line_length] = {};
		uint32_t position = 0;
	};

	static inline void split_step(split_state &st, float x, float fb_a, float fb_b, float sign, float input_gain,
		float stage1, float stage2, float &high_side, float &low_side)
	{
		using fx_flow::sat;
		const float fb = sat(fb_a * st.s1 + 2.0f * st.s2 + fb_b * st.s1);
		const float h = sat(x + sign * fb);
		high_side = sat(input_gain * h);
		const float s1 = sat(stage1 * high_side + st.s1);
		const float s2 = sat(stage2 * sat(s1 + st.s1) + st.s2);
		low_side = sat(s2 + st.s2);
		st.s1 = s1;
		st.s2 = s2;
	}

	void run(int c, const float *in, float *out, int n)
	{
		using fx_flow::sat;
		channel_state st = state[c];
		const int d = delay[c];
		for (int s = 0; s < n; s++) {
			const float x = in[s];
			float g, l, u, ug, ul, mid_in;
			split_step(st.low_split, x, p.low_fb_a[c], p.low_fb_b[c], p.low_sign[c], p.low_input_gain[c],
				p.low_stage1[c], p.low_stage2[c], g, l);
			u = st.above_low.step(p.above_low_current[c], p.above_low_x1[c], p.above_low_x2[c], p.above_low_y1[c],
				p.above_low_y2[c], g);
			split_step(st.high_split, u, p.mid_fb_a[c], p.mid_fb_b[c], p.mid_sign[c], p.mid_input_gain[c],
				p.mid_stage1[c], p.mid_stage2[c], ug, ul);
			mid_in = ul;
			const float low_band =
				st.low_band.step(p.low_band_current[c], p.low_band_x1[c], p.low_band_x2[c], p.low_band_y1[c],
					p.low_band_y2[c], l);
			const float w = sat(p.mid_band_current[c] * mid_in + p.mid_band_y1[c] * st.mid_w1 +
				p.mid_band_y2[c] * st.mid_w2);
			const float mid = sat(w + p.mid_band_x1[c] * st.mid_w1 + p.mid_band_x2[c] * st.mid_w2);
			st.mid_w2 = st.mid_w1;
			st.mid_w1 = w;
			const float high = st.high_band.step(p.high_band_current[c], p.high_band_x1[c], p.high_band_x2[c],
				p.high_band_y1[c], p.high_band_y2[c], ug);
			const float low = st.line[(st.position - uint32_t(d)) & (line_length - 1)];
			st.line[st.position & (line_length - 1)] = fx_flow::word(low_band);
			st.position++;
			out[s] = sat(p.dry_gain[c] * x + p.low_gain[c] * low + p.mid_gain[c] * mid + p.high_gain[c] * high);
		}
		state[c] = st;
	}

	isolator_params p;
	int delay[2] = {5, 5};
	channel_state state[2];
};

// ---------------------------------------------------------------- MULTI COMP
class multi_comp_flow {
public:
	multi_comp_flow() { reset(); }
	void configure(const multi_comp_params &params) { p = params; }
	void reset() { state = {}; }

	void process(const float *in_left, const float *in_right, const float *const[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using fx_flow::sat;
		using fx_flow::table_read;
		const float *in[2] = {in_left, in_right};
		float *out[2] = {out_left, out_right};
		float pre[2][fx_flow::max_block];
		state_t st = state;
		for (int s = 0; s < n; s++) {
			float low_prev[2], mid_prev[2], high[2];
			for (int c = 0; c < 2; c++) {
				const float x = st.highpass[c].step(p.highpass_current[c], p.highpass_previous[c],
					p.highpass_feedback[c], sat(p.input_gain[c] * in[c][s]));
				low_prev[c] = st.low[c];
				mid_prev[c] = st.mid[c];
				const float l = sat(x - st.low[c]);
				const float h = sat(l - st.mid[c]);
				st.low[c] = sat(p.low_speed[c] * l + p.low_keep[c] * st.low[c]);
				st.mid[c] = sat(p.mid_speed[c] * h + p.mid_keep[c] * st.mid[c]);
				high[c] = h;
			}
			float level[3];
			level[0] = std::max(std::min(fx_flow::sat_max, std::fabs(p.detector_low[0] * low_prev[0])),
				std::min(fx_flow::sat_max, std::fabs(p.detector_low[1] * low_prev[1])));
			level[1] = std::max(std::min(fx_flow::sat_max, std::fabs(p.detector_mid[0] * mid_prev[0])),
				std::min(fx_flow::sat_max, std::fabs(p.detector_mid[1] * mid_prev[1])));
			level[2] = std::max(std::min(fx_flow::sat_max, std::fabs(p.detector_high[0] * high[0])),
				std::min(fx_flow::sat_max, std::fabs(p.detector_high[1] * high[1])));
			float gain[3];
			float difference[3];
			for (int b = 0; b < 3; b++)
				difference[b] = p.envelope_input[b] * level[b] - p.envelope_state[b] * st.e[b];
			for (int b = 0; b < 3; b++) {
				const float d = difference[b];
				// 中の帯の速さは高い帯の向きで選ぶ（文書の「追従」）
				const float speed = difference[b == 1 ? 2 : b] < 0.0f ? fx_flow::stepped(p.release_speed[b] * st.r[b]) : p.attack_speed[b];
				if (d < 0.0f)
					st.r[b] = sat(st.r[b] + p.release_step * st.r[b]);
				else
					st.r[b] = sat(p.release_reset[b] + p.release_clear[b] * st.r[b] + st.r[b]);
				st.e[b] = fx_flow::settled_24bit(sat(speed * d + p.envelope_keep[b] * st.e[b]));
				gain[b] = fx_flow::stepped(table_read(table, p.gain_read_center[b] + p.gain_read_scale[b] * st.e[b]));
			}
			for (int c = 0; c < 2; c++)
				pre[c][s] = sat(p.band_output[c][0] * gain[0] * low_prev[c] + p.band_output[c][1] * gain[1] * mid_prev[c] +
					p.band_output[c][2] * gain[2] * high[c]);
		}
		// 出力を 1 サンプル遅らせて柔らかい飽和の表へ
		for (int c = 0; c < 2; c++) {
			const float center = float(p.soft_read_center[c]);
			float previous[fx_flow::max_block];
			previous[0] = st.last_pre[c];
			for (int s = 1; s < n; s++)
				previous[s] = pre[c][s - 1];
			for (int s = 0; s < n; s++) {
				const float v = table_read(table, center + p.soft_read_scale[c] * previous[s]);
				out[c][s] = sat(p.out_gain[c] * sat(p.soft_output[c] * v));
			}
			st.last_pre[c] = pre[c][n - 1];
		}
		state = st;
	}

private:
	struct state_t {
		misc_detail::first_order highpass[2];
		float low[2] = {}, mid[2] = {};
		float e[3] = {}, r[3] = {};
		float last_pre[2] = {};
	};
	multi_comp_params p;
	state_t state;
};

// ---------------------------------------------------------------- LO-FI
class lo_fi_flow {
public:
	lo_fi_flow() { reset(); }
	void configure(const lo_fi_params &params) { p = params; }
	void reset() { state = {}; }

	void process(const float *in_left, const float *in_right, const float *const[6], const float *, float *out_left,
		float *out_right, int n)
	{
		using fx_flow::sat;
		state_t st = state;
		float follow[2][fx_flow::max_block];
		for (int s = 0; s < n; s++) {
			const float a_in = sat(p.input_a_from_left * in_left[s] + p.input_a_from_right * in_right[s]);
			const float b_in = p.input_b_from_right * in_right[s];
			const float a = st.pre_lowpass[0].step(p.pre_lowpass_current[0], p.pre_lowpass_x1[0], p.pre_lowpass_x2[0],
				p.pre_lowpass_y1[0], p.pre_lowpass_y2[0],
				st.pre_emphasis[0].step(p.pre_emphasis_current[0], p.pre_emphasis_previous[0],
					p.pre_emphasis_feedback[0], a_in));
			const float b = st.pre_lowpass[1].step(p.pre_lowpass_current[1], p.pre_lowpass_x1[1], p.pre_lowpass_x2[1],
				p.pre_lowpass_y1[1], p.pre_lowpass_y2[1],
				st.pre_emphasis[1].step(p.pre_emphasis_current[1], p.pre_emphasis_previous[1],
					p.pre_emphasis_feedback[1], b_in));
			st.counter = sat(st.counter + p.counter_step);
			if (st.counter - p.counter_limit >= 0.0f) {
				st.hold[0] = a;
				st.hold[1] = b;
				st.counter = 0.0f;
			}
			for (int c = 0; c < 2; c++) {
				const float e = sat(st.hold[c] + p.accumulator_sign * st.s[c]);
				float m;
				if (e - p.step_size >= 0.0f)
					m = p.step_size;
				else if (e + p.step_size < 0.0f)
					m = p.negative_step * p.step_size;
				else
					m = e;
				st.s[c] = sat(st.s[c] + m);
				follow[c][s] = st.s[c];
			}
		}
		float w[2][fx_flow::max_block];
		for (int c = 0; c < 2; c++) {
			float *v = follow[c];
			for (int s = 0; s < n; s++)
				v[s] = sat(sat(sat(v[s] * p.output_gain) * p.post_stage_gain) * p.post_stage_gain_2);
			for (int s = 0; s < n; s++)
				v[s] = st.post_first[c].step(p.post_first_current[c], p.post_first_previous[c],
					p.post_first_feedback[c], v[s]);
			for (int s = 0; s < n; s++)
				v[s] = st.post_second[c].step(p.post_second_current[c], p.post_second_x1[c], p.post_second_x2[c],
					p.post_second_y1[c], p.post_second_y2[c], v[s]);
			for (int s = 0; s < n; s++)
				v[s] = st.post_third[c].step(p.post_third_current[c], p.post_third_previous[c],
					p.post_third_feedback[c], v[s]);
			for (int s = 0; s < n; s++)
				w[c][s] = st.de_emphasis[c].step(p.de_emphasis_current[c], p.de_emphasis_previous[c],
					p.de_emphasis_feedback[c], v[s]);
		}
		// 出口は 1 サンプル前の w から
		float wet[2][fx_flow::max_block];
		for (int c = 0; c < 2; c++) {
			wet[c][0] = sat(p.wet_scale * st.last_w[c]);
			for (int s = 1; s < n; s++)
				wet[c][s] = sat(p.wet_scale * w[c][s - 1]);
			st.last_w[c] = w[c][n - 1];
		}
		for (int s = 0; s < n; s++) {
			out_left[s] = sat(p.left_from_input * in_left[s] + p.left_from_a * wet[0][s]);
			out_right[s] = sat(p.right_from_input * in_right[s] + p.right_from_b * wet[1][s] +
				p.right_from_a * wet[0][s]);
		}
		state = st;
	}

private:
	struct state_t {
		misc_detail::first_order pre_emphasis[2], post_first[2], post_third[2], de_emphasis[2];
		misc_detail::second_order pre_lowpass[2], post_second[2];
		float counter = 0.0f;
		float hold[2] = {}, s[2] = {}, last_w[2] = {};
	};
	lo_fi_params p;
	state_t state;
};

// ---------------------------------------------------------------- SLICE
class slice_flow {
public:
	slice_flow() { reset(); }
	void configure(const slice_params &params) { p = params; }
	void reset() { state = {}; }

	void process(const float *in_left, const float *in_right, const float *const[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using fx_flow::sat;
		using fx_flow::table_read;
		using fx_flow::wrap;
		state_t st = state;
		const float *in[2] = {in_left, in_right};
		float x[2][fx_flow::max_block];
		for (int c = 0; c < 2; c++)
			for (int s = 0; s < n; s++) {
				const float a = sat(p.input_gain[c] * in[c][s]);
				const float b = st.low_shelf[c].step(p.low_shelf_current[c], p.low_shelf_previous[c],
					p.low_shelf_feedback[c], a);
				const float h = st.high_shelf[c].step(p.high_shelf_current[c], p.high_shelf_previous[c],
					p.high_shelf_feedback[c], b);
				x[c][s] = sat(p.pre_gain[c] * h);
			}
		float phi[fx_flow::max_block];
		for (int s = 0; s < n; s++) {
			phi[s] = st.phase.value();
			st.phase.add(p.phase_increment);
		}
		float gate_g[fx_flow::max_block], pan0[fx_flow::max_block], pan1[fx_flow::max_block], aeg[fx_flow::max_block];
		for (int s = 0; s < n; s++) {
			const float ph = phi[s];
			const float divide = table_read(table, float(p.wave_read_center[0]) + p.wave_read_scale * ph);
			const float pan = table_read(table, float(p.wave_read_center[1]) + p.wave_read_scale * ph);
			const float aeg_wave =
				table_read(table, float(p.wave_read_center[2]) + p.wave_read_scale * wrap(ph - p.aeg_phase));
			const float length =
				table_read(table, float(p.wave_read_center[3]) + p.wave_read_scale * wrap(ph - p.gate_phase));
			const float gate = fx_flow::positive(p.gate_from_length * length + sat(p.gate_from_divide * divide));
			gate_g[s] = sat(p.gate_floor + gate);
			pan0[s] = p.pan_wave_gain * pan;
			pan1[s] = p.pan_center - pan0[s];
			aeg[s] = sat(p.aeg_center + p.aeg_depth * aeg_wave);
		}
		float ps0[fx_flow::max_block], ps1[fx_flow::max_block], a[fx_flow::max_block];
		for (int s = 0; s < n; s++)
			ps0[s] = st.pan_smooth[0].step(p.pan_smooth_current[0], p.pan_smooth_previous[0], p.pan_smooth_feedback[0],
				sat(fx_flow::stepped(gate_g[s]) * pan0[s]));
		for (int s = 0; s < n; s++)
			ps1[s] = st.pan_smooth[1].step(p.pan_smooth_current[1], p.pan_smooth_previous[1], p.pan_smooth_feedback[1],
				sat(fx_flow::stepped(gate_g[s]) * pan1[s]));
		for (int s = 0; s < n; s++)
			a[s] = st.aeg_smooth.step(p.aeg_smooth_current, p.aeg_smooth_previous, p.aeg_smooth_feedback, aeg[s]);
		float *out[2] = {out_left, out_right};
		for (int s = 0; s < n; s++) {
			const float t0 = fx_flow::stepped(ps0[s]), t1 = fx_flow::stepped(ps1[s]), ta = fx_flow::stepped(a[s]);
			const float l = sat(p.left_from_left * sat(t0 * x[0][s]) + p.left_from_right * sat(t1 * x[0][s]));
			const float r = sat(p.right_from_left * sat(t0 * x[1][s]) + p.right_from_right * sat(t1 * x[1][s]));
			const float drive_in[2] = {l, r};
			for (int c = 0; c < 2; c++) {
				float y = sat(p.drive_first[c] * sat(ta * drive_in[c]));
				y = sat(p.drive_second[c] * y);
				y = sat(p.drive_out[c] * y);
				out[c][s] = sat(p.dry_gain[c] * x[c][s] + p.wet_gain[c] * y);
			}
		}
		state = st;
	}

private:
	struct state_t {
		misc_detail::first_order low_shelf[2], high_shelf[2], pan_smooth[2], aeg_smooth;
		fx_flow::phase_word phase;
	};
	slice_params p;
	state_t state;
};

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_MISC_H
