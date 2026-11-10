// license:BSD-3-Clause
//
// 利用者のプログラム ROM の中の、表の位置。位置の数は、このファイルだけに置く。

#ifndef S_MU2000_XG_EFFECT_ROM_LAYOUT_H
#define S_MU2000_XG_EFFECT_ROM_LAYOUT_H

#pragma once

namespace smu2000::xg::effect_rom_layout {

inline constexpr int amp_sim2_drive_curve = 1640104;
inline constexpr int amp_sim2_low_drive_cabinet_level = 1640522;
inline constexpr int amp_sim2_param2_table = 1633352;
inline constexpr int amp_sim2_output_level_curve = 1640556;
inline constexpr int amp_sim2_table = 1643240;
inline constexpr int amp_type_table = 1633296;
inline constexpr int auto_pan2_phase_step_table = 1622224;
inline constexpr int auto_pan2_switch_steepness_table = 1622166;
inline constexpr int auto_pan2_triangle_curve = 1624786;
inline constexpr int auto_pan2_sine_curve = 1622736;
inline constexpr int chorus_kind_valid = 1453982;
inline constexpr int chorus_kind_makes = 1453892;
inline constexpr int comp_row_table = 1633496;
inline constexpr int reverb_parts_room_comb_lengths = 1636890;
inline constexpr int reverb_room_scale = 1621718;
inline constexpr int reverb_parts_room_decay_offsets = 1637290;
inline constexpr int dry_wet_curve = 1617176;
inline constexpr int dyna_parts_attack_time_table = 1627604;
inline constexpr int dyna_parts_release_time_table = 1628116;
inline constexpr int dyna_parts_level_curve = 1629140;
inline constexpr int dyna_filter_resonance_table = 1627092;
inline constexpr int dyna_flanger_sensitivity_curve = 1628628;
inline constexpr int dyna_ring_table = 1672622;
inline constexpr int early_reflection_allpass_row_by_type = 1621652;
inline constexpr int early_reflection_liveness_shift = 1638338;
inline constexpr int early_reflection_room_size_scale = 1638210;
inline constexpr int early_reflection_reflection_pattern = 1637490;
inline constexpr int fb_gain_table = 1634414;
inline constexpr int low_reso_scratch_soft_zone_near = 1673200;
inline constexpr int low_reso_scratch_soft_zone_steps = 1673136;
inline constexpr int low_reso_resolution_curve_near = 1673662;
inline constexpr int low_reso_resolution_curve_steps = 1673218;
inline constexpr int filter_frequency_table = 1615336;
inline constexpr int filter_gain_table = 1615580;
inline constexpr int filter_sqrt_gain_table = 1615680;
inline constexpr int send_attenuation_table = 1808824;
inline constexpr int part_gain_attenuation_table = 1808692;
inline constexpr int symphonic_detune_param14_table = 1612060;
inline constexpr int chorus3_wide_depth_curve = 1613568;
inline constexpr int chorus3_narrow_depth_curve = 1613312;
inline constexpr int pitch_shift_detune_ratio_table = 1612880;
inline constexpr int isolator_band_level_table = 1616664;
inline constexpr int karaoke_param1_table = 1618334;
inline constexpr int lfo_speed_table = 1611932;
inline constexpr int chorus_delay_table = 1617822;
inline constexpr int lo_fi_output_gain_table = 1639732;
inline constexpr int lo_fi_filter_type_table = 1639842;
inline constexpr int multi_comp_band_gain_table = 1629832;
inline constexpr int multi_comp_param1_table = 1630052;
inline constexpr int multi_comp_table = 1655722;
inline constexpr int multi_comp_soft_curve = 1663914;
inline constexpr int multi_comp_output_level = 1616408;
inline constexpr int multi_comp_attack_speed_table = 1629944;
inline constexpr int multi_comp_release_speed_table = 1630004;
inline constexpr int multi_comp_crossover_table = 1629652;
inline constexpr int out_gain_table = 1618620;
inline constexpr int pan_attenuation_table = 1617692;
inline constexpr int return_attenuation_table = 1616536;
inline constexpr int pitch_shift_low_adjust_frequency_table = 1613284;
inline constexpr int pitch_shift_saw_speed_table = 1612286;
inline constexpr int pitch_shift_semitone_depth_table = 1612188;
inline constexpr int pitch_shift_ratio_one_depth_table = 1612488;
inline constexpr int pitch_shift_exact_semitone_depth_table = 1612684;
inline constexpr int pitch_shift_fine_depth_table = 1612384;
inline constexpr int rev_all_tail_table = 1621760;
inline constexpr int system_return_levels = 1618590;
inline constexpr int reverb_default_params = 1579912;
inline constexpr int reverb_size_classes = 1621472;
inline constexpr int reverb_time_table = 1634060;
inline constexpr int full_at_center_curve = 1617434;
inline constexpr int system_early_tap_table = 1635750;
inline constexpr int reverb_delay_samples_table = 1618078;
inline constexpr int early_allpass_rows = 1638358;
inline constexpr int early_section_switches = 1638350;
inline constexpr int reverb_density_rows = 1635600;
inline constexpr int reverb_density_active_bits = 1635720;
inline constexpr int reverb_diffusion_rows = 1635480;
inline constexpr int reverb_diffusion_offsets = 1636038;
inline constexpr int reverb_dim_line_layout = 1580960;
inline constexpr int reverb_kind_by_type = 1453684;
inline constexpr int reverb_kind_valid = 1453784;
inline constexpr int reverb_kind_high_types = 1453884;
inline constexpr int reverb_record_numbers = 1580872;
inline constexpr int reverb_records = 1578752;
inline constexpr int reverb_standard_line_layout = 1580904;
inline constexpr int reverb_second_line_layout = 1580932;
inline constexpr int rev_time_table = 1635438;
inline constexpr int reverb_parts_decay_steps = 1634344;
inline constexpr int reverb_comb_decay_offsets = 1635449;
inline constexpr int ring_mod_carrier_lowest_increment = 1621820;
inline constexpr int ring_mod_carrier_highest_increment = 1621852;
inline constexpr int drive_parts_release_feedback_table = 1633676;
inline constexpr int set_clip_rise_table = 1633636;
inline constexpr int drive_parts_output_level_table = 1616024;
inline constexpr int multi_comp_crossover_speed_curve = 1615780;
inline constexpr int sixth_octave_gain_table = 1633624;
inline constexpr int slice_gate_phase_4_pulses = 1673728;
inline constexpr int slice_gate_phase_8_pulses = 1673830;
inline constexpr int slice_gate_phase_16_pulses = 1673932;
inline constexpr int slice_aeg_type_waves = 1689584;
inline constexpr int slice_pan_type_waves = 1679324;
inline constexpr int slice_aeg_phase_table = 1674162;
inline constexpr int slice_divide_16_pulses = 1677272;
inline constexpr int slice_divide_8_pulses = 1676246;
inline constexpr int slice_divide_4_pulses = 1675220;
inline constexpr int talk_mod_vowel_table = 1638468;
inline constexpr int tempo_lfo_table = 1643136;
inline constexpr int v_flanger_phase_offset_table = 1630436;
inline constexpr int v_flanger_damp_speed_table = 1630672;
inline constexpr int v_flanger_allpass_base_table = 1631204;
inline constexpr int v_flanger_phase_rate_table = 1630692;
inline constexpr int v_flanger_smooth_speed_table = 1630948;
inline constexpr int v_flanger_allpass_depth_table = 1631226;
inline constexpr int v_flanger_table = 1651434;
inline constexpr int v_flanger_octave_fraction_curve = 1631248;
inline constexpr int v_flanger_percent_curve = 1630470;
inline constexpr int v_distortion_transistor_overdrive = 1639902;
inline constexpr int v_distortion_transistor_cabinet_level = 1640508;
inline constexpr int v_distortion_overdrive = 1640306;
inline constexpr int v_distortion_soft_and_cabinet = 1640536;
inline constexpr int vdist_p0_table = 1640808;
inline constexpr int vdist_p1_table = 1640758;
inline constexpr int v_distortion_speaker_filter_table = 1640858;
inline constexpr int v_distortion_speaker_presence_second_order = 1640960;
inline constexpr int v_distortion_speaker_presence_first_order = 1642220;

// 効果の種類の記録。回路の組ごとに、種類の番号から記録の番号を引く表、記録の先頭、既定値の並び
struct effect_records { int numbers, first, defaults; };

// バリエーション
inline constexpr effect_records variation_modulated_delay = { 1591216, 1587952, 1477872 };
inline constexpr effect_records variation_rotary = { 1593404, 1591252, 1592860 };
inline constexpr effect_records variation_phaser = { 1596996, 1595968, 1596772 };
inline constexpr effect_records variation_early_reflection = { 1587340, 1586280, 1587084 };
inline constexpr effect_records variation_auto_wah = { 1603264, 1601980, 1602784 };
inline constexpr effect_records variation_distortion = { 1584464, 1582472, 1584080 };
inline constexpr effect_records variation_variation_reverb = { 1599036, 1597004, 1598076 };
inline constexpr effect_records variation_pitch_shift = { 1595956, 1594028, 1595636 };
inline constexpr effect_records variation_dual_rotary = { 1599968, 1599068, 1599872 };
inline constexpr effect_records variation_talk_mod = { 1601372, 1600772, 1601308 };
inline constexpr effect_records variation_lo_fi = { 1587948, 1587348, 1587884 };
inline constexpr effect_records variation_wah_distortion_delay = { 1604040, 1603280, 1603816 };
inline constexpr effect_records variation_distortion_delay = { 1582456, 1581236, 1582040 };
inline constexpr effect_records variation_v_distortion = { 1585236, 1584476, 1585012 };
inline constexpr effect_records variation_stereo_distortion = { 1600764, 1599972, 1600508 };
inline constexpr effect_records variation_distortion_rotary = { 1586272, 1585244, 1480112 };
inline constexpr effect_records variation_chorus3 = { 1594024, 1593424, 1593960 };
inline constexpr effect_records variation_amp_sim2 = { 1604648, 1604048, 1604584 };
inline constexpr effect_records variation_v_flanger = { 1605252, 1604652, 1605188 };
inline constexpr effect_records variation_multi_comp = { 1605856, 1605256, 1605792 };
inline constexpr effect_records variation_dyna_flanger = { 1606460, 1605860, 1606396 };
inline constexpr effect_records variation_dyna_phaser = { 1607064, 1606464, 1607000 };
inline constexpr effect_records variation_dyna_filter = { 1607668, 1607068, 1607604 };
inline constexpr effect_records variation_dyna_ring = { 1608272, 1607672, 1608208 };
inline constexpr effect_records variation_auto_pan2 = { 1608876, 1608276, 1608812 };
inline constexpr effect_records variation_low_reso = { 1609512, 1608880, 1609416 };
inline constexpr effect_records variation_isolator = { 1610116, 1609516, 1610052 };
inline constexpr effect_records variation_slice = { 1610720, 1610120, 1610656 };
inline constexpr effect_records variation_ring_mod = { 1611324, 1610724, 1611260 };
inline constexpr effect_records variation_turntable = { 1611928, 1611328, 1611864 };

// インサーション 1
inline constexpr effect_records insertion_modulated_delay = { 1556968, 1553704, 1478992 };
inline constexpr effect_records insertion_rotary = { 1559156, 1557004, 1558612 };
inline constexpr effect_records insertion_phaser = { 1563348, 1562320, 1563124 };
inline constexpr effect_records insertion_early_reflection = { 1553088, 1551728, 1552800 };
inline constexpr effect_records insertion_auto_wah = { 1569616, 1568332, 1569136 };
inline constexpr effect_records insertion_distortion = { 1549912, 1547920, 1549528 };
inline constexpr effect_records insertion_variation_reverb = { 1565388, 1563356, 1564428 };
inline constexpr effect_records insertion_pitch_shift = { 1562308, 1559780, 1561924 };
inline constexpr effect_records insertion_dual_rotary = { 1566320, 1565420, 1566224 };
inline constexpr effect_records insertion_talk_mod = { 1567724, 1567124, 1567660 };
inline constexpr effect_records insertion_lo_fi = { 1553700, 1553100, 1553636 };
inline constexpr effect_records insertion_wah_distortion_delay = { 1570392, 1569632, 1570168 };
inline constexpr effect_records insertion_distortion_delay = { 1547904, 1546684, 1547488 };
inline constexpr effect_records insertion_v_distortion = { 1550684, 1549924, 1550460 };
inline constexpr effect_records insertion_stereo_distortion = { 1567116, 1566324, 1566860 };
inline constexpr effect_records insertion_distortion_rotary = { 1551720, 1550692, 1480336 };
inline constexpr effect_records insertion_chorus3 = { 1559776, 1559176, 1559712 };
inline constexpr effect_records insertion_amp_sim2 = { 1571000, 1570400, 1570936 };
inline constexpr effect_records insertion_v_flanger = { 1571604, 1571004, 1571540 };
inline constexpr effect_records insertion_multi_comp = { 1572208, 1571608, 1572144 };
inline constexpr effect_records insertion_dyna_flanger = { 1572812, 1572212, 1572748 };
inline constexpr effect_records insertion_dyna_phaser = { 1573416, 1572816, 1573352 };
inline constexpr effect_records insertion_dyna_filter = { 1574020, 1573420, 1573956 };
inline constexpr effect_records insertion_dyna_ring = { 1574624, 1574024, 1574560 };
inline constexpr effect_records insertion_auto_pan2 = { 1575228, 1574628, 1575164 };
inline constexpr effect_records insertion_low_reso = { 1576132, 1575232, 1576036 };
inline constexpr effect_records insertion_isolator = { 1576736, 1576136, 1576672 };
inline constexpr effect_records insertion_slice = { 1577340, 1576740, 1577276 };
inline constexpr effect_records insertion_ring_mod = { 1577944, 1577344, 1577880 };
inline constexpr effect_records insertion_turntable = { 1578548, 1577948, 1578484 };

// インサーション 2-4
inline constexpr effect_records extra_insertion_modulated_delay = { 1785976, 1782712, 1480560 };
inline constexpr effect_records extra_insertion_rotary = { 1788164, 1786012, 1787620 };
inline constexpr effect_records extra_insertion_phaser = { 1792356, 1791328, 1792132 };
inline constexpr effect_records extra_insertion_early_reflection = { 1782100, 1781040, 1781844 };
inline constexpr effect_records extra_insertion_auto_wah = { 1798624, 1797340, 1798144 };
inline constexpr effect_records extra_insertion_distortion = { 1777884, 1775892, 1777500 };
inline constexpr effect_records extra_insertion_variation_reverb = { 1794396, 1792364, 1793436 };
inline constexpr effect_records extra_insertion_pitch_shift = { 1791316, 1788788, 1790932 };
inline constexpr effect_records extra_insertion_dual_rotary = { 1795328, 1794428, 1795232 };
inline constexpr effect_records extra_insertion_talk_mod = { 1796732, 1796132, 1796668 };
inline constexpr effect_records extra_insertion_lo_fi = { 1782708, 1782108, 1782644 };
inline constexpr effect_records extra_insertion_wah_distortion_delay = { 1799400, 1798640, 1799176 };
inline constexpr effect_records extra_insertion_distortion_delay = { 1775876, 1774656, 1775460 };
inline constexpr effect_records extra_insertion_v_distortion = { 1779996, 1777896, 1779772 };
inline constexpr effect_records extra_insertion_stereo_distortion = { 1796124, 1795332, 1795868 };
inline constexpr effect_records extra_insertion_distortion_rotary = { 1781032, 1780004, 1482448 };
inline constexpr effect_records extra_insertion_chorus3 = { 1788784, 1788184, 1788720 };
inline constexpr effect_records extra_insertion_amp_sim2 = { 1800008, 1799408, 1799944 };
inline constexpr effect_records extra_insertion_v_flanger = { 1800612, 1800012, 1800548 };
inline constexpr effect_records extra_insertion_multi_comp = { 1801216, 1800616, 1801152 };
inline constexpr effect_records extra_insertion_dyna_flanger = { 1801820, 1801220, 1801756 };
inline constexpr effect_records extra_insertion_dyna_phaser = { 1802424, 1801824, 1802360 };
inline constexpr effect_records extra_insertion_dyna_filter = { 1803028, 1802428, 1802964 };
inline constexpr effect_records extra_insertion_dyna_ring = { 1803632, 1803032, 1803568 };
inline constexpr effect_records extra_insertion_auto_pan2 = { 1804236, 1803636, 1804172 };
inline constexpr effect_records extra_insertion_low_reso = { 1804872, 1804240, 1804776 };
inline constexpr effect_records extra_insertion_isolator = { 1805476, 1804876, 1805412 };
inline constexpr effect_records extra_insertion_slice = { 1806080, 1805480, 1806016 };
inline constexpr effect_records extra_insertion_ring_mod = { 1806684, 1806084, 1806620 };
inline constexpr effect_records extra_insertion_turntable = { 1807288, 1806688, 1807224 };

// 組の番号の順。8 と 12 の組は種類から選ばれない
inline constexpr effect_records no_records = {};
inline constexpr int effect_group_count = 32;
inline constexpr effect_records variation_records[effect_group_count] = {
	variation_modulated_delay, variation_rotary, variation_phaser,
	variation_early_reflection, variation_auto_wah, variation_distortion,
	variation_variation_reverb, variation_pitch_shift, no_records,
	variation_dual_rotary, variation_talk_mod, variation_lo_fi,
	no_records, variation_wah_distortion_delay, variation_distortion_delay,
	variation_v_distortion, variation_stereo_distortion, variation_distortion_rotary,
	variation_chorus3, variation_amp_sim2, variation_v_flanger,
	variation_multi_comp, variation_dyna_flanger, variation_dyna_phaser,
	variation_dyna_filter, variation_dyna_ring, variation_auto_pan2,
	variation_low_reso, variation_isolator, variation_slice,
	variation_ring_mod, variation_turntable,
};
inline constexpr effect_records insertion_records[effect_group_count] = {
	insertion_modulated_delay, insertion_rotary, insertion_phaser,
	insertion_early_reflection, insertion_auto_wah, insertion_distortion,
	insertion_variation_reverb, insertion_pitch_shift, no_records,
	insertion_dual_rotary, insertion_talk_mod, insertion_lo_fi,
	no_records, insertion_wah_distortion_delay, insertion_distortion_delay,
	insertion_v_distortion, insertion_stereo_distortion, insertion_distortion_rotary,
	insertion_chorus3, insertion_amp_sim2, insertion_v_flanger,
	insertion_multi_comp, insertion_dyna_flanger, insertion_dyna_phaser,
	insertion_dyna_filter, insertion_dyna_ring, insertion_auto_pan2,
	insertion_low_reso, insertion_isolator, insertion_slice,
	insertion_ring_mod, insertion_turntable,
};
inline constexpr effect_records extra_insertion_records[effect_group_count] = {
	extra_insertion_modulated_delay, extra_insertion_rotary, extra_insertion_phaser,
	extra_insertion_early_reflection, extra_insertion_auto_wah, extra_insertion_distortion,
	extra_insertion_variation_reverb, extra_insertion_pitch_shift, no_records,
	extra_insertion_dual_rotary, extra_insertion_talk_mod, extra_insertion_lo_fi,
	no_records, extra_insertion_wah_distortion_delay, extra_insertion_distortion_delay,
	extra_insertion_v_distortion, extra_insertion_stereo_distortion, extra_insertion_distortion_rotary,
	extra_insertion_chorus3, extra_insertion_amp_sim2, extra_insertion_v_flanger,
	extra_insertion_multi_comp, extra_insertion_dyna_flanger, extra_insertion_dyna_phaser,
	extra_insertion_dyna_filter, extra_insertion_dyna_ring, extra_insertion_auto_pan2,
	extra_insertion_low_reso, extra_insertion_isolator, extra_insertion_slice,
	extra_insertion_ring_mod, extra_insertion_turntable,
};

// システムのコーラスの記録。組ごと
inline constexpr effect_records system_chorus_common = { 1545780, 1544420, 1544980 };
inline constexpr effect_records system_chorus_phaser = { 1546680, 1546392, 1546616 };
inline constexpr effect_records system_chorus_ensemble_detune = { 1546388, 1546100, 1546324 };
inline constexpr effect_records system_chorus_chorus3 = { 1546096, 1545808, 1546032 };
inline constexpr effect_records system_chorus_records[4] = { system_chorus_common, system_chorus_phaser, system_chorus_ensemble_detune, system_chorus_chorus3 };

// 種類の MSB から組を引く表
inline constexpr int variation_group_by_msb = 1454072, insertion_group_by_msb = 1456216;

// MSB の範囲ごとに、LSB を引く表。行の長さは LSB の数
// lsb_valid: 使える LSB なら 0 以外。record_numbers: 組の中の種類の番号
struct effect_type_range { int first_msb, end_msb, lsb_count, lsb_valid, record_numbers; };
inline constexpr effect_type_range variation_type_ranges[] = {
	{ 0, 6, 20, 1454320, 1454200 },
	{ 6, 12, 17, 1454542, 1454440 },
	{ 16, 23, 9, 1454707, 1454644 },
	{ 64, 79, 22, 1455100, 1454770 },
	{ 80, 89, 20, 1455610, 1455430 },
	{ 93, 104, 18, 1455988, 1455790 },
	{ 104, 119, 1, 1456201, 1456186 },
};
inline constexpr effect_type_range insertion_type_ranges[] = {
	{ 0, 6, 20, 1456464, 1456344 },
	{ 6, 12, 17, 1456686, 1456584 },
	{ 16, 23, 9, 1456851, 1456788 },
	{ 64, 79, 22, 1457244, 1456914 },
	{ 80, 89, 20, 1457754, 1457574 },
	{ 93, 104, 18, 1458132, 1457934 },
	{ 104, 119, 1, 1458345, 1458330 },
};

// 回路の出口の利得。組ごとに種類の数ぶん並ぶ
inline constexpr int variation_output_gain_table = 1618720, insertion_output_gain_table = 1619840;

// 記録の中身。利得、読み出し位置、LFO の順
inline constexpr int effect_record_size = 0x10C, effect_record_read_positions = 192, effect_record_lfos = 256;
} // namespace smu2000::xg::effect_rom_layout

#endif
