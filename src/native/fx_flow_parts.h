// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/ のフローから書いたエフェクトが共通で使う部品。

#ifndef S_MU2000_NATIVE_FX_FLOW_PARTS_H
#define S_MU2000_NATIVE_FX_FLOW_PARTS_H

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace smu2000::native_fx {

namespace fx_flow {

constexpr int max_block = 32;

// 飽和の上は 24 ビットの最大値。1.0 ちょうどにはならない（u - 1 が負になって、T(v) で 1 刻み下がる所がある）
constexpr float sat_max = 8388607.0f / 8388608.0f;
constexpr float word_steps = 8388608.0f, word_step = 1.0f / word_steps;

// 段の値は 24 ビットの整数で、端数は 0 の側へ切り捨てる。
// 浮動小数のまま端数を持つと、小さい音の余韻だけが違った（Room 2 に送り 8 で入れると、-78 dB で 1.2 dB、-95 dB で 5 dB 小さい）
inline float sat(float v)
{
	const float held = v < -1.0f ? -1.0f : (v > sat_max ? sat_max : v);
	return std::trunc(held * word_steps) * word_step;
}

// README の語(v)。負の x は ~大きさ なので、「残した桁を反転して戻し、落とした桁は 0」は x の下位を 0 にするのと同じ。
inline float word(float v)
{
	const int32_t x = int32_t(v * 8388608.0f);
	const int32_t magnitude = x ^ (x >> 31);
	// magnitude は 2^23 以下で float に正確に入るので、指数がそのまま桁数になる（vector 化できる形の clz）
	const int32_t exponent_field = std::bit_cast<int32_t>(float(magnitude)) >> 23;
	const int32_t dropped_bits = std::max(exponent_field - 138, 0);
	return float(x & (int32_t(-1) << dropped_bits)) / 8388608.0f;
}

// 長さは 2 の冪。どの線も同じ position（これから書くサンプルの通し番号）で指す。
template <uint32_t length>
class delay_line {
public:
	void clear() { std::fill(words.begin(), words.end(), 0.0f); }

	float at(uint32_t index) const { return words[index & mask]; }
	void put(uint32_t index, float value) { words[index & mask] = value; }

	void read(uint32_t position, int delay, float *out, int n) const
	{
		const uint32_t start = (position - uint32_t(delay)) & mask;
		const int before_wrap = std::min(n, int(length - start));
		std::copy_n(words.data() + start, before_wrap, out);
		std::copy_n(words.data(), n - before_wrap, out + before_wrap);
	}

	void write(uint32_t position, const float *values, int n)
	{
		const uint32_t start = position & mask;
		const int before_wrap = std::min(n, int(length - start));
		std::copy_n(values, before_wrap, words.data() + start);
		std::copy_n(values + before_wrap, n - before_wrap, words.data());
	}

	void add_tap(uint32_t position, int delay, float gain, float *sum, int n) const
	{
		if (gain == 0.0f)
			return;
		const uint32_t start = (position - uint32_t(delay)) & mask;
		const int before_wrap = std::min(n, int(length - start));
		const float *tap = words.data() + start;
		for (int s = 0; s < before_wrap; s++)
			sum[s] += gain * tap[s];
		const float *wrapped_tap = words.data();
		for (int s = before_wrap; s < n; s++)
			sum[s] += gain * wrapped_tap[s - before_wrap];
	}

private:
	static constexpr uint32_t mask = length - 1;
	std::vector<float> words = std::vector<float>(length);
};

// README のオールパス。遅れが m 以上のときだけ呼ぶ。
template <uint32_t length>
inline void allpass_run(delay_line<length> &line, uint32_t position, int delay, float input, float feedback,
	float delayed_out, float direct_out, float *signal, int m)
{
	float delayed[max_block];
	float written[max_block];
	line.read(position, delay, delayed, m);
	for (int s = 0; s < m; s++) {
		written[s] = word(sat(input * signal[s] + feedback * delayed[s]));
		signal[s] = sat(direct_out * signal[s] + delayed_out * delayed[s]);
	}
	line.write(position, written, m);
}

// README の max(0, v): 0 〜 1.0 に収める
inline float positive(float v)
{
	return v < 0.0f ? 0.0f : (v > sat_max ? sat_max : v);
}

// README の T(v): 掛け算の係数にする信号を 1/32768 の刻みに切り捨てる
inline float stepped(float v)
{
	return std::floor(v * 32768.0f) * (1.0f / 32768.0f);
}

// 値を -1 以上 1 未満に折り返す（位相の足し算）
inline float wrap(float v)
{
	return v - 2.0f * std::floor((v + 1.0f) * 0.5f);
}

// README の「ゆっくり動く状態」。24 ビットへ、0 の側へ切り捨てる。
// 1 LSB に満たない動きは消えるので、音量の追従はある所で止まる（float のままだと止まらない）
inline float settled_24bit(float v)
{
	return float(int32_t(v * 8388608.0f)) * (1.0f / 8388608.0f);
}

// README の「足し続ける位相」。実機と同じ 24 ビットの整数で足す（float で足すと丸めが積もって、位相がずれていく）
struct phase_word {
	int32_t word = 0;
	float value() const { return float(word) * (1.0f / 8388608.0f); }
	void add(double increment)
	{
		const int32_t sum = int32_t(double(word) + increment * 8388608.0);   // 0 の側へ切り捨てる
		word = int32_t(uint32_t(sum) << 8) >> 8;
	}
};

// README の「表の読み出し」。position は表全体の中の番号
inline float table_read(const float *table, float position)
{
	const float whole = std::floor(position);
	const float fraction = position - whole;
	const int32_t index = int32_t(whole);
	const float a = table[index];
	return sat(a + fraction * (table[index + 1] - a));
}

} // namespace fx_flow

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_PARTS_H
