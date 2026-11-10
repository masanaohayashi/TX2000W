// license:BSD-3-Clause
// S-MU2000: doc/fx-flow/ のフローから書いたエフェクトを、形の番号から選んで同じ口で呼ぶ。

#ifndef S_MU2000_NATIVE_FX_FLOW_ALL_H
#define S_MU2000_NATIVE_FX_FLOW_ALL_H

#include "fx_flow_delay.h"
#include "fx_flow_drive.h"
#include "fx_flow_master.h"
#include "fx_flow_misc.h"
#include "fx_flow_phase.h"
#include "fx_flow_rotary.h"
#include "xg/fx_settings.h"

#include <memory>

namespace smu2000::native_fx {

class effect_flow
{
public:
	virtual ~effect_flow() = default;
	// バリエーションとインサーションの指示から設定する。LFO と遅延メモリの曲線は extras に返す
	virtual void configure(const fx_settings::effect_request &, fx_settings::effect_extras &) {}
	// システムコーラスの指示から設定する
	virtual void configure(const fx_settings::system_request &, u16 lfo[4]) {}
	virtual void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n) = 0;
	virtual void reset() = 0;     // 残響を消す。係数はそのまま
};

template<class Flow, class Params>
class effect_flow_of final : public effect_flow
{
public:
	void configure(const fx_settings::effect_request &in, fx_settings::effect_extras &extras) override
	{
		Params p;
		fx_settings::settle(in, p, extras);
		m_flow.configure(p);
	}
	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *table,
		float *out_left, float *out_right, int n) override
	{
		m_flow.process(in_left, in_right, lfo, table, out_left, out_right, n);
	}
	void reset() override { m_flow.reset(); }
private:
	Flow m_flow;
};

// system_chorus は LFO の数と口が違うので、ここで合わせる
class system_chorus_flow final : public effect_flow
{
public:
	void configure(const fx_settings::system_request &in, u16 lfo[4]) override
	{
		chorus_params p;
		fx_settings::settle_chorus(in, p, lfo);
		m_flow.configure(p);
	}
	void process(const float *in_left, const float *in_right, const float *const lfo[6], const float *,
		float *out_left, float *out_right, int n) override
	{
		const float *const voices[4] = { lfo[0], lfo[1], lfo[2], lfo[3] };
		m_flow.process(in_left, in_right, voices, out_left, out_right, n);
	}
	void reset() override { m_flow.reset(); }
private:
	chorus_flow m_flow;
};

template<class Flow, class Params>
class system_flow_of final : public effect_flow
{
public:
	void configure(const fx_settings::system_request &in, u16 lfo[4]) override
	{
		Params p;
		fx_settings::settle_chorus(in, p, lfo);
		m_flow.configure(p);
	}
	void process(const float *in_left, const float *in_right, const float *const lfos[6], const float *table,
		float *out_left, float *out_right, int n) override
	{
		m_flow.process(in_left, in_right, lfos, table, out_left, out_right, n);
	}
	void reset() override { m_flow.reset(); }
private:
	Flow m_flow;
};

// バリエーション・インサーションの形の組
inline std::unique_ptr<effect_flow> make_variation_effect(int group)
{
#define S_FLOW(name) std::make_unique<effect_flow_of<name##_flow, name##_params>>()
	switch (group) {
	case 0: return S_FLOW(modulated_delay);
	case 1: return S_FLOW(rotary);
	case 2: return S_FLOW(phaser);
	case 3: return S_FLOW(early_reflection);
	case 4: return S_FLOW(auto_wah);
	case 5: return S_FLOW(distortion);
	case 6: return S_FLOW(variation_reverb);
	case 7: return S_FLOW(pitch_shift);
	case 9: return S_FLOW(dual_rotary);
	case 10: return S_FLOW(talk_mod);
	case 11: return S_FLOW(lo_fi);
	case 13: return S_FLOW(wah_distortion_delay);
	case 14: return S_FLOW(distortion_delay);
	case 15: return S_FLOW(v_distortion);
	case 16: return S_FLOW(stereo_distortion);
	case 17: return S_FLOW(distortion_rotary);
	case 18: return S_FLOW(chorus3);
	case 19: return S_FLOW(amp_sim2);
	case 20: return S_FLOW(v_flanger);
	case 21: return S_FLOW(multi_comp);
	case 22: return S_FLOW(dyna_flanger);
	case 23: return S_FLOW(dyna_phaser);
	case 24: return S_FLOW(dyna_filter);
	case 25: return S_FLOW(dyna_ring);
	case 26: return S_FLOW(auto_pan2);
	case 27: return S_FLOW(low_reso);
	case 28: return S_FLOW(isolator);
	case 29: return S_FLOW(slice);
	case 30: return S_FLOW(ring_mod);
	case 31: return S_FLOW(turntable);
	default: return nullptr;
	}
}

// システムコーラスの形の組
inline std::unique_ptr<effect_flow> make_system_chorus(int group)
{
	switch (group) {
	case 0: return std::make_unique<system_chorus_flow>();
	case 1: return std::make_unique<system_flow_of<system_phaser_flow, system_phaser_params>>();
	case 2: return std::make_unique<system_flow_of<system_detune_flow, system_detune_params>>();
	case 3: return std::make_unique<system_flow_of<system_chorus3_flow, system_chorus3_params>>();
	default: return nullptr;
	}
#undef S_FLOW
}

} // namespace smu2000::native_fx

#endif // S_MU2000_NATIVE_FX_FLOW_ALL_H
