// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/ のフローだけから書いた遅延系エフェクト
// (modulated_delay, chorus3, system_chorus3, early_reflection, variation_reverb, pitch_shift, system_detune)。

#ifndef S_MU2000_NATIVE_FX_FLOW_DELAY_H
#define S_MU2000_NATIVE_FX_FLOW_DELAY_H

#include "fx_flow_parts.h"
#include "fx_params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace smu2000::native_fx {

namespace fx_flow_delay_detail {

using fx_flow::max_block;
using fx_flow::sat;
using fx_flow::word;

// ---- 共通の 1 次フィルタと 2 次フィルタ ----

struct onepole_coef {
	float current = 0.0f, previous = 0.0f, feedback = 0.0f;
};
struct onepole_state {
	float x1 = 0.0f, y1 = 0.0f;
};

inline float onepole_step(const onepole_coef &c, onepole_state &s, float x)
{
	const float y = sat(c.current * x + c.previous * s.x1 + c.feedback * s.y1);
	s.x1 = x;
	s.y1 = y;
	return y;
}

struct biquad_coef {
	float current = 0.0f, x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};
struct biquad_state {
	float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};

inline float biquad_step(const biquad_coef &c, biquad_state &s, float x)
{
	const float y = sat(c.current * x + c.x1 * s.x1 + c.x2 * s.x2 + c.y1 * s.y1 + c.y2 * s.y2);
	s.x2 = s.x1;
	s.x1 = x;
	s.y2 = s.y1;
	s.y1 = y;
	return y;
}

// input_gain → [sat] → 低域の棚 → (中域の山) → 高域の棚
struct shelf_chain {
	onepole_coef low, high;
	biquad_coef mid;
	bool use_mid = true;
	onepole_state low_state, high_state;
	biquad_state mid_state;

	void reset()
	{
		low_state = {};
		high_state = {};
		mid_state = {};
	}

	void block(float input_gain, const float *in, float *out, int m)
	{
		const onepole_coef lo = low, hi = high;
		const biquad_coef mi = mid;
		const bool with_mid = use_mid;
		onepole_state ls = low_state, hs = high_state;
		biquad_state ms = mid_state;
		for (int s = 0; s < m; s++) {
			float x = sat(input_gain * in[s]);
			x = onepole_step(lo, ls, x);
			if (with_mid)
				x = biquad_step(mi, ms, x);
			out[s] = onepole_step(hi, hs, x);
		}
		low_state = ls;
		high_state = hs;
		mid_state = ms;
	}
};

// 2 次を 3 段
struct biquad_chain3 {
	biquad_coef c[3];
	biquad_state st[3];

	void reset() { st[0] = st[1] = st[2] = {}; }

	void block(const float *in, float *out, int m)
	{
		const biquad_coef c0 = c[0], c1 = c[1], c2 = c[2];
		biquad_state s0 = st[0], s1 = st[1], s2 = st[2];
		for (int s = 0; s < m; s++) {
			float x = biquad_step(c0, s0, in[s]);
			x = biquad_step(c1, s1, x);
			out[s] = biquad_step(c2, s2, x);
		}
		st[0] = s0;
		st[1] = s1;
		st[2] = s2;
	}
};

// ---- LFO を整える ----

struct shaper_coef {
	float offset = -1.0f, input_gain = 1.0f, linear = 0.5f, cubic = 0.0f, center = 0.5f;
	bool identity = true;

	void set(float offset_, float input_gain_, float linear_, float cubic_, float center_)
	{
		offset = offset_;
		input_gain = input_gain_;
		linear = linear_;
		cubic = cubic_;
		center = center_;
		identity = offset == -1.0f && input_gain == 1.0f && linear == 0.5f && cubic == 0.0f && center == 0.5f;
	}

	void block(const float *lfo, float *out, int n) const
	{
		if (identity) {
			std::copy_n(lfo, n, out);
			return;
		}
		const float o = offset, g = input_gain, l = linear, c = cubic, k = center;
		for (int s = 0; s < n; s++) {
			const float x = sat(g * sat(o + 2.0f * lfo[s]));
			out[s] = fx_flow::positive(k + c * x * x * x + sat(l * x));
		}
	}
};

// ---- 揺らす読み出し ----

// 揺れ swing[s] (0 以上、端数あり) を基本の遅れに足して読む。a = (base + 整数部) サンプル前、b = その 1 つ前。
template <uint32_t length>
inline void read_swung(const fx_flow::delay_line<length> &line, uint32_t position, int base, const float *swing,
	float *out, int n)
{
	for (int s = 0; s < n; s++) {
		const int whole = int(swing[s]);
		const float fraction = swing[s] - float(whole);
		const uint32_t index = position + uint32_t(s) - uint32_t(base) - uint32_t(whole);
		const float a = line.at(index);
		const float b = line.at(index - 1);
		out[s] = sat(a + fraction * (b - a));
	}
}

// ピッチの読み頭。遅れ 0 の a は無音（文書に決まりが無いので 0 とする）。
template <uint32_t length>
inline void read_head(const fx_flow::delay_line<length> &line, uint32_t position, int base, float depth,
	const float *phase, float *out, int n)
{
	for (int s = 0; s < n; s++) {
		const float swing = depth * phase[s];
		const int whole = int(swing);
		const float fraction = swing - float(whole);
		const int delay = base + whole;
		const uint32_t index = position + uint32_t(s) - uint32_t(delay);
		const float a_raw = line.at(index);
		const float a = delay > 0 ? a_raw : 0.0f;
		const float b = line.at(index - 1);
		out[s] = sat(a + fraction * (b - a));
	}
}

struct pitch_head_coef {
	int base_a = 0, base_b = 0;
	float depth_a = 0.0f, depth_b = 0.0f;
	float phase_center = 0.0f, phase_gain = 0.0f;
	float window_a_offset = 0.0f, window_b_offset = 0.0f;
	float window_a_gain = 0.0f, window_b_gain = 0.0f;
};

// phi[s]: 読み出しに使う のこぎり (0 以上 1 未満)。pitch[s] を作る。
template <uint32_t length>
inline void pitch_block(const fx_flow::delay_line<length> &line, uint32_t position, const pitch_head_coef &h,
	const float *phi, float *pitch, int n)
{
	float phase_b[max_block], g_a[max_block], g_b[max_block], a[max_block], b[max_block];
	const float center = h.phase_center, gain = h.phase_gain;
	const float wao = h.window_a_offset, wbo = h.window_b_offset, wag = h.window_a_gain, wbg = h.window_b_gain;
	for (int s = 0; s < n; s++) {
		const float saw = fx_flow::wrap(2.0f * phi[s]);
		const float magnitude = std::fabs(saw);
		phase_b[s] = fx_flow::positive(center + gain * saw);
		g_a[s] = sat(wag * fx_flow::positive(wao + magnitude));
		g_b[s] = sat(wbg * fx_flow::positive(wbo + magnitude));
	}
	read_head(line, position, h.base_a, h.depth_a, phi, a, n);
	read_head(line, position, h.base_b, h.depth_b, phase_b, b, n);
	for (int s = 0; s < n; s++)
		pitch[s] = sat(g_a[s] * a[s] + (1.0f - g_b[s]) * b[s]);
}

} // namespace fx_flow_delay_detail

// =====================================================================================================
// modulated_delay.md
// =====================================================================================================

class modulated_delay_flow {
public:
	modulated_delay_flow() { configure(modulated_delay_params{}); }

	void configure(const modulated_delay_params &p)
	{
		using namespace fx_flow_delay_detail;
		for (int ch = 0; ch < 2; ch++) {
			input_gain_[ch] = p.input_gain[ch];
			eq_[ch].low = {p.low_shelf_current[ch], p.low_shelf_previous[ch], p.low_shelf_feedback[ch]};
			eq_[ch].mid = {p.mid_current[ch], p.mid_x1[ch], p.mid_x2[ch], p.mid_y1[ch], p.mid_y2[ch]};
			eq_[ch].high = {p.high_shelf_current[ch], p.high_shelf_previous[ch], p.high_shelf_feedback[ch]};
			eq_[ch].use_mid = true;
			feedback_current_[ch] = p.feedback_filter_current[ch];
			feedback_previous_[ch] = p.feedback_filter_previous[ch];
			dry_gain_[ch] = p.dry_gain[ch];
			wet_gain_[ch] = p.wet_gain[ch];
		}
		for (int k = 0; k < 3; k++)
			shaper_[k].set(p.shaper_lfo_offset[k], p.shaper_input_gain[k], p.shaper_linear[k], p.shaper_cubic[k],
				p.shaper_center[k]);
		for (int v = 0; v < 4; v++) {
			base_[v] = p.voice_base_delay[v];
			second_[v] = p.voice_on_second_line[v] != 0;
			fast_depth_[v] = p.voice_fast_depth[v];
			slow_depth_[v] = p.voice_slow_depth[v];
		}
		first_from_left_ = p.first_line_from_left;
		first_from_right_ = p.first_line_from_right;
		first_from_left_feedback_ = p.first_line_from_left_feedback;
		first_from_right_feedback_ = p.first_line_from_right_feedback;
		second_from_right_ = p.second_line_from_right;
		second_from_left_feedback_ = p.second_line_from_left_feedback;
		second_from_right_feedback_ = p.second_line_from_right_feedback;
		for (int v = 0; v < 3; v++)
			wet_left_[v] = p.wet_left[v];
		for (int v = 0; v < 4; v++)
			wet_right_[v] = p.wet_right[v];

		// 使う声: 声 0 と 3 は帰還フィルタの状態のため、base が 0 でなければ常に計算する
		used_[0] = base_[0] > 0;
		used_[3] = base_[3] > 0;
		used_[1] = base_[1] > 0 && (wet_left_[1] != 0.0f || wet_right_[1] != 0.0f);
		used_[2] = base_[2] > 0 && (wet_left_[2] != 0.0f || wet_right_[2] != 0.0f);

		// 帰還のある声は書く前に読む。その遅れより長いブロックには区切る。
		feedback_voice_[0] = used_[0] && (first_from_left_feedback_ != 0.0f || second_from_left_feedback_ != 0.0f);
		feedback_voice_[1] = used_[3] && (first_from_right_feedback_ != 0.0f || second_from_right_feedback_ != 0.0f);
		chunk_limit_ = max_block;
		if (feedback_voice_[0])
			chunk_limit_ = std::min(chunk_limit_, base_[0]);
		if (feedback_voice_[1])
			chunk_limit_ = std::min(chunk_limit_, base_[3]);
		chunk_limit_ = std::max(chunk_limit_, 1);
	}

	void reset()
	{
		line1_.clear();
		line2_.clear();
		position_ = 0;
		eq_[0].reset();
		eq_[1].reset();
		voice_previous_[0] = voice_previous_[1] = 0.0f;
		for (int i = 0; i < 6; i++)
			lfo_previous_[i] = 0.0f;
		for (int k = 0; k < 3; k++)
			shaped_previous_[k] = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		(void)table;
		int done = 0;
		while (done < n) {
			const int m = std::min(n - done, chunk_limit_);
			const float *l[6];
			for (int i = 0; i < 6; i++)
				l[i] = lfo[i] + done;
			run(in_left + done, in_right + done, l, out_left + done, out_right + done, m);
			done += m;
		}
	}

private:
	using shelf_chain = fx_flow_delay_detail::shelf_chain;

	template <uint32_t length>
	void read_from(const fx_flow::delay_line<length> &line, int v, const float *fast, const float *slow, float *dest,
		int m) const
	{
		if (fast_depth_[v] == 0.0f && slow_depth_[v] == 0.0f) {
			line.read(position_, base_[v], dest, m);
			return;
		}
		float swing[fx_flow::max_block];
		const float fd = fast_depth_[v], sd = slow_depth_[v];
		for (int s = 0; s < m; s++)
			swing[s] = fd * fast[s] + sd * slow[s];
		fx_flow_delay_detail::read_swung(line, position_, base_[v], swing, dest, m);
	}

	void run(const float *in_left, const float *in_right, const float *const *lfo, float *out_left, float *out_right,
		int m)
	{
		using namespace fx_flow_delay_detail;
		float input_l[max_block], input_r[max_block], L[max_block], R[max_block];
		std::copy_n(in_left, m, input_l);
		std::copy_n(in_right, m, input_r);
		eq_[0].block(input_gain_[0], input_l, L, m);
		eq_[1].block(input_gain_[1], input_r, R, m);

		// 1 サンプル前の LFO。声 0 = LFO 3 / LFO 0、声 1・2 = LFO 5 / LFO 2、声 3 = LFO 4 / LFO 1（添え字 k = 0, 2, 1）
		float fast[3][max_block], slow[3][max_block];
		for (int k = 0; k < 3; k++) {
			const float *raw_fast = lfo[3 + k];
			fast[k][0] = lfo_previous_[3 + k];
			std::copy_n(raw_fast, m - 1, fast[k] + 1);
			lfo_previous_[3 + k] = raw_fast[m - 1];

			float shaped[max_block];
			shaper_[k].block(lfo[k], shaped, m);
			slow[k][0] = shaped_previous_[k];
			std::copy_n(shaped, m - 1, slow[k] + 1);
			shaped_previous_[k] = shaped[m - 1];
		}
		static constexpr int lfo_set[4] = {0, 2, 2, 1};

		float voice[4][max_block];
		auto read_voice = [&](int v) {
			if (!used_[v]) {
				std::fill_n(voice[v], m, 0.0f);
				return;
			}
			if (second_[v])
				read_from(line2_, v, fast[lfo_set[v]], slow[lfo_set[v]], voice[v], m);
			else
				read_from(line1_, v, fast[lfo_set[v]], slow[lfo_set[v]], voice[v], m);
		};

		// 帰還のある声（書く前に読む）と帰還フィルタ
		float feedback_left[max_block], feedback_right[max_block];
		if (feedback_voice_[0]) {
			read_voice(0);
			const float fc = feedback_current_[0], fp = feedback_previous_[0];
			float previous = voice_previous_[0];
			for (int s = 0; s < m; s++) {
				feedback_left[s] = sat(fc * voice[0][s] + fp * previous);
				previous = voice[0][s];
			}
		} else
			std::fill_n(feedback_left, m, 0.0f);
		if (feedback_voice_[1]) {
			read_voice(3);
			const float fc = feedback_current_[1], fp = feedback_previous_[1];
			float previous = voice_previous_[1];
			for (int s = 0; s < m; s++) {
				feedback_right[s] = sat(fc * voice[3][s] + fp * previous);
				previous = voice[3][s];
			}
		} else
			std::fill_n(feedback_right, m, 0.0f);

		// 線へ書く
		{
			float written1[max_block], written2[max_block];
			const float ffl = first_from_left_, ffr = first_from_right_, f1l = first_from_left_feedback_,
				f1r = first_from_right_feedback_, s2r = second_from_right_, s2l = second_from_left_feedback_,
				s2rf = second_from_right_feedback_;
			for (int s = 0; s < m; s++) {
				written1[s] = word(sat(ffl * L[s] + ffr * R[s] + f1l * feedback_left[s] + f1r * feedback_right[s]));
				written2[s] = word(sat(s2r * R[s] + s2l * feedback_left[s] + s2rf * feedback_right[s]));
			}
			line1_.write(position_, written1, m);
			line2_.write(position_, written2, m);
		}

		// 書いたあとに読む声
		if (!feedback_voice_[0])
			read_voice(0);
		read_voice(1);
		read_voice(2);
		if (!feedback_voice_[1])
			read_voice(3);
		voice_previous_[0] = voice[0][m - 1];
		voice_previous_[1] = voice[3][m - 1];

		// 出口
		{
			const float wl0 = wet_left_[0], wl1 = wet_left_[1], wl2 = wet_left_[2];
			const float wr3 = wet_right_[3], wr2 = wet_right_[2], wr1 = wet_right_[1];
			const float wg0 = wet_gain_[0], wg1 = wet_gain_[1], dg0 = dry_gain_[0], dg1 = dry_gain_[1];
			float out_l[max_block], out_r[max_block];
			for (int s = 0; s < m; s++) {
				const float wet_l = sat(wl0 * voice[0][s] + wl1 * voice[1][s] + wl2 * voice[2][s]);
				const float wet_r = sat(wr3 * voice[3][s] + wr2 * voice[2][s] + wr1 * voice[1][s]);
				out_l[s] = sat(wg0 * wet_l + dg0 * L[s]);
				out_r[s] = sat(wg1 * wet_r + dg1 * R[s]);
			}
			std::copy_n(out_l, m, out_left);
			std::copy_n(out_r, m, out_right);
		}
		position_ += uint32_t(m);
	}

	fx_flow::delay_line<131072> line1_;
	fx_flow::delay_line<65536> line2_;
	uint32_t position_ = 0;

	fx_flow_delay_detail::shelf_chain eq_[2];
	fx_flow_delay_detail::shaper_coef shaper_[3];
	float input_gain_[2] = {};
	float feedback_current_[2] = {}, feedback_previous_[2] = {};
	float dry_gain_[2] = {}, wet_gain_[2] = {};
	int base_[4] = {};
	bool second_[4] = {};
	bool used_[4] = {};
	bool feedback_voice_[2] = {};
	float fast_depth_[4] = {}, slow_depth_[4] = {};
	float first_from_left_ = 0, first_from_right_ = 0, first_from_left_feedback_ = 0, first_from_right_feedback_ = 0;
	float second_from_right_ = 0, second_from_left_feedback_ = 0, second_from_right_feedback_ = 0;
	float wet_left_[3] = {}, wet_right_[4] = {};
	int chunk_limit_ = fx_flow::max_block;

	float voice_previous_[2] = {};
	float lfo_previous_[6] = {};
	float shaped_previous_[3] = {};
};

// =====================================================================================================
// chorus3.md（バリエーション・インサーションとシステムコーラス）
// =====================================================================================================

namespace fx_flow_delay_detail {

struct chorus3_config {
	float input_gain[2] = {};
	shelf_chain eq[2];
	int base[4] = {};
	bool second[4] = {};
	float depth[4] = {};
	float first_from_left = 0, first_from_right = 0, first_feedback[2] = {};
	float second_from_right = 0, second_feedback[2] = {};
	float wet_mix[2][2] = {};
	float dry_gain[2] = {};
	float wet_gain[2] = {};
};

template <bool system>
class chorus3_core {
public:
	void set(const chorus3_config &c)
	{
		// 状態を残して係数だけ入れ替える
		for (int ch = 0; ch < 2; ch++) {
			input_gain_[ch] = c.input_gain[ch];
			eq_[ch].low = c.eq[ch].low;
			eq_[ch].mid = c.eq[ch].mid;
			eq_[ch].high = c.eq[ch].high;
			eq_[ch].use_mid = c.eq[ch].use_mid;
			dry_gain_[ch] = c.dry_gain[ch];
			wet_gain_[ch] = c.wet_gain[ch];
			for (int k = 0; k < 2; k++)
				wet_mix_[ch][k] = c.wet_mix[ch][k];
		}
		for (int v = 0; v < 4; v++) {
			base_[v] = c.base[v];
			second_[v] = c.second[v];
			depth_[v] = c.depth[v];
		}
		first_from_left_ = c.first_from_left;
		first_from_right_ = c.first_from_right;
		second_from_right_ = c.second_from_right;
		feedback_gain_[0] = c.first_feedback[0];
		feedback_gain_[1] = c.first_feedback[1];
		feedback_gain_[2] = c.second_feedback[0];
		feedback_gain_[3] = c.second_feedback[1];

		chunk_limit_ = max_block;
		for (int v = 0; v < 4; v++) {
			before_[v] = feedback_gain_[v] != 0.0f;
			if (before_[v])
				chunk_limit_ = std::min(chunk_limit_, base_[v]);
		}
		chunk_limit_ = std::max(chunk_limit_, 1);
	}

	void reset()
	{
		line1_.clear();
		line2_.clear();
		position_ = 0;
		eq_[0].reset();
		eq_[1].reset();
		for (int i = 0; i < 6; i++)
			lfo_previous_[i] = 0.0f;
		voice3_carry_ = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], float *out_left,
		float *out_right, int n)
	{
		int done = 0;
		while (done < n) {
			const int m = std::min(n - done, chunk_limit_);
			const float *l[6];
			for (int i = 0; i < 6; i++)
				l[i] = lfo[i] + done;
			run(in_left + done, in_right + done, l, out_left + done, out_right + done, m);
			done += m;
		}
	}

private:
	template <uint32_t length>
	void read_from(const fx_flow::delay_line<length> &line, int v, const float *phase, float *dest, int m) const
	{
		if (depth_[v] == 0.0f) {
			line.read(position_, base_[v], dest, m);
			return;
		}
		float swing[max_block];
		const float d = depth_[v];
		for (int s = 0; s < m; s++)
			swing[s] = d * phase[s];
		read_swung(line, position_, base_[v], swing, dest, m);
	}

	void run(const float *in_left, const float *in_right, const float *const *lfo, float *out_left, float *out_right,
		int m)
	{
		float input_l[max_block], input_r[max_block], L[max_block], R[max_block];
		std::copy_n(in_left, m, input_l);
		std::copy_n(in_right, m, input_r);
		eq_[0].block(input_gain_[0], input_l, L, m);
		eq_[1].block(input_gain_[1], input_r, R, m);

		// 声が使う LFO。バリエーション・インサーションは 1 サンプル前の値、システムコーラスは今の値。
		static constexpr int lfo_index[4] = {system ? 0 : 3, system ? 1 : 5, 2, system ? 3 : 4};
		float phase[4][max_block];
		for (int v = 0; v < 4; v++) {
			const float *raw = lfo[lfo_index[v]];
			if constexpr (system)
				std::copy_n(raw, m, phase[v]);
			else {
				phase[v][0] = lfo_previous_[lfo_index[v]];
				std::copy_n(raw, m - 1, phase[v] + 1);
			}
		}
		for (int i = 0; i < 6; i++)
			lfo_previous_[i] = lfo[i][m - 1];

		float voice[4][max_block];
		auto read_voice = [&](int v) {
			if (second_[v])
				read_from(line2_, v, phase[v], voice[v], m);
			else
				read_from(line1_, v, phase[v], voice[v], m);
			if (system && v == 3) {
				// 補間した値が出るのは次のサンプル
				const float last = voice[3][m - 1];
				for (int s = m - 1; s > 0; s--)
					voice[3][s] = voice[3][s - 1];
				voice[3][0] = voice3_carry_;
				voice3_carry_ = last;
			}
		};

		for (int v = 0; v < 4; v++) {
			if (before_[v])
				read_voice(v);
			else
				std::fill_n(voice[v], m, 0.0f);
		}

		{
			float written1[max_block], written2[max_block];
			const float ffl = first_from_left_, ffr = first_from_right_, s2r = second_from_right_;
			const float f0 = feedback_gain_[0], f1 = feedback_gain_[1], f2 = feedback_gain_[2], f3 = feedback_gain_[3];
			for (int s = 0; s < m; s++) {
				written1[s] = word(sat(ffl * L[s] + ffr * R[s] + f0 * voice[0][s] + f1 * voice[1][s]));
				written2[s] = word(sat(s2r * R[s] + f2 * voice[2][s] + f3 * voice[3][s]));
			}
			line1_.write(position_, written1, m);
			line2_.write(position_, written2, m);
		}

		for (int v = 0; v < 4; v++)
			if (!before_[v])
				read_voice(v);

		{
			const float w00 = wet_mix_[0][0], w01 = wet_mix_[0][1], w10 = wet_mix_[1][0], w11 = wet_mix_[1][1];
			const float dg0 = dry_gain_[0], dg1 = dry_gain_[1], wg0 = wet_gain_[0], wg1 = wet_gain_[1];
			float out_l[max_block], out_r[max_block];
			for (int s = 0; s < m; s++) {
				const float wet_l = w00 * voice[0][s] + w01 * voice[1][s];
				const float wet_r = w10 * voice[2][s] + w11 * voice[3][s];
				out_l[s] = sat(dg0 * L[s] + wg0 * wet_l);
				out_r[s] = sat(dg1 * R[s] + wg1 * wet_r);
			}
			std::copy_n(out_l, m, out_left);
			std::copy_n(out_r, m, out_right);
		}
		position_ += uint32_t(m);
	}

	fx_flow::delay_line<4096> line1_, line2_;
	uint32_t position_ = 0;
	shelf_chain eq_[2];
	float input_gain_[2] = {};
	int base_[4] = {};
	bool second_[4] = {};
	float depth_[4] = {};
	float first_from_left_ = 0, first_from_right_ = 0, second_from_right_ = 0;
	float feedback_gain_[4] = {};
	bool before_[4] = {};
	float wet_mix_[2][2] = {};
	float dry_gain_[2] = {}, wet_gain_[2] = {};
	int chunk_limit_ = max_block;
	float lfo_previous_[6] = {};
	float voice3_carry_ = 0.0f;
};

} // namespace fx_flow_delay_detail

class chorus3_flow {
public:
	chorus3_flow() { configure(chorus3_params{}); }

	void configure(const chorus3_params &p)
	{
		fx_flow_delay_detail::chorus3_config c;
		for (int ch = 0; ch < 2; ch++) {
			c.input_gain[ch] = p.input_gain[ch];
			c.eq[ch].low = {p.low_shelf_current[ch], p.low_shelf_previous[ch], p.low_shelf_feedback[ch]};
			c.eq[ch].mid = {p.mid_current[ch], p.mid_x1[ch], p.mid_x2[ch], p.mid_y1[ch], p.mid_y2[ch]};
			c.eq[ch].high = {p.high_shelf_current[ch], p.high_shelf_previous[ch], p.high_shelf_feedback[ch]};
			c.eq[ch].use_mid = true;
			c.dry_gain[ch] = p.dry_gain[ch];
			c.wet_gain[ch] = p.wet_gain[ch];
			for (int k = 0; k < 2; k++) {
				c.wet_mix[ch][k] = p.wet_mix[ch][k];
				c.first_feedback[k] = p.first_line_feedback[k];
				c.second_feedback[k] = p.second_line_feedback[k];
			}
		}
		for (int v = 0; v < 4; v++) {
			c.base[v] = p.voice_base_delay[v];
			// 声 0・1 は 1 本目の線に固定（文書の文）
			c.second[v] = v >= 2 && p.voice_on_second_line[v] != 0;
			c.depth[v] = p.voice_depth[v];
		}
		c.first_from_left = p.first_line_from_left;
		c.first_from_right = p.first_line_from_right;
		c.second_from_right = p.second_line_from_right;
		core_.set(c);
	}

	void reset() { core_.reset(); }

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		(void)table;
		core_.process(in_left, in_right, lfo, out_left, out_right, n);
	}

private:
	fx_flow_delay_detail::chorus3_core<false> core_;
};

class system_chorus3_flow {
public:
	system_chorus3_flow() { configure(system_chorus3_params{}); }

	void configure(const system_chorus3_params &p)
	{
		fx_flow_delay_detail::chorus3_config c;
		for (int ch = 0; ch < 2; ch++) {
			c.input_gain[ch] = p.input_gain[ch];
			c.eq[ch].low = {p.low_shelf_current[ch], p.low_shelf_previous[ch], p.low_shelf_feedback[ch]};
			c.eq[ch].high = {p.high_shelf_current[ch], p.high_shelf_previous[ch], p.high_shelf_feedback[ch]};
			c.eq[ch].use_mid = false;
			c.dry_gain[ch] = 0.0f; // 乾いた音は無い
			c.wet_gain[ch] = p.wet_gain[ch];
			for (int k = 0; k < 2; k++) {
				c.wet_mix[ch][k] = p.wet_mix[ch][k];
				c.first_feedback[k] = p.first_line_feedback[k];
				c.second_feedback[k] = p.second_line_feedback[k];
			}
		}
		for (int v = 0; v < 4; v++) {
			c.base[v] = p.voice_base_delay[v];
			c.second[v] = v >= 2 && p.voice_on_second_line[v] != 0;
			c.depth[v] = p.voice_depth[v];
		}
		c.first_from_left = p.first_line_from_left;
		c.first_from_right = p.first_line_from_right;
		c.second_from_right = p.second_line_from_right;
		core_.set(c);
	}

	void reset() { core_.reset(); }

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		(void)table;
		core_.process(in_left, in_right, lfo, out_left, out_right, n);
	}

private:
	fx_flow_delay_detail::chorus3_core<true> core_;
};

// =====================================================================================================
// early_reflection.md
// =====================================================================================================

class early_reflection_flow {
public:
	early_reflection_flow() { configure(early_reflection_params{}); }

	void configure(const early_reflection_params &p)
	{
		using namespace fx_flow_delay_detail;
		input_from_left_ = p.input_from_left;
		input_from_right_ = p.input_from_right;
		highpass_ = {p.highpass_current, p.highpass_previous, p.highpass_feedback};
		lowpass_input_gain_ = p.lowpass_input_gain;
		lowpass_ = {p.lowpass_current, p.lowpass_previous, p.lowpass_feedback};
		for (int i = 0; i < 2; i++) {
			diffuser_delay_[i] = p.diffuser_delay[i];
			diffuser_feedback_[i] = p.diffuser_feedback[i];
			diffuser_input_[i] = p.diffuser_input[i];
			diffuser_delayed_out_[i] = p.diffuser_delayed_out[i];
			diffuser_direct_out_[i] = p.diffuser_direct_out[i];
			cross_delay_[i] = p.cross_delay[i];
			cross_gain_[i] = p.cross_gain[i];
			sum_gain_[i] = p.sum_gain[i];
			out_delay_[i] = p.out_allpass_delay[i];
			out_feedback_[i] = p.out_allpass_feedback[i];
			out_input_[i] = p.out_allpass_input[i];
			out_delayed_out_[i] = p.out_allpass_delayed_out[i];
			out_direct_out_[i] = p.out_allpass_direct_out[i];
			dry_gain_[i] = p.dry_gain[i];
			wet_gain_[i] = p.wet_gain[i];
			for (int t = 0; t < 9; t++) {
				tap_delay_[i][t] = p.tap_delay[i][t];
				tap_gain_[i][t] = p.tap_gain[i][t];
			}
		}
		feedback_delay_ = p.feedback_delay;
		feedback_filter_ = {p.feedback_current, p.feedback_previous, p.feedback_feedback};
		line_from_diffuser_ = p.line_from_diffuser;
		line_from_feedback_ = p.line_from_feedback;

		feedback_on_ = line_from_feedback_ != 0.0f;
		chunk_limit_ = max_block;
		if (feedback_on_)
			chunk_limit_ = std::min(chunk_limit_, feedback_delay_);
		for (int i = 0; i < 2; i++)
			chunk_limit_ = std::min({chunk_limit_, diffuser_delay_[i], out_delay_[i]});
		chunk_limit_ = std::max(chunk_limit_, 1);
	}

	void reset()
	{
		main_.clear();
		for (auto &l : diffuser_line_)
			l.clear();
		for (auto &l : out_line_)
			l.clear();
		for (auto &l : short_line_)
			l.clear();
		position_ = 0;
		highpass_state_ = lowpass_state_ = feedback_state_ = {};
		wet_previous_[0] = wet_previous_[1] = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		(void)lfo;
		(void)table;
		int done = 0;
		while (done < n) {
			const int m = std::min(n - done, chunk_limit_);
			run(in_left + done, in_right + done, out_left + done, out_right + done, m);
			done += m;
		}
	}

private:
	static constexpr int max_block = fx_flow::max_block;

	void run(const float *in_left, const float *in_right, float *out_left, float *out_right, int m)
	{
		using namespace fx_flow_delay_detail;
		float input[2][max_block], signal[max_block], r[max_block], feedback[max_block], written[max_block];
		std::copy_n(in_left, m, input[0]);
		std::copy_n(in_right, m, input[1]);

		// 入口: モノラル → ハイパス → × lowpass_input_gain → [sat] → ローパス
		{
			const float ifl = input_from_left_, ifr = input_from_right_, lg = lowpass_input_gain_;
			const onepole_coef hp = highpass_, lp = lowpass_;
			onepole_state hs = highpass_state_, ls = lowpass_state_;
			for (int s = 0; s < m; s++) {
				float x = sat(ifl * input[0][s] + ifr * input[1][s]);
				x = onepole_step(hp, hs, x);
				x = sat(lg * x);
				signal[s] = onepole_step(lp, ls, x);
			}
			highpass_state_ = hs;
			lowpass_state_ = ls;
		}
		for (int i = 0; i < 2; i++)
			fx_flow::allpass_run(diffuser_line_[i], position_, diffuser_delay_[i], diffuser_input_[i],
				diffuser_feedback_[i], diffuser_delayed_out_[i], diffuser_direct_out_[i], signal, m);

		auto feedback_filter = [&]() {
			const onepole_coef c = feedback_filter_;
			onepole_state st = feedback_state_;
			for (int s = 0; s < m; s++)
				feedback[s] = onepole_step(c, st, r[s]);
			feedback_state_ = st;
		};

		// 主の遅延線: 帰還があれば書く前に帰還を読む
		if (feedback_on_) {
			main_.read(position_, feedback_delay_, r, m);
			feedback_filter();
		} else
			std::fill_n(feedback, m, 0.0f);
		{
			const float lfd = line_from_diffuser_, lff = line_from_feedback_;
			for (int s = 0; s < m; s++)
				written[s] = word(sat(lfd * signal[s] + lff * feedback[s]));
			main_.write(position_, written, m);
		}
		if (!feedback_on_) {
			// 出力には使わないが、帰還フィルタの状態は進めておく
			main_.read(position_, feedback_delay_, r, m);
			feedback_filter();
		}

		// タップ
		float reflection[2][max_block];
		for (int ch = 0; ch < 2; ch++) {
			float sum[max_block];
			std::fill_n(sum, m, 0.0f);
			for (int t = 0; t < 9; t++)
				main_.add_tap(position_, tap_delay_[ch][t], tap_gain_[ch][t], sum, m);
			for (int s = 0; s < m; s++)
				reflection[ch][s] = sat(sum[s]);
			for (int s = 0; s < m; s++)
				written[s] = word(reflection[ch][s]);
			short_line_[ch].write(position_, written, m);
		}

		// 反対側を混ぜる
		float mixed[2][max_block];
		for (int ch = 0; ch < 2; ch++) {
			const float sg = sum_gain_[ch];
			if (cross_delay_[ch] > 0 && cross_gain_[ch] != 0.0f) {
				float cross[max_block];
				short_line_[1 - ch].read(position_, cross_delay_[ch], cross, m);
				const float cg = cross_gain_[ch];
				for (int s = 0; s < m; s++)
					mixed[ch][s] = sat(sg * reflection[ch][s] + cg * cross[s]);
			} else
				for (int s = 0; s < m; s++)
					mixed[ch][s] = sat(sg * reflection[ch][s]);
		}

		// 出口オールパス → z⁻¹ → 出口
		for (int ch = 0; ch < 2; ch++) {
			fx_flow::allpass_run(out_line_[ch], position_, out_delay_[ch], out_input_[ch], out_feedback_[ch],
				out_delayed_out_[ch], out_direct_out_[ch], mixed[ch], m);
			const float dg = dry_gain_[ch], wg = wet_gain_[ch];
			float *out = ch == 0 ? out_left : out_right;
			float result[max_block];
			result[0] = sat(dg * input[ch][0] + wg * wet_previous_[ch]);
			for (int s = 1; s < m; s++)
				result[s] = sat(dg * input[ch][s] + wg * mixed[ch][s - 1]);
			wet_previous_[ch] = mixed[ch][m - 1];
			std::copy_n(result, m, out);
		}
		position_ += uint32_t(m);
	}

	fx_flow::delay_line<65536> main_;
	fx_flow::delay_line<1024> diffuser_line_[2];
	fx_flow::delay_line<1024> out_line_[2];
	fx_flow::delay_line<256> short_line_[2];
	uint32_t position_ = 0;

	float input_from_left_ = 0, input_from_right_ = 0, lowpass_input_gain_ = 0;
	fx_flow_delay_detail::onepole_coef highpass_, lowpass_, feedback_filter_;
	fx_flow_delay_detail::onepole_state highpass_state_, lowpass_state_, feedback_state_;
	int diffuser_delay_[2] = {};
	float diffuser_feedback_[2] = {}, diffuser_input_[2] = {}, diffuser_delayed_out_[2] = {}, diffuser_direct_out_[2] = {};
	int feedback_delay_ = 0;
	float line_from_diffuser_ = 0, line_from_feedback_ = 0;
	int tap_delay_[2][9] = {};
	float tap_gain_[2][9] = {};
	int cross_delay_[2] = {};
	float cross_gain_[2] = {}, sum_gain_[2] = {};
	int out_delay_[2] = {};
	float out_feedback_[2] = {}, out_input_[2] = {}, out_delayed_out_[2] = {}, out_direct_out_[2] = {};
	float dry_gain_[2] = {}, wet_gain_[2] = {};
	float wet_previous_[2] = {};
	bool feedback_on_ = false;
	int chunk_limit_ = max_block;
};

// =====================================================================================================
// reverb.md の「バリエーション・インサーションのリバーブ」
// =====================================================================================================

class variation_reverb_flow {
public:
	variation_reverb_flow() { configure(variation_reverb_params{}); }

	void configure(const variation_reverb_params &p)
	{
		using namespace fx_flow_delay_detail;
		input_from_left_ = p.input_from_left;
		input_from_right_ = p.input_from_right;
		highpass_ = {p.highpass_current, p.highpass_previous, p.highpass_feedback};
		lowpass_ = {p.lowpass_current, p.lowpass_previous, p.lowpass_feedback};
		predelay_ = p.predelay;
		predelay_feedback_ = p.predelay_feedback;
		diffuse_input_gain_ = p.diffuse_input_gain;
		diffuse_filter_ = {p.diffuse_filter_current, p.diffuse_filter_previous, p.diffuse_filter_feedback};
		for (int i = 0; i < 2; i++) {
			diffuser_delay_[i] = p.diffuser_delay[i];
			diffuser_feedback_[i] = p.diffuser_feedback[i];
			diffuser_input_[i] = p.diffuser_input[i];
			diffuser_delayed_out_[i] = p.diffuser_delayed_out[i];
			diffuser_direct_out_[i] = p.diffuser_direct_out[i];
			dry_gain_[i] = p.dry_gain[i];
			wet_gain_[i] = p.wet_gain[i];
			for (int k = 0; k < 2; k++) {
				out_delay_ap_[i][k] = p.out_allpass_delay[i][k];
				out_feedback_[i][k] = p.out_allpass_feedback[i][k];
				out_input_[i][k] = p.out_allpass_input[i][k];
				out_delayed_out_[i][k] = p.out_allpass_delayed_out[i][k];
				out_direct_out_[i][k] = p.out_allpass_direct_out[i][k];
			}
			for (int t = 0; t < 5; t++) {
				tap_delay_[i][t] = p.out_delay[i][t];
				tap_gain_[i][t] = p.out_gain[i][t];
			}
		}
		for (int c = 0; c < 4; c++) {
			comb_delay_[c] = p.comb_delay[c];
			comb_damp_current_[c] = p.comb_damp_current[c];
			comb_damp_previous_[c] = p.comb_damp_previous[c];
			comb_damp_feedback_[c] = p.comb_damp_feedback[c];
			comb_input_[c] = p.comb_input[c];
		}

		feedback_on_ = predelay_feedback_ != 0.0f;
		chunk_limit_ = max_block;
		if (feedback_on_)
			chunk_limit_ = std::min(chunk_limit_, predelay_);
		for (int i = 0; i < 2; i++) {
			chunk_limit_ = std::min(chunk_limit_, diffuser_delay_[i]);
			for (int k = 0; k < 2; k++)
				chunk_limit_ = std::min(chunk_limit_, out_delay_ap_[i][k]);
		}
		for (int c = 0; c < 4; c++)
			chunk_limit_ = std::min(chunk_limit_, comb_delay_[c]);
		chunk_limit_ = std::max(chunk_limit_, 1);
	}

	void reset()
	{
		initial_.clear();
		for (auto &l : comb_)
			l.clear();
		for (auto &l : diffuser_line_)
			l.clear();
		for (auto &row : out_line_)
			for (auto &l : row)
				l.clear();
		position_ = 0;
		highpass_state_ = lowpass_state_ = diffuse_state_ = {};
		for (int c = 0; c < 4; c++)
			comb_read_previous_[c] = comb_damped_[c] = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		(void)lfo;
		(void)table;
		int done = 0;
		while (done < n) {
			const int m = std::min(n - done, chunk_limit_);
			run(in_left + done, in_right + done, out_left + done, out_right + done, m);
			done += m;
		}
	}

private:
	static constexpr int max_block = fx_flow::max_block;

	void run(const float *in_left, const float *in_right, float *out_left, float *out_right, int m)
	{
		using namespace fx_flow_delay_detail;
		float input[2][max_block], signal[max_block], p[max_block], written[max_block];
		std::copy_n(in_left, m, input[0]);
		std::copy_n(in_right, m, input[1]);

		{
			const float ifl = input_from_left_, ifr = input_from_right_;
			const onepole_coef hp = highpass_, lp = lowpass_;
			onepole_state hs = highpass_state_, ls = lowpass_state_;
			for (int s = 0; s < m; s++) {
				float x = sat(ifl * input[0][s] + ifr * input[1][s]);
				x = onepole_step(hp, hs, x);
				signal[s] = onepole_step(lp, ls, x);
			}
			highpass_state_ = hs;
			lowpass_state_ = ls;
		}

		// 初期遅延線: 帰還があれば書く前に p を読む
		if (feedback_on_) {
			initial_.read(position_, predelay_, p, m);
			const float fb = predelay_feedback_;
			for (int s = 0; s < m; s++)
				written[s] = word(sat(signal[s] + fb * p[s]));
		} else
			for (int s = 0; s < m; s++)
				written[s] = word(sat(signal[s]));
		initial_.write(position_, written, m);
		if (!feedback_on_)
			initial_.read(position_, predelay_, p, m);

		// 拡散
		float diffused[max_block];
		{
			const float dg = diffuse_input_gain_;
			const onepole_coef dc = diffuse_filter_;
			onepole_state ds = diffuse_state_;
			for (int s = 0; s < m; s++)
				diffused[s] = onepole_step(dc, ds, sat(dg * p[s]));
			diffuse_state_ = ds;
		}
		for (int i = 0; i < 2; i++)
			fx_flow::allpass_run(diffuser_line_[i], position_, diffuser_delay_[i], diffuser_input_[i],
				diffuser_feedback_[i], diffuser_delayed_out_[i], diffuser_direct_out_[i], diffused, m);

		// 櫛形 4 本（読み出し・減衰・書き込み）
		{
			float r[4][max_block], w[4][max_block];
			for (int c = 0; c < 4; c++)
				comb_[c].read(position_, comb_delay_[c], r[c], m);
			float dcur[4], dprev[4], dfb[4], cin[4], rprev[4], damped[4];
			for (int c = 0; c < 4; c++) {
				dcur[c] = comb_damp_current_[c];
				dprev[c] = comb_damp_previous_[c];
				dfb[c] = comb_damp_feedback_[c];
				cin[c] = comb_input_[c];
				rprev[c] = comb_read_previous_[c];
				damped[c] = comb_damped_[c];
			}
			for (int s = 0; s < m; s++) {
				const float d = diffused[s];
				for (int c = 0; c < 4; c++) {
					const float rc = r[c][s];
					const float y = sat(dcur[c] * rc + dprev[c] * rprev[c] + dfb[c] * damped[c]);
					rprev[c] = rc;
					damped[c] = y;
					w[c][s] = word(sat(cin[c] * d + y));
				}
			}
			for (int c = 0; c < 4; c++) {
				comb_read_previous_[c] = rprev[c];
				comb_damped_[c] = damped[c];
				comb_[c].write(position_, w[c], m);
			}
		}

		// 出口のタップ → 出口オールパス 2 段 → 乾いた音と混ぜる
		for (int ch = 0; ch < 2; ch++) {
			float sum[max_block];
			std::fill_n(sum, m, 0.0f);
			initial_.add_tap(position_, tap_delay_[ch][0], tap_gain_[ch][0], sum, m);
			comb_[0].add_tap(position_, tap_delay_[ch][1], tap_gain_[ch][1], sum, m);
			comb_[1].add_tap(position_, tap_delay_[ch][2], tap_gain_[ch][2], sum, m);
			comb_[2].add_tap(position_, tap_delay_[ch][3], tap_gain_[ch][3], sum, m);
			comb_[3].add_tap(position_, tap_delay_[ch][4], tap_gain_[ch][4], sum, m);
			float wet[max_block];
			for (int s = 0; s < m; s++)
				wet[s] = sat(sum[s]);
			for (int k = 0; k < 2; k++)
				fx_flow::allpass_run(out_line_[ch][k], position_, out_delay_ap_[ch][k], out_input_[ch][k],
					out_feedback_[ch][k], out_delayed_out_[ch][k], out_direct_out_[ch][k], wet, m);
			const float dg = dry_gain_[ch], wg = wet_gain_[ch];
			float result[max_block];
			for (int s = 0; s < m; s++)
				result[s] = sat(dg * input[ch][s] + wg * wet[s]);
			std::copy_n(result, m, ch == 0 ? out_left : out_right);
		}
		position_ += uint32_t(m);
	}

	fx_flow::delay_line<16384> initial_;
	fx_flow::delay_line<8192> comb_[4];
	fx_flow::delay_line<2048> diffuser_line_[2];
	fx_flow::delay_line<2048> out_line_[2][2];
	uint32_t position_ = 0;

	float input_from_left_ = 0, input_from_right_ = 0;
	fx_flow_delay_detail::onepole_coef highpass_, lowpass_, diffuse_filter_;
	fx_flow_delay_detail::onepole_state highpass_state_, lowpass_state_, diffuse_state_;
	int predelay_ = 0;
	float predelay_feedback_ = 0, diffuse_input_gain_ = 0;
	int diffuser_delay_[2] = {};
	float diffuser_feedback_[2] = {}, diffuser_input_[2] = {}, diffuser_delayed_out_[2] = {}, diffuser_direct_out_[2] = {};
	int comb_delay_[4] = {};
	float comb_damp_current_[4] = {}, comb_damp_previous_[4] = {}, comb_damp_feedback_[4] = {}, comb_input_[4] = {};
	float comb_read_previous_[4] = {}, comb_damped_[4] = {};
	int tap_delay_[2][5] = {};
	float tap_gain_[2][5] = {};
	int out_delay_ap_[2][2] = {};
	float out_feedback_[2][2] = {}, out_input_[2][2] = {}, out_delayed_out_[2][2] = {}, out_direct_out_[2][2] = {};
	float dry_gain_[2] = {}, wet_gain_[2] = {};
	bool feedback_on_ = false;
	int chunk_limit_ = max_block;
};

// =====================================================================================================
// pitch_shift.md（バリエーション・インサーション）
// =====================================================================================================

class pitch_shift_flow {
public:
	pitch_shift_flow() { configure(pitch_shift_params{}); }

	void configure(const pitch_shift_params &p)
	{
		using namespace fx_flow_delay_detail;
		for (int ch = 0; ch < 2; ch++) {
			input_from_left_[ch] = p.input_from_left[ch];
			input_from_right_[ch] = p.input_from_right[ch];
			for (int st = 0; st < 3; st++)
				eq_[ch].c[st] = {p.band_current[ch][st], p.band_x1[ch][st], p.band_x2[ch][st], p.band_y1[ch][st],
					p.band_y2[ch][st]};
			head_[ch].base_a = p.head_a_base_delay[ch];
			head_[ch].base_b = p.head_b_base_delay[ch];
			head_[ch].depth_a = p.head_a_depth[ch];
			head_[ch].depth_b = p.head_b_depth[ch];
			head_[ch].phase_center = p.head_b_phase_center[ch];
			head_[ch].phase_gain = p.head_b_phase_gain[ch];
			head_[ch].window_a_offset = p.window_a_offset[ch];
			head_[ch].window_b_offset = p.window_b_offset[ch];
			head_[ch].window_a_gain = p.window_a_gain[ch];
			head_[ch].window_b_gain = p.window_b_gain[ch];
			wet_left_[ch] = p.wet_left[ch];
			wet_right_[ch] = p.wet_right[ch];
		}
		first_from_left_ = p.first_line_from_left;
		first_from_right_ = p.first_line_from_right;
		first_feedback_ = p.first_line_feedback;
		second_from_right_ = p.second_line_from_right;
		second_feedback_ = p.second_line_feedback;
		out_left_from_input_ = p.out_left_from_input;
		out_left_from_left_ = p.out_left_from_left;
		out_left_from_right_ = p.out_left_from_right;
		out_left_from_wet_ = p.out_left_from_wet;
		out_right_from_input_ = p.out_right_from_input;
		out_right_from_right_ = p.out_right_from_right;
		out_right_from_left_ = p.out_right_from_left;
		out_right_from_wet_ = p.out_right_from_wet;

		// ピッチ（左 / 右）を計算するか。濡れた音が出口に出ず、帰還も無ければ計算しない。
		feedback_[0] = first_feedback_ != 0.0f;
		feedback_[1] = second_feedback_ != 0.0f;
		const bool wet_to_left = out_left_from_wet_ != 0.0f, wet_to_right = out_right_from_wet_ != 0.0f;
		need_[0] = feedback_[0] || (wet_to_left && wet_left_[0] != 0.0f) || (wet_to_right && wet_right_[0] != 0.0f);
		need_[1] = feedback_[1] || (wet_to_left && wet_left_[1] != 0.0f) || (wet_to_right && wet_right_[1] != 0.0f);

		chunk_limit_ = max_block;
		for (int ch = 0; ch < 2; ch++)
			if (feedback_[ch])
				chunk_limit_ = std::min(chunk_limit_, std::min(head_[ch].base_a, head_[ch].base_b));
		chunk_limit_ = std::max(chunk_limit_, 1);
	}

	void reset()
	{
		line_[0].clear();
		line_[1].clear();
		position_ = 0;
		eq_[0].reset();
		eq_[1].reset();
		lfo_previous_[0] = lfo_previous_[1] = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		(void)table;
		int done = 0;
		while (done < n) {
			const int m = std::min(n - done, chunk_limit_);
			run(in_left + done, in_right + done, lfo[4] + done, lfo[5] + done, out_left + done, out_right + done, m);
			done += m;
		}
	}

private:
	static constexpr int max_block = fx_flow::max_block;

	void run(const float *in_left, const float *in_right, const float *lfo_left, const float *lfo_right,
		float *out_left, float *out_right, int m)
	{
		using namespace fx_flow_delay_detail;
		float input_l[max_block], input_r[max_block], x[2][max_block], L[max_block], R[max_block];
		std::copy_n(in_left, m, input_l);
		std::copy_n(in_right, m, input_r);
		{
			const float a0 = input_from_left_[0], a1 = input_from_left_[1], b1 = input_from_right_[1];
			for (int s = 0; s < m; s++) {
				x[0][s] = sat(a0 * input_l[s]);
				x[1][s] = sat(a1 * input_l[s] + b1 * input_r[s]);
			}
		}
		eq_[0].block(x[0], L, m);
		eq_[1].block(x[1], R, m);

		// 1 サンプル前の LFO
		float phi[2][max_block];
		phi[0][0] = lfo_previous_[0];
		std::copy_n(lfo_left, m - 1, phi[0] + 1);
		phi[1][0] = lfo_previous_[1];
		std::copy_n(lfo_right, m - 1, phi[1] + 1);
		lfo_previous_[0] = lfo_left[m - 1];
		lfo_previous_[1] = lfo_right[m - 1];

		float pitch[2][max_block] = {};
		if (feedback_[0])
			pitch_block(line_[0], position_, head_[0], phi[0], pitch[0], m);
		if (feedback_[1])
			pitch_block(line_[1], position_, head_[1], phi[1], pitch[1], m);

		{
			float written1[max_block], written2[max_block];
			const float ffl = first_from_left_, ffr = first_from_right_, ffb = first_feedback_;
			const float s2r = second_from_right_, s2f = second_feedback_;
			for (int s = 0; s < m; s++) {
				written1[s] = word(sat(ffl * L[s] + ffr * R[s] + ffb * pitch[0][s]));
				written2[s] = word(sat(s2r * R[s] + s2f * pitch[1][s]));
			}
			line_[0].write(position_, written1, m);
			line_[1].write(position_, written2, m);
		}

		if (need_[0] && !feedback_[0])
			pitch_block(line_[0], position_, head_[0], phi[0], pitch[0], m);
		if (need_[1] && !feedback_[1])
			pitch_block(line_[1], position_, head_[1], phi[1], pitch[1], m);

		{
			const float wl0 = wet_left_[0], wl1 = wet_left_[1], wr0 = wet_right_[0], wr1 = wet_right_[1];
			const float oli = out_left_from_input_, oll = out_left_from_left_, olr = out_left_from_right_,
				olw = out_left_from_wet_;
			const float ori = out_right_from_input_, orr = out_right_from_right_, orl = out_right_from_left_,
				orw = out_right_from_wet_;
			float out_l[max_block], out_r[max_block];
			for (int s = 0; s < m; s++) {
				const float wet_l = sat(wl0 * pitch[0][s] + wl1 * pitch[1][s]);
				const float wet_r = sat(wr0 * pitch[0][s] + wr1 * pitch[1][s]);
				out_l[s] = sat(oli * input_l[s] + oll * L[s] + olr * R[s] + olw * wet_l);
				out_r[s] = sat(ori * input_r[s] + orr * R[s] + orl * L[s] + orw * wet_r);
			}
			std::copy_n(out_l, m, out_left);
			std::copy_n(out_r, m, out_right);
		}
		position_ += uint32_t(m);
	}

	fx_flow::delay_line<32768> line_[2];
	uint32_t position_ = 0;

	fx_flow_delay_detail::biquad_chain3 eq_[2];
	fx_flow_delay_detail::pitch_head_coef head_[2];
	float input_from_left_[2] = {}, input_from_right_[2] = {};
	float wet_left_[2] = {}, wet_right_[2] = {};
	float first_from_left_ = 0, first_from_right_ = 0, first_feedback_ = 0, second_from_right_ = 0, second_feedback_ = 0;
	float out_left_from_input_ = 0, out_left_from_left_ = 0, out_left_from_right_ = 0, out_left_from_wet_ = 0;
	float out_right_from_input_ = 0, out_right_from_right_ = 0, out_right_from_left_ = 0, out_right_from_wet_ = 0;
	bool feedback_[2] = {}, need_[2] = {};
	int chunk_limit_ = max_block;
	float lfo_previous_[2] = {};
};

// =====================================================================================================
// pitch_shift.md（システムコーラスの ENS DETUNE）
// =====================================================================================================

class system_detune_flow {
public:
	system_detune_flow() { configure(system_detune_params{}); }

	void configure(const system_detune_params &p)
	{
		for (int ch = 0; ch < 2; ch++) {
			input_gain_[ch] = p.input_gain[ch];
			out_gain_[ch] = p.out_gain[ch];
			head_[ch].base_a = p.head_a_base_delay[ch];
			head_[ch].base_b = p.head_b_base_delay[ch];
			head_[ch].depth_a = p.head_a_depth[ch];
			head_[ch].depth_b = p.head_b_depth[ch];
			head_[ch].phase_center = p.head_b_phase_center[ch];
			head_[ch].phase_gain = p.head_b_phase_gain[ch];
			head_[ch].window_a_offset = p.window_a_offset[ch];
			head_[ch].window_b_offset = p.window_b_offset[ch];
			head_[ch].window_a_gain = p.window_a_gain[ch];
			head_[ch].window_b_gain = p.window_b_gain[ch];
		}
	}

	void reset()
	{
		line_[0].clear();
		line_[1].clear();
		position_ = 0;
		pitch_previous_[0] = pitch_previous_[1] = 0.0f;
	}

	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n)
	{
		(void)table;
		int done = 0;
		while (done < n) {
			const int m = std::min(n - done, fx_flow::max_block);
			run(in_left + done, in_right + done, lfo[0] + done, lfo[1] + done, out_left + done, out_right + done, m);
			done += m;
		}
	}

private:
	static constexpr int max_block = fx_flow::max_block;

	void run(const float *in_left, const float *in_right, const float *lfo_left, const float *lfo_right,
		float *out_left, float *out_right, int m)
	{
		using namespace fx_flow_delay_detail;
		float input[2][max_block], written[max_block], pitch[max_block];
		std::copy_n(in_left, m, input[0]);
		std::copy_n(in_right, m, input[1]);
		const float *lfo[2] = {lfo_left, lfo_right};
		float *out[2] = {out_left, out_right};
		float result[2][max_block];
		for (int ch = 0; ch < 2; ch++) {
			const float g = input_gain_[ch];
			for (int s = 0; s < m; s++)
				written[s] = word(sat(g * input[ch][s]));
			line_[ch].write(position_, written, m);
			// 今のサンプルの LFO で読み、出口へ出るのは次のサンプル
			pitch_block(line_[ch], position_, head_[ch], lfo[ch], pitch, m);
			const float og = out_gain_[ch];
			result[ch][0] = sat(og * pitch_previous_[ch]);
			for (int s = 1; s < m; s++)
				result[ch][s] = sat(og * pitch[s - 1]);
			pitch_previous_[ch] = pitch[m - 1];
		}
		std::copy_n(result[0], m, out[0]);
		std::copy_n(result[1], m, out[1]);
		position_ += uint32_t(m);
	}

	fx_flow::delay_line<8192> line_[2];
	uint32_t position_ = 0;
	fx_flow_delay_detail::pitch_head_coef head_[2];
	float input_gain_[2] = {}, out_gain_[2] = {};
	float pitch_previous_[2] = {};
};

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_DELAY_H
