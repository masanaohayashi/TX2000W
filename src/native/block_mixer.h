// license:BSD-3-Clause
// S-MU2000: doc/voice-flow/mixer.md の 4 章・5 章・6 章の 2-7（マスター EQ の手前まで）。

#ifndef S_MU2000_NATIVE_BLOCK_MIXER_H
#define S_MU2000_NATIVE_BLOCK_MIXER_H

#include "voice_bank.h"

namespace native_voice {

enum effect_id : int { effect_ins1, effect_ins2, effect_ins3, effect_ins4, effect_variation, effect_chorus, effect_reverb, effect_count };

class block_mixer {
public:
	block_mixer() { reset(); }

	void reset()
	{
		std::memset(static_cast<void *>(return_history), 0, sizeof(return_history));
		std::memset(static_cast<void *>(link_held), 0, sizeof(link_held));
		std::memset(static_cast<void *>(return_gain), 0, sizeof(return_gain));
		std::memset(static_cast<void *>(&buses), 0, sizeof(buses));
		std::fill(std::begin(chain_bus), std::end(chain_bus), -1);
	}

	// 5 章。pan[入力 0 左,1 右][出力 0 左,1 右]、level_att は dry, reverb, chorus, variation の順の減衰（行かない先は 255）
	void set_return(effect_id e, const int pan[2][2], const int level_att[4])
	{
		for (int target = 0; target < return_target_count; target++)
			for (int input = 0; input < 2; input++)
				for (int output = 0; output < 2; output++)
					return_gain[e][target][input][output] = att_gain(pan[input][output] + level_att[target]);
	}

	void set_chain(effect_id e, int next_bus, float gain)
	{
		chain_bus[e] = next_bus;
		chain_gain[e] = gain;
	}

	// run(effect_id e, const float *in_left, const float *in_right, float *out_left, float *out_right, int n)
	// dry の n サンプルを dry_left, dry_right に書く
	template <class Runner>
	void process(int n, const bus_block &first, const bus_block &second, Runner &&run, float *dry_left, float *dry_right)
	{
		// インサーション 1 から 2-4 へ直列につなぐときは、1 を先に回す
		const bool ins1_first = chain_bus[effect_ins1] >= 0;
		if (ins1_first) {
			build_first_chip_bus(bus_ins1, n, first, second);
			run_effect(effect_ins1, bus_ins1, n, run);
		}
		// 2. insertion_send[2..4]: 第 2 群はそのまま、第 1 群は線を通って 1 サンプル遅れる
		for (int k = 0; k < insertion_late_count; k++) {
			const int bus = bus_ins2 + k;
			for (int channel = 0; channel < 2; channel++) {
				float *out = buses.s[bus][channel];
				const float *delayed = first.s[bus][channel];
				const float *direct = second.s[bus][channel];
				out[0] = direct[0] + limit_link(link_held[bus][channel]);
				for (int s = 1; s < n; s++)
					out[s] = direct[s] + limit_link(delayed[s - 1]);
			}
			for (int e = effect_ins1; e < effect_ins2 + k; e++)
				if (chain_bus[e] == bus)
					add_chain(effect_id(e), bus, n);
			run_effect(effect_id(effect_ins2 + k), bus, n, run);
		}
		// 3. insertion_send[1]
		if (!ins1_first) {
			build_first_chip_bus(bus_ins1, n, first, second);
			run_effect(effect_ins1, bus_ins1, n, run);
		}
		// 4. variation_send
		build_first_chip_bus(bus_variation, n, first, second);
		add_return(effect_ins1, bus_variation, n);
		run_effect(effect_variation, bus_variation, n, run);
		// 5. chorus_send
		build_first_chip_bus(bus_chorus, n, first, second);
		add_return(effect_ins1, bus_chorus, n);
		add_return(effect_variation, bus_chorus, n);
		run_effect(effect_chorus, bus_chorus, n, run);
		// 6. reverb_send
		build_first_chip_bus(bus_reverb, n, first, second);
		add_return(effect_ins1, bus_reverb, n);
		add_return(effect_variation, bus_reverb, n);
		add_return(effect_chorus, bus_reverb, n);
		run_effect(effect_reverb, bus_reverb, n, run);
		// 7. dry
		build_first_chip_bus(bus_dry, n, first, second);
		add_return(effect_ins1, bus_dry, n);
		add_return(effect_variation, bus_dry, n);
		add_return(effect_chorus, bus_dry, n);
		add_return(effect_reverb, bus_dry, n);
		std::copy_n(buses.s[bus_dry][0], n, dry_left);
		std::copy_n(buses.s[bus_dry][1], n, dry_right);

		remember_block_end(n, first, second);
	}

	// 直前の process で作った各母線（dry・reverb・…・ins4 の順。試験が読む）
	const bus_block &sends() const { return buses; }

private:
	static constexpr float link_limit = 8.0f;
	static constexpr float return_limit = 1.0f; // [sat]
	static constexpr int return_target_count = 4; // dry, reverb, chorus, variation
	static constexpr int insertion_late_count = 3; // インサーション 2-4
	static constexpr int return_delay = 1, link_return_delay = 2;
	// 戻りの配列の頭に前のブロックの最後の 2 サンプルを置く。run へ渡す先頭を 32 バイトに揃えるので 8 個ぶん空ける
	static constexpr int return_lead = 8;

	static float limit_link(float v) { return voice_flow::sat(v, link_limit); }

	// 第 1 群の母線: 第 1 群の声 + 線[t-1]。線[t-1] = clamp(第 2 群の声の和[t-1] + インサーション 2-4 の戻り[t-2] × 利得)
	// 利得はこのブロックのものを、持ち越した分にも掛ける（4 章の太字）。insertion_send[1] の線に戻りは乗らない
	void build_first_chip_bus(int bus, int n, const bus_block &first, const bus_block &second)
	{
		for (int output = 0; output < 2; output++) {
			alignas(32) float line[max_block];
			line[0] = link_held[bus][output];
			std::copy_n(second.s[bus][output], n - 1, line + 1);
			if (bus < return_target_count) {
				for (int k = 0; k < insertion_late_count; k++)
					for (int input = 0; input < 2; input++) {
						const float gain = return_gain[effect_ins2 + k][bus][input][output];
						if (gain == 0.0f)
							continue;
						const float *delayed = return_history[effect_ins2 + k][input] + return_lead - link_return_delay;
						for (int s = 0; s < n; s++)
							line[s] += gain * delayed[s];
					}
			}
			float *out = buses.s[bus][output];
			const float *direct = first.s[bus][output];
			for (int s = 0; s < n; s++)
				out[s] = direct[s] + limit_link(line[s]);
		}
	}

	void add_return(effect_id e, int bus, int n)
	{
		for (int output = 0; output < 2; output++)
			for (int input = 0; input < 2; input++) {
				const float gain = return_gain[e][bus][input][output];
				if (gain == 0.0f)
					continue;
				float *out = buses.s[bus][output];
				const float *delayed = return_history[e][input] + return_lead - return_delay;
				for (int s = 0; s < n; s++)
					out[s] += gain * delayed[s];
			}
	}

	void add_chain(effect_id e, int bus, int n)
	{
		for (int channel = 0; channel < 2; channel++) {
			float *out = buses.s[bus][channel];
			const float *from = return_history[e][channel] + return_lead;
			for (int s = 0; s < n; s++)
				out[s] += chain_gain[e] * from[s];
		}
	}

	template <class Runner>
	void run_effect(effect_id e, int bus, int n, Runner &run)
	{
		float *out_left = return_history[e][0] + return_lead;
		float *out_right = return_history[e][1] + return_lead;
		run(e, buses.s[bus][0], buses.s[bus][1], out_left, out_right, n);
		for (int s = 0; s < n; s++) {
			out_left[s] = voice_flow::sat(out_left[s], return_limit);
			out_right[s] = voice_flow::sat(out_right[s], return_limit);
		}
	}

	void remember_block_end(int n, const bus_block &first, const bus_block &second)
	{
		for (int bus = 0; bus < bus_count; bus++) {
			const bus_block &source = bus >= bus_ins2 ? first : second;
			link_held[bus][0] = source.s[bus][0][n - 1];
			link_held[bus][1] = source.s[bus][1][n - 1];
		}
		for (int e = 0; e < effect_count; e++)
			for (int channel = 0; channel < 2; channel++) {
				float *history = return_history[e][channel];
				const float older = history[return_lead + n - 2], newest = history[return_lead + n - 1];
				history[return_lead - 2] = older;
				history[return_lead - 1] = newest;
			}
	}

	alignas(32) float return_history[effect_count][2][return_lead + max_block];
	float link_held[bus_count][2]; // 線へ入る声の和の、前のブロックの最後のサンプル
	float return_gain[effect_count][return_target_count][2][2]; // [戻り][先][入力][出力]
	int chain_bus[effect_count] = { -1, -1, -1, -1, -1, -1, -1 };
	float chain_gain[effect_count] = {};
	bus_block buses;
};

} // namespace native_voice

#endif // S_MU2000_NATIVE_BLOCK_MIXER_H
