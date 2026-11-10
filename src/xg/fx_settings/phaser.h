// license:BSD-3-Clause
// S-MU2000: PHASER 1、PHASER 2、T.PHASER。doc/fx-flow/parameters.md の phaser。
// 左右のオールパスの鎖の係数を LFO で動かす。Stage は音が通る段の数。

#ifndef S_MU2000_XG_FX_SETTINGS_PHASER_H
#define S_MU2000_XG_FX_SETTINGS_PHASER_H

namespace smu2000::fx_settings {

namespace phaser {

constexpr int tempo_phaser_make = 6;
constexpr int center_step = panel_value_step, shifted_right_center_step = 242;	// PhaseShift 1 あたりの掃引の中心。PHASER 2 系は右を 15/16 に下げて左右をずらす
constexpr int lfo_phase_step = 0x8000 / 60;

// 掃引の中心と、そこから上の残りを LFO Depth で割った深さ
inline void set_sweep(phaser_params &p, int ch, int phase_shift, int lfo_depth, int step)
{
	const int center = s16(u16(phase_shift * step));
	p.sweep_center[ch] = scaled(center);
	p.sweep_depth[ch] = scaled(((0x7fff - center) * lfo_depth) / 127);
}

// 鎖の段のうち、取り出す 1 段だけを 1 倍にする
inline void select_taps(float *taps, int selected)
{
	for (int k = 0; k < 5; k++)
		taps[k] = k == selected ? 1.0f : 0.0f;
}

// PHASER 1: 左の鎖の取り出しを右の鎖へ入れて 1 本につなぎ、右の取り出しを左の鎖へ戻す
inline void cross_feedback_phaser(const effect_request &in, phaser_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	const int stage = P[10], diffusion = P[11];
	set_lfo_speed(x.lfo[0], in.rom, P[0]);
	for (int ch = 0; ch < 2; ch++)
		set_sweep(p, ch, P[2], P[1], center_step);
	p.left_chain_from_right_tap = scaled(centered_feedback(P[3]));
	// Stage は左右の段数の和。取り出しは左の段 2-6 と右の段 2-6 で、8 までは右を 2 段、9 以上は右を 6 段にする
	select_taps(p.left_tap, stage <= 4 ? 0 : stage <= 8 ? stage - 4 : stage - 8);
	select_taps(p.right_tap, stage >= 9 ? 4 : 0);
	// Diffusion が stereo なら、左の濡れた音を右の取り出しより 1 段手前から取り、右を逆相にして広げる。mono は左右とも同じ音
	p.wet_left_from_right_tap = diffusion == 0 ? 1.0f : 0.0f;
	p.wet_left_from_right_stage1 = diffusion != 0 && stage <= 8 ? 1.0f : 0.0f;
	p.wet_left_from_right_stage5 = diffusion != 0 && stage > 8 ? 1.0f : 0.0f;
	set_dry_wet(in, p, P[9]);
	if (diffusion)
		p.wet_gain[1] = scaled(-dry_wet(in, P[9]).wet, 4.0);
}

// PHASER 2、T.PHASER: 左右とも自分の取り出しを戻し、右の LFO の位相をずらす。Stage は片側の段数
inline void own_feedback_phaser(const effect_request &in, phaser_params &p, effect_extras &x)
{
	const u16 *P = in.P;
	if (in.make == tempo_phaser_make)
		set_lfo_speed_value(x.lfo[0], tempo_lfo_speed(in.rom, P[0]));
	else
		set_lfo_speed(x.lfo[0], in.rom, P[0]);
	set_sweep(p, 0, P[2], P[1], center_step);
	set_sweep(p, 1, P[2], P[1], shifted_right_center_step);
	p.right_chain_from_right_tap = p.left_chain_from_left_tap = scaled(centered_feedback(P[3]));
	const int first_tap_stage = 2;
	select_taps(p.left_tap, P[10] - first_tap_stage);
	select_taps(p.right_tap, P[10] - first_tap_stage);
	p.right_lfo_offset = scaled(((P[12] & 0xff) - 64) * lfo_phase_step);
	set_dry_wet(in, p, P[9]);
}

} // namespace phaser

inline bool settle(const effect_request &in, phaser_params &p, effect_extras &x)
{
	start_settle(in, p, x);
	switch (in.kind) {
	case tables::effect_phaser: phaser::cross_feedback_phaser(in, p, x); break;
	case tables::effect_tempo_phaser: phaser::own_feedback_phaser(in, p, x); break;
	default: return false;
	}
	set_first_order_shelves(in.rom, p, in.P[5], in.P[6], in.P[7], in.P[8], range_4x);
	return true;
}

} // namespace smu2000::fx_settings

#endif // S_MU2000_XG_FX_SETTINGS_PHASER_H
