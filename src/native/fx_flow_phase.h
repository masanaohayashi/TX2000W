// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/phaser.md, dyna.md, v_flanger.md のフローから書いたエフェクト。

#ifndef S_MU2000_NATIVE_FX_FLOW_PHASE_H
#define S_MU2000_NATIVE_FX_FLOW_PHASE_H

#include "native/fx_flow_parts.h"
#include "native/fx_params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smu2000::native_fx {

namespace phase_detail {

using fx_flow::sat;
constexpr int B = fx_flow::max_block;

// 低域の棚 → 高域の棚（どちらも README の 1 次フィルタ）。入力の大きさ（gain）つき。
struct shelf_coef {
	float gain = 0, low_previous = 0, low_feedback = 0, low_current = 0;
	float high_previous = 0, high_feedback = 0, high_current = 0;
};
struct shelf_state {
	float low_x = 0, low_y = 0, high_x = 0, high_y = 0;
};

inline void run_shelves(const shelf_coef &k, shelf_state &st, const float *in, float *out, int n)
{
	float lx = st.low_x, ly = st.low_y, hx = st.high_x, hy = st.high_y;
	for (int s = 0; s < n; s++) {
		const float xi = fx_flow::settled_24bit(sat(k.gain * in[s]));
		const float yl = fx_flow::settled_24bit(sat(k.low_current * xi + k.low_previous * lx + k.low_feedback * ly));
		lx = xi;
		ly = yl;
		const float yh = fx_flow::settled_24bit(sat(k.high_current * yl + k.high_previous * hx + k.high_feedback * hy));
		hx = yl;
		hy = yh;
		out[s] = yh;
	}
	st.low_x = lx;
	st.low_y = ly;
	st.high_x = hx;
	st.high_y = hy;
}

// 6 段の 1 次オールパス。Y[0] = 1 つ前の鎖の入力、Y[k] = 1 つ前の段 k の出力。
inline void chain6(float a, float in, float *Y)
{
	float x = in, x_previous = Y[0];
	Y[0] = in;
	for (int k = 1; k <= 6; k++) {
		const float y_previous = Y[k];
		const float y = sat(a * x - a * y_previous + x_previous);
		Y[k] = y;
		x = y;
		x_previous = y_previous;
	}
}

inline float quartic(float u)
{
	const float u2 = sat(fx_flow::stepped(u) * u);
	return fx_flow::stepped(u2) * u2;
}

// ---- dyna 共通の入口と音量検出 ----
struct dyna_front_coef {
	shelf_coef shelf[2];
	float sensitivity, threshold, difference_scale, attack_speed, release_speed, release_reset, release_step;
	float level_gain, level_offset, direction_up, direction_down, direction_center;
};
struct dyna_front_state {
	shelf_state shelf[2];
	float follow = 0;  // e
	float release = 0; // r
};

template <class P>
dyna_front_coef make_front(const P &p)
{
	dyna_front_coef k{};
	for (int ch = 0; ch < 2; ch++)
		k.shelf[ch] = {p.input_gain[ch], p.low_shelf_previous[ch], p.low_shelf_feedback[ch], p.low_shelf_current[ch],
			p.high_shelf_previous[ch], p.high_shelf_feedback[ch], p.high_shelf_current[ch]};
	k.sensitivity = p.sensitivity;
	k.threshold = p.threshold;
	k.difference_scale = p.difference_scale;
	k.attack_speed = p.attack_speed;
	k.release_speed = p.release_speed;
	k.release_reset = p.release_reset;
	k.release_step = p.release_step;
	k.level_gain = p.level_gain;
	k.level_offset = p.level_offset;
	k.direction_up = p.direction_up;
	k.direction_down = p.direction_down;
	k.direction_center = p.direction_center;
	return k;
}

// x0, x1: 棚を通した入力。c = sat(direction_up × u + direction_down × u2)
template <bool phase>
inline void dyna_front_run(const dyna_front_coef &k, dyna_front_state &st, const float *in_left, const float *in_right,
	float *x0, float *x1, float *c, int n)
{
	run_shelves(k.shelf[0], st.shelf[0], in_left, x0, n);
	run_shelves(k.shelf[1], st.shelf[1], in_right, x1, n);
	float e = st.follow, r = st.release;
	for (int s = 0; s < n; s++) {
		float level = std::max(std::min(fx_flow::sat_max, std::fabs(k.sensitivity * x0[s])), std::min(fx_flow::sat_max, std::fabs(k.sensitivity * x1[s])));
		if (k.threshold - level > 0.0f)
			level = 0.0f;
		const float d = sat(k.difference_scale * level - k.difference_scale * e);
		float speed;
		if (d < 0.0f) {
			r = fx_flow::settled_24bit(sat(r + k.release_step * r));
			speed = fx_flow::settled_24bit(k.release_speed * r);
		} else {
			r = k.release_reset;
			speed = k.attack_speed;
		}
		e = fx_flow::settled_24bit(sat(e + speed * fx_flow::stepped(d)));
		float u, u2;
		if (phase) {
			u = sat(k.level_offset + fx_flow::positive(k.level_gain * e));
			u2 = sat(k.direction_center + u);
		} else {
			u = sat(fx_flow::positive(k.level_offset + fx_flow::positive(k.level_gain * e)));
			u2 = fx_flow::positive(k.direction_center - u);
		}
		u = fx_flow::settled_24bit(u);
		u2 = fx_flow::settled_24bit(u2);
		c[s] = sat(k.direction_up * u + k.direction_down * u2);
	}
	st.follow = e;
	st.release = r;
}

} // namespace phase_detail

// ============================================================ PHASER 1/2, T.PHASER
class phaser_flow {
public:
	phaser_flow() { reset(); }
	void configure(const phaser_params &p) { params = p; }
	void reset()
	{
		shelf[0] = {};
		shelf[1] = {};
		for (int k = 0; k < 7; k++)
			stage_left[k] = stage_right[k] = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace phase_detail;
		(void)table;
		const phaser_params &p = params;
		float L[B], R[B], a_left[B], a_right[B];
		run_shelves(coef(0), shelf[0], in_left, L, n);
		run_shelves(coef(1), shelf[1], in_right, R, n);
		for (int s = 0; s < n; s++) {
			const float phase_left = lfo[0][s];
			const float phase_right = fx_flow::wrap(lfo[0][s] + p.right_lfo_offset);
			a_left[s] = fx_flow::stepped(sat(quartic(sat(p.sweep_center[0] + std::fabs(sat(p.sweep_depth[0] * phase_left)))) - p.sweep_offset[0]));
			a_right[s] = fx_flow::stepped(sat(quartic(sat(p.sweep_center[1] + std::fabs(sat(p.sweep_depth[1] * phase_right)))) - p.sweep_offset[1]));
		}
		float yl[7], yr[7];
		for (int k = 0; k < 7; k++) {
			yl[k] = stage_left[k];
			yr[k] = stage_right[k];
		}
		for (int s = 0; s < n; s++) {
			const float tap_left = sat(p.left_tap[0] * yl[2] + p.left_tap[1] * yl[3] + p.left_tap[2] * yl[4] + p.left_tap[3] * yl[5] + p.left_tap[4] * yl[6]);
			const float tap_right = sat(p.right_tap[0] * yr[2] + p.right_tap[1] * yr[3] + p.right_tap[2] * yr[4] + p.right_tap[3] * yr[5] + p.right_tap[4] * yr[6]);
			const float wet_left = sat(p.wet_left_from_left_tap * tap_left + p.wet_left_from_right_tap * tap_right
				+ p.wet_left_from_right_stage1 * yr[1] + p.wet_left_from_right_stage5 * yr[5]);
			const float chain_left = sat(p.left_chain_from_left * L[s] + p.left_chain_from_right * R[s]
				+ p.left_chain_from_left_tap * tap_left + p.left_chain_from_right_tap * tap_right);
			const float chain_right = sat(p.right_chain_from_right * R[s]
				+ p.right_chain_from_right_tap * tap_right + p.right_chain_from_left_tap * tap_left);
			chain6(a_left[s], chain_left, yl);
			chain6(a_right[s], chain_right, yr);
			out_left[s] = sat(p.dry_gain[0] * L[s] + p.wet_gain[0] * wet_left);
			out_right[s] = sat(p.dry_gain[1] * R[s] + p.wet_gain[1] * tap_right);
		}
		for (int k = 0; k < 7; k++) {
			stage_left[k] = yl[k];
			stage_right[k] = yr[k];
		}
	}

private:
	phase_detail::shelf_coef coef(int ch) const
	{
		return {params.input_gain[ch], params.low_shelf_previous[ch], params.low_shelf_feedback[ch], params.low_shelf_current[ch],
			params.high_shelf_previous[ch], params.high_shelf_feedback[ch], params.high_shelf_current[ch]};
	}
	phaser_params params{};
	phase_detail::shelf_state shelf[2];
	float stage_left[7], stage_right[7];
};

// ============================================================ システムコーラスの PHASER
class system_phaser_flow {
public:
	system_phaser_flow() { reset(); }
	void configure(const system_phaser_params &p) { params = p; }
	void reset()
	{
		shelf = {};
		for (int k = 0; k < 7; k++)
			stage[k] = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace phase_detail;
		(void)table;
		const system_phaser_params &p = params;
		float mono[B], x[B], a[B];
		for (int s = 0; s < n; s++) {
			mono[s] = sat(p.input_from_left * in_left[s] + p.input_from_right * in_right[s]);
			a[s] = fx_flow::stepped(sat(quartic(sat(p.sweep_center + p.sweep_depth * lfo[1][s])) - p.sweep_offset));
		}
		const shelf_coef k{1.0f, p.low_shelf_previous, p.low_shelf_feedback, p.low_shelf_current,
			p.high_shelf_previous, p.high_shelf_feedback, p.high_shelf_current};
		run_shelves(k, shelf, mono, x, n);
		float Y[7];
		for (int i = 0; i < 7; i++)
			Y[i] = stage[i];
		for (int s = 0; s < n; s++) {
			const float first = sat(p.first_tap[0] * Y[3] + p.first_tap[1] * Y[4] + p.first_tap[2] * Y[5]);
			const float second = sat(p.second_tap[0] * Y[4] + p.second_tap[1] * Y[5] + p.second_tap[2] * Y[6]);
			chain6(a[s], sat(p.chain_input_gain * x[s] + p.chain_feedback * second), Y);
			out_left[s] = sat(p.left_from_first * first + p.left_from_second * second);
			out_right[s] = sat(p.right_from_second * second);
		}
		for (int i = 0; i < 7; i++)
			stage[i] = Y[i];
	}

private:
	system_phaser_params params{};
	phase_detail::shelf_state shelf;
	float stage[7];
};

// ============================================================ DYNA FLT
class dyna_filter_flow {
public:
	dyna_filter_flow() { reset(); }
	void configure(const dyna_filter_params &p)
	{
		params = p;
		front = phase_detail::make_front(p);
	}
	void reset()
	{
		front_state = {};
		for (auto &c : ch)
			c = {};
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace phase_detail;
		(void)lfo;
		(void)table;
		const dyna_filter_params &p = params;
		float x[2][B], c[B], f[B], q[B], t[B];
		dyna_front_run<false>(front, front_state, in_left, in_right, x[0], x[1], c, n);
		for (int s = 0; s < n; s++) {
			const float damping = sat(p.resonance * sat(1.0f + p.damping_from_control * c[s]) + p.damping_offset);
			const float v2 = sat(fx_flow::stepped(c[s]) * c[s]);
			const float ff = fx_flow::positive(c[s] + fx_flow::stepped(sat(p.cutoff_from_damping * sat(-2.0f * damping))) * v2);
			f[s] = fx_flow::stepped(ff);
			q[s] = fx_flow::stepped(sat(ff + sat(damping - ff)));
			t[s] = fx_flow::stepped(fx_flow::positive(4.0f * (p.lowpass_from_square * v2 + c[s])));
		}
		float *const out[2] = {out_left, out_right};
		for (int k = 0; k < 2; k++) {
			state st = ch[k];
			for (int s = 0; s < n; s++) {
				float mix = sat(p.dry_gain[k] * st.x);
				mix = sat(p.from_input[k] * st.input + mix);
				mix = sat(p.from_high[k] * st.high + mix);
				mix = sat(p.from_band[k] * st.band + mix);
				mix = sat(p.from_low[k] * st.low + mix);
				mix = sat(p.from_lowpass1[k] * st.lp1 + mix);
				out[k][s] = sat(4.0f * (p.from_lowpass2[k] * st.lp2 + sat(mix)));
				st.input = sat(q[s] * st.band + x[k][s]);
				st.high = sat(st.input - st.low);
				st.band = sat(4.0f * f[s] * st.high + p.band_keep * st.band);
				st.low = sat(f[s] * st.band + st.low);
				st.lp1 = st.lp1 + t[s] * (st.low - st.lp1);
				st.lp2 = st.lp2 + t[s] * (st.lp1 - st.lp2);
				st.x = x[k][s];
			}
			ch[k] = st;
		}
	}

private:
	struct state {
		float x = 0, input = 0, high = 0, band = 0, low = 0, lp1 = 0, lp2 = 0;
	};
	dyna_filter_params params{};
	phase_detail::dyna_front_coef front{};
	phase_detail::dyna_front_state front_state;
	state ch[2];
};

// ============================================================ DYNA FLANG
class dyna_flanger_flow {
public:
	dyna_flanger_flow() { reset(); }
	void configure(const dyna_flanger_params &p)
	{
		params = p;
		front = phase_detail::make_front(p);
	}
	void reset()
	{
		front_state = {};
		for (int k = 0; k < 2; k++) {
			line[k].clear();
			previous_voice[k] = 0.0f;
		}
		position = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace phase_detail;
		(void)lfo;
		(void)table;
		const dyna_flanger_params &p = params;
		float x[2][B], c[B];
		dyna_front_run<false>(front, front_state, in_left, in_right, x[0], x[1], c, n);
		float *const out[2] = {out_left, out_right};
		// 遅れの一番短い値が区切りの長さ（帰還が voice の 1 サンプル前を使う）
		const int base0 = std::max(1, int(p.voice_base_delay[0]));
		const int base1 = std::max(1, int(p.voice_base_delay[1]));
		const int chunk_max = std::min(base0, base1);
		for (int done = 0; done < n;) {
			const int m = std::min(n - done, chunk_max);
			const uint32_t chunk_pos = position + uint32_t(done);
			for (int k = 0; k < 2; k++) {
				const int base = k ? base1 : base0;
				float v[B + 1], w[B];
				v[0] = previous_voice[k];
				for (int s = 0; s < m; s++) {
					const float swing = p.voice_depth[k] * c[done + s];
					const int whole = int(swing);
					const float fraction = swing - float(whole);
					const uint32_t at = chunk_pos + uint32_t(s) - uint32_t(base + whole);
					const float a = line[k].at(at);
					const float b = line[k].at(at - 1u);
					v[1 + s] = sat(a + fraction * (b - a));
				}
				for (int s = 0; s < m; s++) {
					w[s] = fx_flow::word(sat(p.line_input[k] * x[k][done + s] + p.line_feedback[k] * v[s]));
					out[k][done + s] = sat(p.dry_gain[k] * x[k][done + s] + p.wet_gain[k] * v[1 + s]);
				}
				line[k].write(chunk_pos, w, m);
				previous_voice[k] = v[m];
			}
			done += m;
		}
		position += uint32_t(n);
	}

private:
	dyna_flanger_params params{};
	phase_detail::dyna_front_coef front{};
	phase_detail::dyna_front_state front_state;
	fx_flow::delay_line<4096> line[2];
	float previous_voice[2];
	uint32_t position = 0;
};

// ============================================================ DYNA PHASE
class dyna_phaser_flow {
public:
	dyna_phaser_flow() { reset(); }
	void configure(const dyna_phaser_params &p)
	{
		params = p;
		front = phase_detail::make_front(p);
	}
	void reset()
	{
		front_state = {};
		for (auto &y : stage)
			for (float &v : y)
				v = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace phase_detail;
		(void)lfo;
		(void)table;
		const dyna_phaser_params &p = params;
		float x[2][B], a[B];
		dyna_front_run<true>(front, front_state, in_left, in_right, x[0], x[1], a, n);
		for (int s = 0; s < n; s++)
			a[s] = fx_flow::stepped(a[s]);
		float *const out[2] = {out_left, out_right};
		float Y[2][7];
		for (int k = 0; k < 2; k++)
			for (int i = 0; i < 7; i++)
				Y[k][i] = stage[k][i];
		for (int s = 0; s < n; s++) {
			for (int k = 0; k < 2; k++) {
				const float tap = sat(p.tap_stage4[k] * Y[k][4] + p.tap_stage5[k] * Y[k][5] + p.tap_stage6[k] * Y[k][6]);
				chain6(a[s], sat(p.chain_input[k] * x[k][s] + p.chain_feedback[k] * tap), Y[k]);
				out[k][s] = sat(p.dry_gain[k] * x[k][s] + p.wet_gain[k] * tap);
			}
		}
		for (int k = 0; k < 2; k++)
			for (int i = 0; i < 7; i++)
				stage[k][i] = Y[k][i];
	}

private:
	dyna_phaser_params params{};
	phase_detail::dyna_front_coef front{};
	phase_detail::dyna_front_state front_state;
	float stage[2][7];
};

// ============================================================ DYNA RING
class dyna_ring_flow {
public:
	dyna_ring_flow() { reset(); }
	void configure(const dyna_ring_params &p)
	{
		params = p;
		front = phase_detail::make_front(p);
	}
	void reset()
	{
		front_state = {};
		phase = {};
		for (auto &c : ch)
			c = {};
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace phase_detail;
		(void)lfo;
		const dyna_ring_params &p = params;
		float x[2][B], c[B], carrier[B];
		dyna_front_run<false>(front, front_state, in_left, in_right, x[0], x[1], c, n);
		fx_flow::phase_word phi = phase;
		const float center = float(p.carrier_read_center);
		for (int s = 0; s < n; s++) {
			carrier[s] = fx_flow::stepped(sat(p.carrier_gain * fx_flow::table_read(table, center + p.carrier_read_scale * phi.value())));
			phi.add(c[s]);
		}
		phase = phi;
		float *const out[2] = {out_left, out_right};
		for (int k = 0; k < 2; k++) {
			state st = ch[k];
			for (int s = 0; s < n; s++) {
				const float d = sat(p.band_input[k] * x[k][s]);
				const float lp = sat(p.lowpass_current[k] * d + p.lowpass_x1[k] * st.lp_x1 + p.lowpass_x2[k] * st.lp_x2
					+ p.lowpass_y1[k] * st.lp_y1 + p.lowpass_y2[k] * st.lp_y2);
				st.lp_x2 = st.lp_x1;
				st.lp_x1 = d;
				st.lp_y2 = st.lp_y1;
				st.lp_y1 = lp;
				const float y = sat(p.highpass_current[k] * lp + p.highpass_x1[k] * st.hp_x1 + p.highpass_x2[k] * st.hp_x2
					+ p.highpass_y1[k] * st.hp_y1 + p.highpass_y2[k] * st.hp_y2);
				st.hp_x2 = st.hp_x1;
				st.hp_x1 = lp;
				st.hp_y2 = st.hp_y1;
				st.hp_y1 = y;
				out[k][s] = sat(p.dry_gain[k] * d + p.wet_gain[k] * (carrier[s] * y));
			}
			ch[k] = st;
		}
	}

private:
	struct state {
		float lp_x1 = 0, lp_x2 = 0, lp_y1 = 0, lp_y2 = 0, hp_x1 = 0, hp_x2 = 0, hp_y1 = 0, hp_y2 = 0;
	};
	dyna_ring_params params{};
	phase_detail::dyna_front_coef front{};
	phase_detail::dyna_front_state front_state;
	fx_flow::phase_word phase;
	state ch[2];
};

// ============================================================ V-FLANGER
class v_flanger_flow {
public:
	v_flanger_flow() { reset(); }
	void configure(const v_flanger_params &p) { params = p; }
	void reset()
	{
		for (int k = 0; k < 2; k++) {
			line[k].clear();
			ch[k] = {};
		}
		phase = {};
		position = 0;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		using namespace phase_detail;
		(void)lfo;
		const v_flanger_params &p = params;
		const float *const in[2] = {in_left, in_right};
		float *const out[2] = {out_left, out_right};
		float x[2][B], m[2][B], a[2][B], carr[2][B + 1], frac[2][B];
		int whole[2][B];

		// 入口の棚と山
		for (int k = 0; k < 2; k++) {
			state &st = ch[k];
			float lx = st.low_x, ly = st.low_y, hx = st.high_x, hy = st.high_y;
			float mx1 = st.mid_x1, mx2 = st.mid_x2, my1 = st.mid_y1, my2 = st.mid_y2;
			for (int s = 0; s < n; s++) {
				const float xi = sat(p.input_gain[k] * in[k][s]);
				const float yl = sat(p.low_shelf_current[k] * xi + p.low_shelf_previous[k] * lx + p.low_shelf_feedback[k] * ly);
				lx = xi;
				ly = yl;
				const float ym = sat(p.mid_current[k] * yl + p.mid_x1[k] * mx1 + p.mid_x2[k] * mx2 + p.mid_y1[k] * my1 + p.mid_y2[k] * my2);
				mx2 = mx1;
				mx1 = yl;
				my2 = my1;
				my1 = ym;
				const float yh = sat(p.high_shelf_current[k] * ym + p.high_shelf_previous[k] * hx + p.high_shelf_feedback[k] * hy);
				hx = ym;
				hy = yh;
				x[k][s] = yh;
			}
			st.low_x = lx;
			st.low_y = ly;
			st.high_x = hx;
			st.high_y = hy;
			st.mid_x1 = mx1;
			st.mid_x2 = mx2;
			st.mid_y1 = my1;
			st.mid_y2 = my2;
		}

		// LFO（三角）と平滑化
		{
			fx_flow::phase_word word0 = phase;
			float smooth_left = ch[0].smooth, smooth_right = ch[1].smooth;
			const double step = double(p.phase_rate_gain) * double(p.phase_rate);
			for (int s = 0; s < n; s++) {
				word0.add(step);
				const float phi0 = word0.value();
				const float phi1 = fx_flow::wrap(p.right_phase_offset + phi0);
				const float target0 = sat(p.triangle_gain[0] * sat(2.0f * (p.triangle_offset[0] + std::fabs(phi0))));
				const float target1 = sat(p.triangle_gain[1] * sat(2.0f * (p.triangle_offset[1] + std::fabs(phi1))));
				smooth_left = fx_flow::settled_24bit(sat(smooth_left + p.lfo_smooth * (target0 - smooth_left)));
				smooth_right = fx_flow::settled_24bit(sat(smooth_right + p.lfo_smooth * (target1 - smooth_right)));
				m[0][s] = smooth_left;
				m[1][s] = smooth_right;
			}
			phase = word0;
			ch[0].smooth = smooth_left;
			ch[1].smooth = smooth_right;
		}

		// 遅れの曲線。遅れには 1 サンプル前の曲線を使う。
		int shortest = 1 << 30;
		for (int k = 0; k < 2; k++) {
			carr[k][0] = ch[k].curve;
			for (int s = 0; s < n; s++) {
				const float pos = fx_flow::positive((p.curve_read_center + p.curve_read_scale * m[k][s]) * (1.0f / 32768.0f)) * 32768.0f;
				carr[k][s + 1] = fx_flow::table_read(table, float(p.curve_table_start[k]) + pos);
				a[k][s] = fx_flow::stepped(sat(p.allpass_from_lfo[k] * m[k][s] + p.allpass_base));
			}
			ch[k].curve = carr[k][n];
			for (int s = 0; s < n; s++) {
				const float delay = p.delay_scale[k] * carr[k][s];
				whole[k][s] = int(delay);
				frac[k][s] = delay - float(whole[k][s]);
				shortest = std::min(shortest, whole[k][s]);
			}
		}
		const int chunk_max = std::max(1, shortest);

		for (int done = 0; done < n;) {
			const int len = std::min(n - done, chunk_max);
			const uint32_t chunk_pos = position + uint32_t(done);
			float v[2][B];
			for (int k = 0; k < 2; k++)
				for (int s = 0; s < len; s++) {
					const uint32_t at = chunk_pos + uint32_t(s) - uint32_t(whole[k][done + s]);
					const float lo = line[k].at(at);
					const float hi = line[k].at(at - 1u);
					v[k][s] = sat(lo + frac[k][done + s] * (hi - lo));
				}
			for (int s = 0; s < len; s++) {
				for (int k = 0; k < 2; k++) {
					state &st = ch[k];
					const float xs = x[k][done + s];
					const float h = sat(p.voice_gain[k] * v[k][s] - st.lp);
					const float lp = sat(p.damp_speed[k] * h + st.lp);
					const float ap = sat(a[k][done + s] * lp + a[k][done + s] * st.ap - st.lp);
					const float y = sat(p.dc_block_current[k] * ap + p.dc_block_previous[k] * st.ap + p.dc_block_feedback[k] * st.y);
					const float b = sat(p.line_feedback[k] * st.y + p.line_input[k] * xs);
					line[k].put(chunk_pos + uint32_t(s), fx_flow::word(sat(std::fabs(b) * b - 2.0f * b)));
					out[k][done + s] = sat(p.dry_gain * xs + p.wet_gain * y);
					st.lp = lp;
					st.ap = ap;
					st.y = y;
				}
			}
			done += len;
		}
		position += uint32_t(n);
	}

private:
	struct state {
		float low_x = 0, low_y = 0, high_x = 0, high_y = 0;
		float mid_x1 = 0, mid_x2 = 0, mid_y1 = 0, mid_y2 = 0;
		float smooth = 0, curve = 0, lp = 0, ap = 0, y = 0;
	};
	v_flanger_params params{};
	fx_flow::delay_line<8192> line[2];
	state ch[2];
	fx_flow::phase_word phase;
	uint32_t position = 0;
};

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_PHASE_H
