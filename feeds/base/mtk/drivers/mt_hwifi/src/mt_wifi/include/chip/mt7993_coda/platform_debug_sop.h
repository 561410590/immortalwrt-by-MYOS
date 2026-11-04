#ifndef __MT7993_PLATFORM_DEBUG_SOP_H__
#define __MT7993_PLATFORM_DEBUG_SOP_H__


/******************************************/
/*                                        */
/* 1_[Griffin] conn_infra_bus_debug.xlsx  */
/* - sheet 1_status_result                */
/*                                        */
/******************************************/
/* SectionA - IF CONN_INFRA ON read check ok */
#define PSOP_1_1_00_WRITE_ADDR 0x18023408 /* set conn_infra_off_bus_dbg_group_00 (debug_ctrl_ao) */
#define PSOP_1_1_00_WRITE_VALUE 0x00000000 /* set conn_infra_off_bus_dbg_group_00 (debug_ctrl_ao) */

#define PSOP_1_1_A01_A11_WRITE_ADDR 0x1802340C
#define PSOP_1_1_A01_A11_READ_ADDR 0x18023404

#define PSOP_1_1_A01_WRITE_VALUE 0x00010001 /* [off][debug_ctrl_ao] real time group1_0 (conn_infra_off_apb_bus) */
#define PSOP_1_1_A02_WRITE_VALUE 0x00020001 /* [off][debug_ctrl_ao] real time group1_1 (conn_infra_off_apb_bus) */
#define PSOP_1_1_A03_WRITE_VALUE 0x00030001 /* [off][debug_ctrl_ao] real time group1_2 (conn_infra_off_apb_bus) */
#define PSOP_1_1_A04_WRITE_VALUE 0x00010002 /* [off][debug_ctrl_ao] real time group2_0 (conn_infra_off_apb_bus_hclk) */
#define PSOP_1_1_A05_WRITE_VALUE 0x00000003 /* [off][debug_ctrl_ao] real time group3_0 (conn_infra_off_ahb_bus) */
#define PSOP_1_1_A06_WRITE_VALUE 0x00010003 /* [off][debug_ctrl_ao] real time group3_1 (conn_infra_off_ahb_bus) */
#define PSOP_1_1_A07_WRITE_VALUE 0x00020003 /* [off][debug_ctrl_ao] real time group3_2 (conn_infra_off_ahb_bus) */
#define PSOP_1_1_A08_WRITE_VALUE 0x00030003 /* [off][debug_ctrl_ao] real time group3_3 (conn_infra_off_ahb_bus) */
#define PSOP_1_1_A09_WRITE_VALUE 0x00010004 /* [off][debug_ctrl_ao] real time group1_0 (conn_infra_off_axi_bus) */
#define PSOP_1_1_A10_WRITE_VALUE 0x00020004 /* [off][debug_ctrl_ao] real time group4_1 (conn_infra_off_axi_bus) */
#define PSOP_1_1_A11_WRITE_VALUE 0x00030004 /* [off][debug_ctrl_ao] real time group4_2 (conn_infra_off_axi_bus) */

#define PSOP_1_1_A12_READ_ADDR 0x00030004 /* [von/on/off][timeout protector]conn_infra_bus_timeout_irq */
#define PSOP_1_1_A13_READ_ADDR 0x18023410 /* [von][timeout protector] h_d_n4_timeout_info */
#define PSOP_1_1_A14_READ_ADDR 0x18023410 /* [von][timeout protector] h_d_n4_timeout_addr */
#define PSOP_1_1_A15_READ_ADDR 0x18023418 /* [von][timeout protector] h_d_n4_timeout_wdata */
#define PSOP_1_1_A16_READ_ADDR 0x18023434 /* [von][deadfeed protector] deadfeed protector info */
#define PSOP_1_1_A17_READ_ADDR 0x18023B04 /* [von][gals] conn_infra_top2conn_gals_rx_debug_out */
#define PSOP_1_1_A18_READ_ADDR 0x1802344C /* [von][timeout protector] ap2conn_timeout_info */
#define PSOP_1_1_A19_READ_ADDR 0x18023450 /* [von][timeout protector] ap2conn_timeout_addr */
#define PSOP_1_1_A20_READ_ADDR 0x18023480 /* [von][timeout protector] ap2h_timeout_info */
#define PSOP_1_1_A21_READ_ADDR 0x18023484 /* [von][timeout protector] ap2h_timeout_addr */
#define PSOP_1_1_A22_READ_ADDR 0x1802341c /* [von][timeout protector] p_d_n11_timeout_info */
#define PSOP_1_1_A23_READ_ADDR 0x18023420 /* [von][timeout protector]  p_d_n11_timeout_addr */
#define PSOP_1_1_A24_READ_ADDR 0x18023428 /* [von][timeout protector] p_d_n11_timeout_wdata */
#define PSOP_1_1_A25_READ_ADDR 0x18023428 /* [von][timeout protector] p_d_n13_timeout_info */
#define PSOP_1_1_A26_READ_ADDR 0x1802342c /* [von][timeout protector] p_d_n13_timeout_addr */
#define PSOP_1_1_A27_READ_ADDR 0x18023430 /* [von][timeout protector] p_d_n13_timeout_wdata */

#define PSOP_1_1_A28_A73_WRITE_ADDR 0x18023408
#define PSOP_1_1_A28_A73_READ_ADDR 0x18023404

#define PSOP_1_1_A28_WRITE_VALUE 0x00000013 /* set conn_infra_off_bus_dbg_group_19 [off][timeout protector] osc_domain_apb_info_latch_0 */
#define PSOP_1_1_A29_WRITE_VALUE 0x00000014 /* set conn_infra_off_bus_dbg_group_20 [off][timeout protector] osc_domain_apb_info_latch_1 */
#define PSOP_1_1_A30_WRITE_VALUE 0x00000015 /* set conn_infra_off_bus_dbg_group_21 [off][timeout protector] osc_domain_apb_info_latch_2 */
#define PSOP_1_1_A31_WRITE_VALUE 0x00000016 /* set conn_infra_off_bus_dbg_group_22 [off][timeout protector] osc_domain_paddr_latch */
#define PSOP_1_1_A32_WRITE_VALUE 0x00000017 /* set conn_infra_off_bus_dbg_group_23 [off][timeout protector] osc_domain_pwdata_latch */
#define PSOP_1_1_A33_WRITE_VALUE 0x00000018 /* set conn_infra_off_bus_dbg_group_24 [off][timeout protector] h_d_n25_hclk_domain_apb_info_latch */
#define PSOP_1_1_A34_WRITE_VALUE 0x00000019 /* set conn_infra_off_bus_dbg_group_25 [off][timeout protector] h_d_n25_hclk_domain_paddr_latch */
#define PSOP_1_1_A35_WRITE_VALUE 0x0000001A /* set conn_infra_off_bus_dbg_group_26 [off][timeout protector] h_d_n25_hclk_domain_pwdata_latch */
#define PSOP_1_1_A36_WRITE_VALUE 0x0000001B /* set conn_infra_off_bus_dbg_group_27 [off][timeout protector] conn_infra_sysram_ahb_info_latch */
#define PSOP_1_1_A37_WRITE_VALUE 0x0000001C /* set conn_infra_off_bus_dbg_group_28 [off][timeout protector] conn_infra_sysram_haddr_latch */
#define PSOP_1_1_A38_WRITE_VALUE 0x0000001D /* set conn_infra_off_bus_dbg_group_29 [off][timeout protector] h_d_n39_ahb_info_latch */
#define PSOP_1_1_A39_WRITE_VALUE 0x0000001E /* set conn_infra_off_bus_dbg_group_30 [off][timeout protector] h_d_n39_haddr_latch */
#define PSOP_1_1_A40_WRITE_VALUE 0x0000001F /* set conn_infra_off_bus_dbg_group_31 [off][timeout protector] conn2ap_ahb_info_latch */
#define PSOP_1_1_A41_WRITE_VALUE 0x00000020 /* set conn_infra_off_bus_dbg_group_32 [off][timeout protector] conn2ap_haddr_latch */
#define PSOP_1_1_A42_WRITE_VALUE 0x00000021 /* set conn_infra_off_bus_dbg_group_33 [off][timeout protector] conn2ap_ar_info_latch */
#define PSOP_1_1_A43_WRITE_VALUE 0x00000022 /* set conn_infra_off_bus_dbg_group_34 [off][timeout protector] conn2ap_aw_info_latch */
#define PSOP_1_1_A44_WRITE_VALUE 0x00000023 /* set conn_infra_off_bus_dbg_group_35 [off][timeout protector] conn2ap_r_info_latch */
#define PSOP_1_1_A45_WRITE_VALUE 0x00000024 /* set conn_infra_off_bus_dbg_group_36 [off][timeout protector] conn2ap_w_info_latch */
#define PSOP_1_1_A46_WRITE_VALUE 0x00000025 /* set conn_infra_off_bus_dbg_group_37 [off][timeout protector] conn2ap_b_info_latch */
#define PSOP_1_1_A47_WRITE_VALUE 0x00000026 /* set conn_infra_off_bus_dbg_group_38 [off][timeout protector] conn2ap_rw_busy_latch */
#define PSOP_1_1_A48_WRITE_VALUE 0x00000027 /* set conn_infra_off_bus_dbg_group_39 [off][timeout protector] conn2ap_idle_latch */
#define PSOP_1_1_A49_WRITE_VALUE 0x00000028 /* set conn_infra_off_bus_dbg_group_40 [off][timeout protector] conn_dma2ap_r_info_latch */
#define PSOP_1_1_A50_WRITE_VALUE 0x00000029 /* set conn_infra_off_bus_dbg_group_41 [off][timeout protector] conn_dma2ap_w_info_latch */
#define PSOP_1_1_A51_WRITE_VALUE 0x0000002A /* set conn_infra_off_bus_dbg_group_42 [off][timeout protector] conn_dma2ap_b_info_latch */
#define PSOP_1_1_A52_WRITE_VALUE 0x0000002B /* set conn_infra_off_bus_dbg_group_43 [off][timeout protector] conn_dma2ap_rw_busy_latch */
#define PSOP_1_1_A53_WRITE_VALUE 0x0000002C /* set conn_infra_off_bus_dbg_group_44 [off][timeout protector] conn_dma2ap_idle_latch_0 */
#define PSOP_1_1_A54_WRITE_VALUE 0x0000002D /* set conn_infra_off_bus_dbg_group_45 [off][timeout protector] conn_dma2ap_idle_latch_1 */
#define PSOP_1_1_A55_WRITE_VALUE 0x00000001 /* set conn_infra_off_bus_dbg_group_01 [off][gals] subsys_0_2conn_gals_rx_gals_axi_dbg_slv */
#define PSOP_1_1_A56_WRITE_VALUE 0x00000002 /* set conn_infra_off_bus_dbg_group_02 [off][gals] conn2subsys_0_gals_tx_debug_out */
#define PSOP_1_1_A57_WRITE_VALUE 0x00000003 /* set conn_infra_off_bus_dbg_group_03 [off][gals] subsys_1_2conn_rx_debug_out */
#define PSOP_1_1_A58_WRITE_VALUE 0x00000004 /* set conn_infra_off_bus_dbg_group_04 [off][gals] conn2subsys_1 gals_tx_debug_ou */
#define PSOP_1_1_A59_WRITE_VALUE 0x00000005 /* set conn_infra_off_bus_dbg_group_05 [off][gals] subsys_1_2conn_gals_rx_debug_out */
#define PSOP_1_1_A60_WRITE_VALUE 0x00000006 /* set conn_infra_off_bus_dbg_group_06 [off][gals] conn2subsys_1_gals_tx_debug_out */
#define PSOP_1_1_A61_WRITE_VALUE 0x00000007 /* set conn_infra_off_bus_dbg_group_07 [off][gals] conn2ap_gals_tx_gals_axi_dbg_mst */
#define PSOP_1_1_A62_WRITE_VALUE 0x00000008 /* set conn_infra_off_bus_dbg_group_08 [off][gals] bt_icap2conn_gals_rx_gals_rx_debug_out */
#define PSOP_1_1_A63_WRITE_VALUE 0x00000009 /* set conn_infra_off_bus_dbg_group_09 [off][gals] wf_icap2conn_gals_rx_gals_axi_dbg_slv */
#define PSOP_1_1_A64_WRITE_VALUE 0x0000000A /* set conn_infra_off_bus_dbg_group_10 [off][gals] wf_dma2conn_gals_rx_gals_axi_dbg_slv */
#define PSOP_1_1_A65_WRITE_VALUE 0x0000000B /* set conn_infra_off_bus_dbg_group_11 [off][gals] conn_dma2ap_tx_gals_tx_axi_dbg_mst */
#define PSOP_1_1_A66_WRITE_VALUE 0x0000000C /* set conn_infra_off_bus_dbg_group_12 [off][idle] conn_infra_bus_idle_0 */
#define PSOP_1_1_A67_WRITE_VALUE 0x0000000D /* set conn_infra_off_bus_dbg_group_13 [off][idle] conn_infra_bus_idle_1 */
#define PSOP_1_1_A68_WRITE_VALUE 0x0000000E /* set conn_infra_off_bus_dbg_group_14 [off][idle] conn_infra_bus_idle_2 */
#define PSOP_1_1_A69_WRITE_VALUE 0x0000000F /* set conn_infra_off_bus_dbg_group_15 [off][idle] conn_infra_bus_idle_3 */
#define PSOP_1_1_A70_WRITE_VALUE 0x00000010 /* set conn_infra_off_bus_dbg_group_16 [off][idle] conn_infra_axi_layer_bus_idel_0 */
#define PSOP_1_1_A71_WRITE_VALUE 0x00000011 /* set conn_infra_off_bus_dbg_group_17 [off][idle] conn_infra_axi_layer_bus_idel_1)  & read debug info. */
#define PSOP_1_1_A72_WRITE_VALUE 0x00000012 /* set conn_infra_off_bus_dbg_group_18 [off][idle] conn_infra_wakeup_idle)  & read debug info. */
#define PSOP_1_1_A73_WRITE_VALUE 0x0000002E /* set conn_infra_off_bus_dbg_group_46 [off][slpprot state] off bus node slpprot en/rdy */

#define PSOP_1_1_A74_READ_ADDR 0x18020074 /* von_bcrm_p_d_n10_to_apb2h_s_async_ctrl_0 */
#define PSOP_1_1_A75_READ_ADDR 0x18020078 /* von_bcrm_p_a_n9_ctrl_0 */
#define PSOP_1_1_A76_READ_ADDR 0x1802007C /* von_bcrm_p_d_n10_ctrl_0 */
#define PSOP_1_1_A77_READ_ADDR 0x18020084 /* von_bcrm_p_d_n11_ctrl_0 */
#define PSOP_1_1_A78_READ_ADDR 0x18020088 /* von_bcrm_p_d_n12_to_p_a_n9_async_ctrl */
#define PSOP_1_1_A79_READ_ADDR 0x1802008C /* von_bcrm_p2p_to_conn_von_osc_ctrl */
#define PSOP_1_1_A80_READ_ADDR 0x18023454 /* connsys2host_timeout_info */
#define PSOP_1_1_A81_READ_ADDR 0x18023458 /* connsys_timeout_addr */
#define PSOP_1_1_A82_READ_ADDR 0x18020080 /* von_bcrm_p_d_n10_ctrl_0 */

/* SectionB - IF CONN_INFRA OFF read check ok - bus */
#define PSOP_1_1_B01_READ_ADDR 0x1804F280 /* [off][access protector] subsys_0_haddr_latch */
#define PSOP_1_1_B02_READ_ADDR 0x1804F284 /* [off][access protector] subsys_1_haddr_latch */
#define PSOP_1_1_B03_READ_ADDR 0x1804F288 /* [off][access protector] subsys_2_haddr_latch */
#define PSOP_1_1_B04_READ_ADDR 0x1804F28C /* [off][access protector] ap2conn_haddr_latch */
#define PSOP_1_1_B05_READ_ADDR 0x1804F290 /* [off][access protector] cmdbt0_haddr_latch */
#define PSOP_1_1_B06_READ_ADDR 0x1804F294 /* [off][access protector] cmdbt1_haddr_latch */
#define PSOP_1_1_B07_READ_ADDR 0x1804F298 /* [off][access protector] conn_met_haddr_latch */
#define PSOP_1_1_B08_READ_ADDR 0x1804F29C /* [off][access protector] coex_icer_haddr_latch */
#define PSOP_1_1_B09_READ_ADDR 0x1804F2A0 /* [off][access protector] *_security_irq_b */

#define PSOP_1_1_B10_READ_ADDR 0x18049408 /* [off][debug_ctrl_ao] real time group1_0 (snapshot) */
#define PSOP_1_1_B11_READ_ADDR 0x1804940C /* [off][debug_ctrl_ao] real time group1_1 (snapshot) */
#define PSOP_1_1_B12_READ_ADDR 0x18049410 /* [off][debug_ctrl_ao] real time group1_2 (snapshot) */
#define PSOP_1_1_B13_READ_ADDR 0x18049414 /* [off][debug_ctrl_ao] real time group2_0 (snapshot) */
#define PSOP_1_1_B14_READ_ADDR 0x18049418 /* [off][debug_ctrl_ao] real time group3_0 (snapshot) */
#define PSOP_1_1_B15_READ_ADDR 0x1804941C /* [off][debug_ctrl_ao] real time group3_1 (snapshot) */
#define PSOP_1_1_B16_READ_ADDR 0x18049420 /* [off][debug_ctrl_ao] real time group3_2 (snapshot) */
#define PSOP_1_1_B17_READ_ADDR 0x18049424 /* [off][debug_ctrl_ao] real time group3_3 (snapshot) */
#define PSOP_1_1_B18_READ_ADDR 0x18049428 /* [off][debug_ctrl_ao] real time group1_0 (snapshot) */
#define PSOP_1_1_B19_READ_ADDR 0x1804942C /* [off][debug_ctrl_ao] real time group4_1 (snapshot) */
#define PSOP_1_1_B20_READ_ADDR 0x18049430 /* [off][debug_ctrl_ao] real time group4_2 (snapshot) */

#define PSOP_1_1_B21_READ_ADDR 0x1804F650 /* [off] conn2ap remapping setting */

#define PSOP_1_1_B22_READ_ADDR 0x1804F700 /* [off] low power layer ctrl */

#define PSOP_1_1_B23_READ_ADDR 0x18060D04 /* [on] conn2ap remapping cr_remap_mcu0_emi_base_addr */
#define PSOP_1_1_B24_READ_ADDR 0x18060D08 /* [on] conn2ap remapping cr_remap_md_share_base_addr */
#define PSOP_1_1_B25_READ_ADDR 0x18060D0C /* [on] conn2ap remapping cr_remap_gps_emi_base_addr */

#define PSOP_1_1_B26_READ_ADDR 0x1804F610 /* [off] conn2ap remapping cr_scpsys_rma_base_addr */
#define PSOP_1_1_B27_READ_ADDR 0x1804F614 /* [off] conn2ap remapping cr_peri_wf_base_addr */
#define PSOP_1_1_B28_READ_ADDR 0x1804F618 /* [off] conn2ap remapping cr_peri_bt_base_addr */
#define PSOP_1_1_B29_READ_ADDR 0x1804F61C /* [off] conn2ap remapping cr_peri_gps_base_addr */

#define PSOP_1_1_B30_READ_ADDR 0x18049000 /* [off][debug_ctrl_ao] PWR_CTRL_0 */
#define PSOP_1_1_B31_READ_ADDR 0x18049004 /* [off][debug_ctrl_ao] PWR_CTRL_1 */
#define PSOP_1_1_B32_READ_ADDR 0x18049008 /* [off][debug_ctrl_ao] PWR_CTRL_2 */
#define PSOP_1_1_B33_READ_ADDR 0x1804900C /* [off][debug_ctrl_ao] PWR_BUSY1 */

#define PSOP_1_1_B34_READ_ADDR 0x18049010 /* [off][debug_ctrl_ao] PWR_IDLE1 */
#define PSOP_1_1_B35_READ_ADDR 0x18049014 /* [off][debug_ctrl_ao] PWR_IDLE2 */

#define PSOP_1_1_B36_READ_ADDR 0x1800E128 /* [on][timeout protector] p_d_n14_timeout_info */
#define PSOP_1_1_B37_READ_ADDR 0x1800E12C /* [on][timeout protector] p_d_n14_timeout_info */
#define PSOP_1_1_B38_READ_ADDR 0x1800E130 /* [on][timeout protector] p_d_n14_timeout_addr */
#define PSOP_1_1_B39_READ_ADDR 0x1800E134 /* [on][timeout protector] p_d_n14_timeout_wdata */
#define PSOP_1_1_B40_READ_ADDR 0x1800E138 /* [on][timeout protector] p_d_n16_timeout_info */
#define PSOP_1_1_B41_READ_ADDR 0x1800E13C /* [on][timeout protector] p_d_n16_timeout_addr */
#define PSOP_1_1_B42_READ_ADDR 0x1800E140 /* [on][timeout protector] p_d_n16_timeout_wdata */
#define PSOP_1_1_B43_READ_ADDR 0x1800E110 /* [on][timeout protector] timeout clear */

/* SectionC - IF CONN_INFRA OFF read check ok - slpprot */
#define PSOP_1_1_C01_READ_ADDR 0x18011404 /* conn2ap_slp_status */
#define PSOP_1_1_C02_READ_ADDR 0x18011408 /* conn2ap_emi_slp_status */
#define PSOP_1_1_C03_READ_ADDR 0x18011434 /* conn_infra_off_bus_slp_status */
#define PSOP_1_1_C04_READ_ADDR 0x180114A4 /* wf_to_conn_slp_status */
#define PSOP_1_1_C05_READ_ADDR 0x18011454 /* conn_to_bt0_slp_status */
#define PSOP_1_1_C06_READ_ADDR 0x18011464 /* bt0_to_conn_slp_status */
#define PSOP_1_1_C07_READ_ADDR 0x18011474 /* conn_to_bt1_slp_status */
#define PSOP_1_1_C08_READ_ADDR 0x18011484 /* bt1_to_conn_slp_status */
#define PSOP_1_1_C09_READ_ADDR 0x180114D4 /* bticap_to_conn status */
#define PSOP_1_1_C10_READ_ADDR 0x18001414 /* ap_to_conn status */
#define PSOP_1_1_C11_READ_ADDR 0x18001424 /* on_to_off status */
#define PSOP_1_1_C12_READ_ADDR 0x18001434 /* bt_to_conn_host status */
#define PSOP_1_1_C13_READ_ADDR 0x18001454 /* wf_to_conn_host status */
#define PSOP_1_1_C14_READ_ADDR 0x70028730 /*  */

/******************************************/
/*                                        */
/* 1_[Griffin] conn_infra_bus_debug.xlsx  */
/* - sheet 2_slpprot status (rdy)         */
/*                                        */
/******************************************/
#define PSOP_1_2_A01_READ_ADDR 0x18011404
#define PSOP_1_2_A02_READ_ADDR 0x18011408
#define PSOP_1_2_A03_READ_ADDR 0x18011434
#define PSOP_1_2_A04_READ_ADDR 0x180114A4
#define PSOP_1_2_A05_READ_ADDR 0x18011454
#define PSOP_1_2_A06_READ_ADDR 0x18011464
#define PSOP_1_2_A07_READ_ADDR 0x18011474
#define PSOP_1_2_A08_READ_ADDR 0x18011484
#define PSOP_1_2_A09_READ_ADDR 0x180114D4
#define PSOP_1_2_A10_READ_ADDR 0x18011414
#define PSOP_1_2_A11_READ_ADDR 0x18011424
#define PSOP_1_2_A12_READ_ADDR 0x18011434
#define PSOP_1_2_A13_READ_ADDR 0x18011454
#define PSOP_1_2_A14_READ_ADDR 0x70028730

/*****************************************/
/*                                       */
/* 2_[Griffin] connsys_power_debug.xlsx  */
/* - sheet 1_dump_list                   */
/*                                       */
/*****************************************/
/* Dump after conn_infra_on is ready */
#define PSOP_2_1_B01_READ_ADDR 0x18060A10 /* MTCMOS STATUS */
#define PSOP_2_1_B02_READ_ADDR 0x18060014 /* [fw/driver own]] WF_AP band-0 (FW --> AP interrupt is target) */
#define PSOP_2_1_B03_READ_ADDR 0x18060054 /* [fw/driver own] WF_MD (FW --> AP interrupt is target) */
#define PSOP_2_1_B04_READ_ADDR 0x18060010 /* [fw/driver own] WF_AP band-0 driver/fw own status */
#define PSOP_2_1_B05_READ_ADDR 0x18060050 /* [fw/driver own] WF_MD driver/fw own status */
#define PSOP_2_1_B06_READ_ADDR 0x18060018 /* [fw/driver own] WF_AP band-0 IRQ_EN (to host side) */
#define PSOP_2_1_B07_READ_ADDR 0x18060058 /* [fw/driver own] WF_MD IRQ_EN (to host side) */

/* Dump after conn_infra_off is ready */
#define PSOP_2_1_C01_READ_ADDR 0x18011030 /* PLL Staus */
#define PSOP_2_1_C02_READ_ADDR 0x18012050 /* CONN_INFRA_OFF BUS CK */
#define PSOP_2_1_C03_READ_ADDR 0x18001344 /* OSC */
#define PSOP_2_1_C04_READ_ADDR 0x18000400 /* WF ON MTCMOS CTRL */
#define PSOP_2_1_C05_READ_ADDR 0x18000404 /* BGF ON MTCMOS CTRL */
#define PSOP_2_1_C06_READ_ADDR 0x18095204 /* debug conn_wt_slp issue */
#define PSOP_2_1_C07_READ_ADDR 0x180910A8 /* conn_wt_slp_status */
#define PSOP_2_1_C08_READ_ADDR 0x18091120 /* conn_wt_slp_status */
#define PSOP_2_1_C09_READ_ADDR 0x18091124 /* conn_wt_slp_status */
#define PSOP_2_1_C10_READ_ADDR 0x18091128 /* conn_wt_slp_status */
#define PSOP_2_1_C11_READ_ADDR 0x1809112C /* conn_wt_slp_status */
#define PSOP_2_1_C12_READ_ADDR 0x18091130 /* conn_wt_slp_status */
#define PSOP_2_1_C13_READ_ADDR 0x18091134 /* conn_wt_slp_status */

#define PSOP_2_1_C14_C17_DBG_WRITE_ADDR 0x18011100 /* 0x18011100[22:21] */
#define PSOP_2_1_C14_C17_DBG_WRITE_MASK ~(BITS(21, 22)) /* 0x18011100[22:21] */
#define PSOP_2_1_C14_WRITE_VALUE (0x0 << 21)
#define PSOP_2_1_C15_WRITE_VALUE (0x1 << 21)
#define PSOP_2_1_C16_WRITE_VALUE (0x2 << 21)
#define PSOP_2_1_C17_WRITE_VALUE (0x3 << 21)
#define PSOP_2_1_C14_C17_DBG_READ_ADDR   0x18011134 /* cb_infra_slp_ctrl (dbg) */

#define PSOP_2_1_C18_READ_ADDR 0x18098000 /* CONN_TOP_SPI_WR/RD CR */
#define PSOP_2_1_C19_READ_ADDR 0x18098050 /* CONN_TOP_SPI_WR/RD CR */
#define PSOP_2_1_C20_READ_ADDR 0x18098054 /* CONN_TOP_SPI_WR/RD CR */
#define PSOP_2_1_C21_READ_ADDR 0x18098058 /* CONN_TOP_SPI_WR/RD CR */

#define PSOP_2_1_C22_READ_ADDR 0x18098108 /* SPI_hs_en ctrl status */
#define PSOP_2_1_C23_READ_ADDR 0x18098004 /* SPI_hs_en ctrl status */

#define PSOP_2_1_C24_READ_ADDR 0x18001620 /* [fw/driver own] WF_AP band-0 (AP-->FW interrupt is target) */
#define PSOP_2_1_C25_READ_ADDR 0x18001610 /* [fw/driver own] WF_MD (AP-->FW interrupt is target) */
#define PSOP_2_1_C26_READ_ADDR 0x18001600 /* [fw/driver own] IRQ_EN (to fw) */

/* A Die part (ATOP) - common */
#define PSOP_2_1_D00_READ_ADDR 0xA10 /* a die related status */
#define PSOP_2_1_D01_READ_ADDR 0x090 /* a die related status */
#define PSOP_2_1_D02_READ_ADDR 0x08C /* a die related status */
#define PSOP_2_1_D03_READ_ADDR 0x0A0 /* a die related status */
#define PSOP_2_1_D04_READ_ADDR 0x09C /* a die related status */
#define PSOP_2_1_D05_READ_ADDR 0xA40 /* a die related status */
#define PSOP_2_1_D06_READ_ADDR 0xA48 /* a die related status */
#define PSOP_2_1_D07_READ_ADDR 0x8E0 /* a die related status */
#define PSOP_2_1_D08_READ_ADDR 0x094 /* a die related status */
#define PSOP_2_1_D09_READ_ADDR 0x5B4 /* a die related status */
#define PSOP_2_1_D10_READ_ADDR 0x2CC /* a die related status */
#define PSOP_2_1_D11_READ_ADDR 0x84C /* a die related status */
#define PSOP_2_1_D12_READ_ADDR 0x860 /* a die related status */
#define PSOP_2_1_D13_READ_ADDR 0x861 /* a die related status */
#define PSOP_2_1_D14_READ_ADDR 0xA70 /* a die related status */
#define PSOP_2_1_D15_READ_ADDR 0xA20 /* a die related status */
#define PSOP_2_1_D16_READ_ADDR 0x850 /* a die related status */
#define PSOP_2_1_D17_READ_ADDR 0x85D /* a die related status */
#define PSOP_2_1_D18_READ_ADDR 0xC08 /* a die related status */

/* A Die part (ATOP) - MT797x */
#define PSOP_2_1_E00_READ_ADDR 0x02C /* a die related status */
#define PSOP_2_1_E01_READ_ADDR 0x000 /* a die related status */
#define PSOP_2_1_E02_READ_ADDR 0xB08 /* a die related status */
#define PSOP_2_1_E03_READ_ADDR 0x0B4 /* a die related status */
#define PSOP_2_1_E04_READ_ADDR 0x580 /* a die related status */
#define PSOP_2_1_E05_READ_ADDR 0xA70 /* a die related status */
#define PSOP_2_1_E06_READ_ADDR 0xA20 /* a die related status */


/*************************************/
/*                                   */
/* 3_[Griffin] WF_TOP_debug.xlsx     */
/* - sheet 1_dump_list               */
/*                                   */
/*************************************/
/* Section A: Dump wf_top_misc_on monflg */
#define PSOP_3_1_A00_WRITE_ADDR   0x18060B00
#define PSOP_3_1_A00_WRITE_VALUE  0x00000001
/* A01 ~ A13 */
#define PSOP_3_1_A01_A13_DBG_WRITE_ADDR 0x18060B04
#define PSOP_3_1_A01_A13_DBG_READ_ADDR   0x18060B10

#define PSOP_3_1_A01_WRITE_VALUE 0x00000000
#define PSOP_3_1_A02_WRITE_VALUE 0x00000001
#define PSOP_3_1_A03_WRITE_VALUE 0x00000002
#define PSOP_3_1_A04_WRITE_VALUE 0x00000003
#define PSOP_3_1_A05_WRITE_VALUE 0x00000004
#define PSOP_3_1_A06_WRITE_VALUE 0x00000010
#define PSOP_3_1_A07_WRITE_VALUE 0x00000012
#define PSOP_3_1_A08_WRITE_VALUE 0x00000017
#define PSOP_3_1_A09_WRITE_VALUE 0x00000018
#define PSOP_3_1_A10_WRITE_VALUE 0x00000019
#define PSOP_3_1_A11_WRITE_VALUE 0x0000001A
#define PSOP_3_1_A12_WRITE_VALUE 0x0000001B
#define PSOP_3_1_A13_WRITE_VALUE 0x0000001D

/* Section B: Dump wf_top_misc_von monflg */
#define PSOP_3_1_B00_WRITE_ADDR  0x18060B18 /* monflg enable */
#define PSOP_3_1_B00_WRITE_VALUE 0x00000001 /* monflg enable */
/* B00 ~ B21 */
#define PSOP_3_1_B01_B11_DBG_WRITE_ADDR 0x18060B1C
#define PSOP_3_1_B01_B11_DBG_READ_ADDR   0x18023638

#define PSOP_3_1_B01_WRITE_VALUE 0x00000000 /* monflg #0 */
#define PSOP_3_1_B02_WRITE_VALUE 0x00000001 /* monflg #1 */
#define PSOP_3_1_B03_WRITE_VALUE 0x00000002 /* monflg #2 */
#define PSOP_3_1_B04_WRITE_VALUE 0x00000003 /* monflg #3 */
#define PSOP_3_1_B05_WRITE_VALUE 0x00000004 /* monflg #4 */
#define PSOP_3_1_B06_WRITE_VALUE 0x00000008 /* monflg #8 */
#define PSOP_3_1_B07_WRITE_VALUE 0x0000000A /* monflg #10 */
#define PSOP_3_1_B08_WRITE_VALUE 0x0000000B /* monflg #11 */
#define PSOP_3_1_B09_WRITE_VALUE 0x0000000C /* monflg #12 */
#define PSOP_3_1_B10_WRITE_VALUE 0x0000000D /* monflg #13 */
#define PSOP_3_1_B11_WRITE_VALUE 0x0000000E /* monflg #14 */

#define PSOP_3_1_B12_READ_ADDR 0x180B0014 /* conn_infra_cmdbt BK start 1 */
#define PSOP_3_1_B13_READ_ADDR 0x180B0018 /* conn_infra_cmdbt BK start 2 */
#define PSOP_3_1_B14_READ_ADDR 0x180B001C /* conn_infra_cmdbt BK end  1 */
#define PSOP_3_1_B15_READ_ADDR 0x180B0020 /* conn_infra_cmdbt BK end 2 */
#define PSOP_3_1_B16_READ_ADDR 0x180B0024 /* conn_infra_cmdbt COMMON RS start 1 */
#define PSOP_3_1_B17_READ_ADDR 0x180B0028 /* conn_infra_cmdbt COMMON RS start 2 */
#define PSOP_3_1_B18_READ_ADDR 0x180B002C /* conn_infra_cmdbt COMMON RS end  1 */
#define PSOP_3_1_B19_READ_ADDR 0x180B0030 /* conn_infra_cmdbt COMMON RS end 2 */
#define PSOP_3_1_B20_READ_ADDR 0x180B0034 /* conn_infra_cmdbt VON RS end */
#define PSOP_3_1_B21_READ_ADDR 0x180B0038 /* conn_infra_cmdbt OFF RS end */

/* Section C: Dump wf_top_cfg_on debug CR */
/* C01 ~ C34 */
#define PSOP_3_1_C01_READ_ADDR 0x184C1B00
#define PSOP_3_1_C02_READ_ADDR 0x184C1B04
#define PSOP_3_1_C03_READ_ADDR 0x184C1B08
#define PSOP_3_1_C04_READ_ADDR 0x184C1B0C
#define PSOP_3_1_C05_READ_ADDR 0x184C1B10
#define PSOP_3_1_C06_READ_ADDR 0x184C1B14
#define PSOP_3_1_C07_READ_ADDR 0x184C1B18
#define PSOP_3_1_C08_READ_ADDR 0x184C1B1C
#define PSOP_3_1_C09_READ_ADDR 0x184C1B20
#define PSOP_3_1_C10_READ_ADDR 0x184C1B24
#define PSOP_3_1_C11_READ_ADDR 0x184C1B28
#define PSOP_3_1_C12_READ_ADDR 0x184C1B2C
#define PSOP_3_1_C13_READ_ADDR 0x184C1B30
#define PSOP_3_1_C14_READ_ADDR 0x184C1B34
#define PSOP_3_1_C15_READ_ADDR 0x184C1B38
#define PSOP_3_1_C16_READ_ADDR 0x184C1B3C
#define PSOP_3_1_C17_READ_ADDR 0x184C1B40
#define PSOP_3_1_C18_READ_ADDR 0x184C1B50
#define PSOP_3_1_C19_READ_ADDR 0x184C1B54
#define PSOP_3_1_C20_READ_ADDR 0x184C1B58
#define PSOP_3_1_C21_READ_ADDR 0x184C1B5C
#define PSOP_3_1_C22_READ_ADDR 0x184C1B60
#define PSOP_3_1_C23_READ_ADDR 0x184C1B64
#define PSOP_3_1_C24_READ_ADDR 0x184C1B68
#define PSOP_3_1_C25_READ_ADDR 0x184C1B6C
#define PSOP_3_1_C26_READ_ADDR 0x184C1B70
#define PSOP_3_1_C27_READ_ADDR 0x184C1B74
#define PSOP_3_1_C28_READ_ADDR 0x184C1B78
#define PSOP_3_1_C29_READ_ADDR 0x184C1B7C
#define PSOP_3_1_C30_READ_ADDR 0x184C1B80
#define PSOP_3_1_C31_READ_ADDR 0x184C1B84
#define PSOP_3_1_C32_READ_ADDR 0x184C1B88
#define PSOP_3_1_C33_READ_ADDR 0x184C1B8C
#define PSOP_3_1_C34_READ_ADDR 0x184C1B90

/* Not in wiki flow */
/* Section D: Dump wf_top_rgu_on debug CR */
/* Section E: Dump wf_top_rgu_von monflg */
/* Section F: Dump wf_top_slpprot_on debug CR */

/***********************************/
/*                                 */
/* 4_[Griffin] WF_BUS_debug.xlsx   */
/* - sheet 1_dump_list             */
/*                                 */
/***********************************/
/* Section A: Dump VDNR timeout host side info. */
/* A00 ~ A19 */
#define PSOP_4_1_A00_READ_ADDR   	0x1802362C
#define PSOP_4_1_A00_WRITE_ADDR   0x18023604
#define PSOP_4_1_A00_WRITE_VALUE  0x4

#define PSOP_4_1_A01_A19_DBG_WRITE_ADDR 0x18023628
#define PSOP_4_1_A01_A19_DBG_READ_ADDR 0x18023608

#define PSOP_4_1_A01_WRITE_VALUE 0x00010001
#define PSOP_4_1_A02_WRITE_VALUE 0x00020001
#define PSOP_4_1_A03_WRITE_VALUE 0x00030001
#define PSOP_4_1_A04_WRITE_VALUE 0x00010002
#define PSOP_4_1_A05_WRITE_VALUE 0x00020002
#define PSOP_4_1_A06_WRITE_VALUE 0x00030002
#define PSOP_4_1_A07_WRITE_VALUE 0x00040002
#define PSOP_4_1_A08_WRITE_VALUE 0x00050002
#define PSOP_4_1_A09_WRITE_VALUE 0x00060002
#define PSOP_4_1_A10_WRITE_VALUE 0x00070002
#define PSOP_4_1_A11_WRITE_VALUE 0x00080002
#define PSOP_4_1_A12_WRITE_VALUE 0x00090002
#define PSOP_4_1_A13_WRITE_VALUE 0x000A0002
#define PSOP_4_1_A14_WRITE_VALUE 0x00010003
#define PSOP_4_1_A15_WRITE_VALUE 0x00020003
#define PSOP_4_1_A16_WRITE_VALUE 0x00030003
#define PSOP_4_1_A17_WRITE_VALUE 0x00040003
#define PSOP_4_1_A18_WRITE_VALUE 0x00050003
#define PSOP_4_1_A19_WRITE_VALUE 0x00060003

/* Section B: Dump VDNR timeout wf side info */
/* B00 ~ B19 */
#define PSOP_4_1_B00_WRITE_ADDR	0x18400120
#define PSOP_4_1_B00_WRITE_VALUE	0x810F0000
#define PSOP_4_1_B00_READ_ADDR 	0x18500408

#define PSOP_4_1_B01_READ_ADDR 0x1850040C
#define PSOP_4_1_B02_READ_ADDR 0x18500410
#define PSOP_4_1_B03_READ_ADDR 0x18500414
#define PSOP_4_1_B04_READ_ADDR 0x18500418
#define PSOP_4_1_B05_READ_ADDR 0x1850041C
#define PSOP_4_1_B06_READ_ADDR 0x18500420
#define PSOP_4_1_B07_READ_ADDR 0x18500424
#define PSOP_4_1_B08_READ_ADDR 0x18500428
#define PSOP_4_1_B09_READ_ADDR 0x1850042C
#define PSOP_4_1_B10_READ_ADDR 0x18500430
#define PSOP_4_1_B11_READ_ADDR 0x18500434
#define PSOP_4_1_B12_READ_ADDR 0x18500438
#define PSOP_4_1_B13_READ_ADDR 0x1850043C
#define PSOP_4_1_B14_READ_ADDR 0x18500440
#define PSOP_4_1_B15_READ_ADDR 0x18500444
#define PSOP_4_1_B16_READ_ADDR 0x18500448
#define PSOP_4_1_B17_READ_ADDR 0x1850044C
#define PSOP_4_1_B18_READ_ADDR 0x18500450
#define PSOP_4_1_B19_READ_ADDR 0x18500000

/* Section C: Dump AHB APB timeout info */
/* C00 ~ C04 */
#define PSOP_4_1_C00_WRITE_ADDR	0x18400120
#define PSOP_4_1_C00_WRITE_VALUE	0x830C0000
#define PSOP_4_1_C00_READ_ADDR 	0x18501004

#define PSOP_4_1_C01_READ_ADDR 0x18501010
#define PSOP_4_1_C02_READ_ADDR 0x18501008
#define PSOP_4_1_C03_READ_ADDR 0x1850100C
#define PSOP_4_1_C04_READ_ADDR 0x18501000

/***************************************************/
/*                                                 */
/* 7_[Griffin]conn_infra_top_debug_related_cr.xlsx */
/* - sheet dump_list                               */
/*                                                 */
/***************************************************/
/* Dump after conn_infra_on is ready */
#define PSOP_7_1_A01_READ_ADDR 0x18060A00 /* conn_infra_systrap */
#define PSOP_7_1_A02_READ_ADDR 0x18060A0C /*conn_infra_clkgen_on */
#define PSOP_7_1_A03_A10_DBG_WRITE_ADDR 0x1806015C /* 0x1806_015c[2:0] */
#define PSOP_7_1_A03_A10_DBG_WRITE_MASK ~(BITS(0, 2)) /* 0x1806_015c[2:0] */

#define PSOP_7_1_A03_WRITE_VALUE (0x0 << 0)
#define PSOP_7_1_A04_WRITE_VALUE (0x1 << 0)
#define PSOP_7_1_A05_WRITE_VALUE (0x2 << 0)
#define PSOP_7_1_A06_WRITE_VALUE (0x3 << 0)
#define PSOP_7_1_A07_WRITE_VALUE (0x4 << 0)
#define PSOP_7_1_A08_WRITE_VALUE (0x5 << 0)
#define PSOP_7_1_A09_WRITE_VALUE (0x6 << 0)
#define PSOP_7_1_A10_WRITE_VALUE (0x7 << 0) /* conn_infra_cfg_on dbg_sel: end */

#define PSOP_7_1_A03_A10_DBG_READ_ADDR 0x18060A04 /* 0x1806_0a04 */

#define PSOP_7_1_A11_A20_DBG_WRITE_ADDR 0x18060160 /* 0x1806_0160[3:0] */
#define PSOP_7_1_A11_A20_DBG_WRITE_MASK ~(BITS(0, 3)) /*0x1806_0160[3:0] */

#define PSOP_7_1_A11_WRITE_VALUE (0x0 << 0)
#define PSOP_7_1_A12_WRITE_VALUE (0x1 << 0)
#define PSOP_7_1_A13_WRITE_VALUE (0x2 << 0)
#define PSOP_7_1_A14_WRITE_VALUE (0x3 << 0)
#define PSOP_7_1_A15_WRITE_VALUE (0x4 << 0)
#define PSOP_7_1_A16_WRITE_VALUE (0x5 << 0)
#define PSOP_7_1_A17_WRITE_VALUE (0x6 << 0)
#define PSOP_7_1_A18_WRITE_VALUE (0x7 << 0)
#define PSOP_7_1_A19_WRITE_VALUE (0x8 << 0)
#define PSOP_7_1_A20_WRITE_VALUE (0x9 << 0) /* conn_infra_rgu_on dbg_sel: end */

#define PSOP_7_1_A11_A20_DBG_READ_ADDR 0x18060A08 /* 0x1806_0a08 */

#define PSOP_7_1_A21_READ_ADDR 0x18023200 /* clock detect */

/* Dump after conn_infra_off is ready */
#define PSOP_7_1_B01_READ_ADDR 0x18011130 /* conn_infra_cfg emi prob */

#define PSOP_7_1_B02_B05_DBG_WRITE_ADDR 0x18011100 /* 0x1801_1100[22:21] */
#define PSOP_7_1_B02_B05_DBG_WRITE_MASK ~(BITS(21, 22)) /* 0x1801_1100[22:21] */

#define PSOP_7_1_B02_WRITE_VALUE (0x0 << 21)
#define PSOP_7_1_B03_WRITE_VALUE (0x1 << 21)
#define PSOP_7_1_B04_WRITE_VALUE (0x2 << 21)
#define PSOP_7_1_B05_WRITE_VALUE (0x3 << 21)
#define PSOP_7_1_B02_B05_DBG_READ_ADDR 0x18011134 /* 0x1801_1134 */

/* B06 Need  WRITE 10 STEPs then READ one */
/* conn_infra_clkgen */
#define PSOP_7_1_B06_DBG_WRITE_ADDR1_ADDR2 0x1801603C /* 0x1801_603C */
#define PSOP_7_1_B06_DBG_WRITE_MASK1 ~(BITS(0, 5)) /* 0x1801_603C[5:0] */
#define PSOP_7_1_B06_DBG_WRITE_VALUE1 (0xE << 0) /* 6'hE */

#define PSOP_7_1_B06_DBG_WRITE_MASK2 ~(BITS(8, 13)) /* 0x1801_603C[13:8] */
#define PSOP_7_1_B06_DBG_WRITE_VALUE2  (0x11 << 8) /* 6'h11 */

#define PSOP_7_1_B06_DBG_WRITE_ADDR3 0x1801601C /* 0x1801_601C */
#define PSOP_7_1_B06_DBG_WRITE_VALUE3 0x03020100 /* 0x0302_0100 */

#define PSOP_7_1_B06_DBG_WRITE_ADDR4 0x18016020 /* 0x1801_6020 */
#define PSOP_7_1_B06_DBG_WRITE_VALUE4  0x07060504 /* 0x0706_0504 */

#define PSOP_7_1_B06_DBG_WRITE_ADDR5 0x18016024 /* 0x1801_6024 */
#define PSOP_7_1_B06_DBG_WRITE_VALUE5 0x0B0A0908 /* 0x0B0A_0908 */

#define PSOP_7_1_B06_DBG_WRITE_ADDR6 0x18016028 /* 0x1801_6028 */
#define PSOP_7_1_B06_DBG_WRITE_VALUE6 0x0F0E0D0C /* 0x0F0E_0D0C */

#define PSOP_7_1_B06_DBG_WRITE_ADDR7 0x1801602C /* 0x1801_602C */
#define PSOP_7_1_B06_DBG_WRITE_VALUE7  0x13121110 /* 0x1312_1110 */

#define PSOP_7_1_B06_DBG_WRITE_ADDR8 0x18016030 /* 0x1801_6030 */
#define PSOP_7_1_B06_DBG_WRITE_VALUE8  0x17161514 /* 0x1716_1514 */

#define PSOP_7_1_B06_DBG_WRITE_ADDR9 0x18016034 /* 0x1801_6034 */
#define PSOP_7_1_B06_DBG_WRITE_VALUE9  0x1B1A1918 /* 0x1B1A_1918 */

#define PSOP_7_1_B06_DBG_WRITE_ADDR10 0x18016038 /* 0x1801_6038 */
#define PSOP_7_1_B06_DBG_WRITE_VALUE10 0x1F1E1D1C /* 0x1F1E_1D1C */

#define PSOP_7_1_B06_DBG_WRITE_ADDR 0x18016058 /* 0x1801_6058[1] */
#define PSOP_7_1_B06_DBG_WRITE_MASK ~(BIT(1)) /* 0x1801_6058[1] */
#define PSOP_7_1_B06_DBG_WRITE_VALUE  (0x1 << 1) /* 1'b1 */
#define PSOP_7_1_B06_DBG_READ_ADDR 0x18023200 /* 0x1802_3200 */

/* B07 Need  WRITE 10 STEPs then READ one */
/* conn_infra_rgu */
#define PSOP_7_1_B07_DBG_WRITE_ADDR1_ADDR2 0x1801603C /* 0x1801_603C */
#define PSOP_7_1_B07_DBG_WRITE_MASK1 ~(BITS(0, 5)) /* 0x1801_603C[5:0] */
#define PSOP_7_1_B07_DBG_WRITE_VALUE1 (0x18 << 0) /* 6'h18 */

#define PSOP_7_1_B07_DBG_WRITE_MASK2 ~(BITS(8, 13)) /* 0x1801_603C[13:8] */
#define PSOP_7_1_B07_DBG_WRITE_VALUE2  (0x0 << 8) /* 6'h0 */

#define PSOP_7_1_B07_DBG_WRITE_ADDR3 0x1801601C /* 0x1801_601C */
#define PSOP_7_1_B07_DBG_WRITE_VALUE3 0x03020100 /* 0x0302_0100 */

#define PSOP_7_1_B07_DBG_WRITE_ADDR4 0x18016020 /* 0x1801_6020 */
#define PSOP_7_1_B07_DBG_WRITE_VALUE4  0x07060504 /* 0x0706_0504 */

#define PSOP_7_1_B07_DBG_WRITE_ADDR5 0x18016024 /* 0x1801_6024 */
#define PSOP_7_1_B07_DBG_WRITE_VALUE5 0x0B0A0908 /* 0x0B0A_0908 */

#define PSOP_7_1_B07_DBG_WRITE_ADDR6 0x18016028 /* 0x1801_6028 */
#define PSOP_7_1_B07_DBG_WRITE_VALUE6 0x0F0E0D0C /* 0x0F0E_0D0C */

#define PSOP_7_1_B07_DBG_WRITE_ADDR7 0x1801602C /* 0x1801_602C */
#define PSOP_7_1_B07_DBG_WRITE_VALUE7  0x13121110 /* 0x1312_1110 */

#define PSOP_7_1_B07_DBG_WRITE_ADDR8 0x18016030 /* 0x1801_6030 */
#define PSOP_7_1_B07_DBG_WRITE_VALUE8  0x17161514 /* 0x1716_1514 */

#define PSOP_7_1_B07_DBG_WRITE_ADDR9 0x18016034 /* 0x1801_6034 */
#define PSOP_7_1_B07_DBG_WRITE_VALUE9  0x1B1A1918 /* 0x1B1A_1918 */

#define PSOP_7_1_B07_DBG_WRITE_ADDR10 0x18016038 /* 0x1801_6038 */
#define PSOP_7_1_B07_DBG_WRITE_VALUE10 0x1F1E1D1C /* 0x1F1E_1D1C */

#define PSOP_7_1_B07_DBG_WRITE_ADDR 0x18016058 /* 0x1801_6058[1] */
#define PSOP_7_1_B07_DBG_WRITE_MASK ~(BIT(1)) /* 0x1801_6058[1] */
#define PSOP_7_1_B07_DBG_WRITE_VALUE  (0x1 << 1) /* 1'b1 */
#define PSOP_7_1_B07_DBG_READ_ADDR 0x18023200 /* 0x1802_3200 */


/* B08 Need  WRITE 11 STEPs then READ one */
/* conn_von_clkgen */
#define PSOP_7_1_B08_DBG_WRITE_ADDR1  0x18093080 /* 0x1809_3080[3] */
#define PSOP_7_1_B08_DBG_WRITE_MASK1  ~(BIT(3)) /* 0x1809_3080[3] */
#define PSOP_7_1_B08_DBG_WRITE_VALUE1  (0x0 << 3) /* 1'b0 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR2  0x18097058 /* 0x18097058 */
#define PSOP_7_1_B08_DBG_WRITE_VALUE2  0x2 /* 0x2 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR3_ADDR4 0x1809703C /* 0x1809_703c */
#define PSOP_7_1_B08_DBG_WRITE_MASK3  ~(BITS(0, 5)) /* 0x1809_703c[5:0] */
#define PSOP_7_1_B08_DBG_WRITE_VALUE3  (0x14 << 0) /* 0x14 */

#define PSOP_7_1_B08_DBG_WRITE_MASK4  ~(BITS(8, 13))/* 0x1809_703c[13:8] */
#define PSOP_7_1_B08_DBG_WRITE_VALUE4   (0x14 << 8) /* 0x15 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR5 0x1809701C /* 0x1809701C */
#define PSOP_7_1_B08_DBG_WRITE_VALUE5 0x03020100 /* 0x03020100 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR6 0x18097020 /* 0x18097020 */
#define PSOP_7_1_B08_DBG_WRITE_VALUE6 0x07060504 /* 0x07060504 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR7 0x18097024 /* 0x18097024 */
#define PSOP_7_1_B08_DBG_WRITE_VALUE7 0x0B0A0908 /* 0x0b0a0908 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR8 0x18097028 /* 0x18097028 */
#define PSOP_7_1_B08_DBG_WRITE_VALUE8 0x0F0E0D0C /* 0x0f0e0d0c */

#define PSOP_7_1_B08_DBG_WRITE_ADDR9 0x1809702C /* 0x1809702C */
#define PSOP_7_1_B08_DBG_WRITE_VALUE9 0x13121110 /* 0x13121110 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR10 0x18097030 /* 0x18097030 */
#define PSOP_7_1_B08_DBG_WRITE_VALUE10 0x17161514 /* 0x17161514 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR11 0x18097034 /* 0x18097034 */
#define PSOP_7_1_B08_DBG_WRITE_VALUE11 0x1B1A1918 /* 0x1b1a1918 */

#define PSOP_7_1_B08_DBG_WRITE_ADDR 0x18097038 /* 0x18097038 */
#define PSOP_7_1_B08_DBG_WRITE_VALUE 0x1F1E1D1C  /* 0x1f1e1d1c */
#define PSOP_7_1_B08_DBG_READ_ADDR 0x18097054 /* 0x18097054 */

/* B09 Need  WRITE STEPs then READ one */
/* conn_von_clkgen */
#define PSOP_7_1_B09_DBG_WRITE_ADDR1  0x18093080 /* 0x1809_3080[3] */
#define PSOP_7_1_B09_DBG_WRITE_MASK1  ~(BIT(3)) /* 0x1809_3080[3] */
#define PSOP_7_1_B09_DBG_WRITE_VALUE1  (0x1 << 3) /* 1'b1 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR2  0x18097058 /* 0x18097058 */
#define PSOP_7_1_B09_DBG_WRITE_VALUE2  0x2 /* 0x2 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR3_ADDR4 0x1809703C /* 0x1809_703c */
#define PSOP_7_1_B09_DBG_WRITE_MASK3  ~(BITS(0, 5)) /* 0x1809_703c[5:0] */
#define PSOP_7_1_B09_DBG_WRITE_VALUE3  (0x14 << 0) /* 0x14 */

#define PSOP_7_1_B09_DBG_WRITE_MASK4  ~(BITS(8, 13))/* 0x1809_703c[13:8] */
#define PSOP_7_1_B09_DBG_WRITE_VALUE4   (0x14 << 8) /* 0x15 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR5 0x1809701C /* 0x1809701C */
#define PSOP_7_1_B09_DBG_WRITE_VALUE5 0x03020100 /* 0x03020100 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR6 0x18097020 /* 0x18097020 */
#define PSOP_7_1_B09_DBG_WRITE_VALUE6 0x07060504 /* 0x07060504 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR7 0x18097024 /* 0x18097024 */
#define PSOP_7_1_B09_DBG_WRITE_VALUE7 0x0B0A0908 /* 0x0b0a0908 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR8 0x18097028 /* 0x18097028 */
#define PSOP_7_1_B09_DBG_WRITE_VALUE8 0x0F0E0D0C /* 0x0f0e0d0c */

#define PSOP_7_1_B09_DBG_WRITE_ADDR9 0x1809702C /* 0x1809702C */
#define PSOP_7_1_B09_DBG_WRITE_VALUE9 0x13121110 /* 0x13121110 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR10 0x18097030 /* 0x18097030 */
#define PSOP_7_1_B09_DBG_WRITE_VALUE10 0x17161514 /* 0x17161514 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR11 0x18097034 /* 0x18097034 */
#define PSOP_7_1_B09_DBG_WRITE_VALUE11 0x1B1A1918 /* 0x1b1a1918 */

#define PSOP_7_1_B09_DBG_WRITE_ADDR 0x18097038 /* 0x18097038 */
#define PSOP_7_1_B09_DBG_WRITE_VALUE 0x1F1E1D1C  /* 0x1f1e1d1c */
#define PSOP_7_1_B09_DBG_READ_ADDR 0x18097054 /* 0x18097054 */


/****************************************/
/*                                      */
/* 9_Griffin_NIC_cb_infra_debug_cr.xlsx */
/* - sheet status result                */
/*                                      */
/****************************************/
/* 8. CB_INFRA/CBTOP  (Note, wiki link is 8 but excel naming is 9) */

/**********************/
/* SectionA - cb_infra vlp */
/**********************/
#define PSOP_9_1_A01_READ_ADDR 0x70025000 /* cb_infra_slp_ctrl */
/* truncated */
#define PSOP_9_1_A34_READ_ADDR 0x70025084 /* cb_infra_slp_ctrl */

#define PSOP_9_1_A35_READ_ADDR 0x700250F0 /* cb_infra_slp_ctrl */

#define PSOP_9_1_A36_READ_ADDR 0x70025100 /* cb_infra_slp_ctrl */
/* truncated */
#define PSOP_9_1_A43_READ_ADDR 0x7002511C /* cb_infra_slp_ctrl */

#define PSOP_9_1_A44_READ_ADDR 0x70025120 /* cb_infra_slp_ctrl */
#define PSOP_9_1_A45_READ_ADDR 0x70025130 /* cb_infra_slp_ctrl */
#define PSOP_9_1_A46_READ_ADDR 0x70025140 /* cb_infra_slp_ctrl */
#define PSOP_9_1_A47_READ_ADDR 0x70025180 /* cb_infra_slp_ctrl */
#define PSOP_9_1_A48_READ_ADDR 0x70025210 /* cb_infra_slp_ctrl */

#define PSOP_9_1_A49_READ_ADDR 0x70025400 /* cb_infra_slp_ctrl */
/* truncated */
#define PSOP_9_1_A54_READ_ADDR 0x70025414 /* cb_infra_slp_ctrl */

#define PSOP_9_1_A55_A67_DBG_WRITE_ADDR 0x70025300 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A55_A67_DBG_READ_ADDR   0x70025304 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A55_WRITE_VALUE 0x00010001 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A56_WRITE_VALUE 0x00010203 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A57_WRITE_VALUE 0x00010405 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A58_WRITE_VALUE 0x00010607 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A59_WRITE_VALUE 0x00010809 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A60_WRITE_VALUE 0x00010A0B /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A61_WRITE_VALUE 0x00010C0D /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A62_WRITE_VALUE 0x00010E0F /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A63_WRITE_VALUE 0x00011011 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A64_WRITE_VALUE 0x00011213 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A65_WRITE_VALUE 0x00011415 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A66_WRITE_VALUE 0x00011617 /* cb_infra_slp_ctrl (dbg) */
#define PSOP_9_1_A67_WRITE_VALUE 0x00011819 /* cb_infra_slp_ctrl (dbg) */

/********************************/
/* SectionB - cb_infra vcore on */
/********************************/

#define PSOP_9_1_B01_READ_ADDR 0x7002801C /* cb_infra_rgu */
#define PSOP_9_1_B02_READ_ADDR 0x7002802C /* cb_infra_rgu */
#define PSOP_9_1_B03_READ_ADDR 0x700283C0 /* cb_infra_rgu */

#define PSOP_9_1_B04_READ_ADDR 0x70028424 /* cb_infra_rgu */
/* truncated */
#define PSOP_9_1_B10_READ_ADDR 0x7002843C /* cb_infra_rgu */

#define PSOP_9_1_B11_READ_ADDR 0x70028520 /* cb_infra_rgu */
/* truncated */
#define PSOP_9_1_B18_READ_ADDR 0x7002873C /* cb_infra_rgu */

#define PSOP_9_1_B19_READ_ADDR 0x70028900 /* cb_infra_rgu */

/******************************************/
/* SectionC - cb_infra off, read check ok */
/******************************************/
#define PSOP_9_1_C01_READ_ADDR 0x70026000 /* cb_infra_misc0 (GALS_DBG) */
/* truncated */
#define PSOP_9_1_C04_READ_ADDR 0x7002600C /* cb_infra_misc0 (GALS_DBG) */

#define PSOP_9_1_C05_READ_ADDR 0x70026100 /* cb_infra_misc0 (GALS_DBG) */
/* truncated */
#define PSOP_9_1_C08_READ_ADDR 0x7002610C /* cb_infra_misc0 (GALS_DBG) */

#define PSOP_9_1_C09_READ_ADDR 0x70026550 /* cb_infra_misc0 (DEBUG_SEL) */
#define PSOP_9_1_C10_READ_ADDR 0x70026554 /* cb_infra_misc0 (REMAPPING CR) */
#define PSOP_9_1_C11_READ_ADDR 0x70026558 /* cb_infra_misc0 (REMAPPING CR) */

#define PSOP_9_1_C12_READ_ADDR 0x70026A50 /* cb_infra_misc0 (BUS TIMEOUT) */
#define PSOP_9_1_C13_READ_ADDR 0x70026A58 /* cb_infra_misc0 (BUS TIMEOUT) */
#define PSOP_9_1_C14_READ_ADDR 0x70026A5C /* cb_infra_misc0 (BUS TIMEOUT) */
#define PSOP_9_1_C15_READ_ADDR 0x70026A60 /* cb_infra_misc0 (BUS TIMEOUT) */
#define PSOP_9_1_C16_READ_ADDR 0x70026A64 /* cb_infra_misc0 (BUS TIMEOUT) */
#define PSOP_9_1_C17_READ_ADDR 0x70026A68 /* cb_infra_misc0 (BUS TIMEOUT) */
#define PSOP_9_1_C18_READ_ADDR 0x70026A6C /* cb_infra_misc0 (BUS TIMEOUT) */

#define PSOP_9_1_C19_READ_ADDR 0x70020004 /* cb_infra_ckgen */
#define PSOP_9_1_C20_READ_ADDR 0x70020008 /* cb_infra_ckgen */
#define PSOP_9_1_C21_READ_ADDR 0x70020080 /* cb_infra_ckgen */
#define PSOP_9_1_C22_READ_ADDR 0x70020110 /* cb_infra_ckgen */

#define PSOP_9_1_C23_READ_ADDR 0x70021060 /* eef_top */
/* truncated */
#define PSOP_9_1_C39_READ_ADDR 0x700210A0 /* eef_top */

/**********************/
/* SectionD - pcie #0 */
/**********************/
#define PSOP_9_1_D01_WRITE_ADDR 0x70026554 /*   0x70026554[31:16] PCIE LTSSM */
#define PSOP_9_1_D01_WRITE_MASK ~(BITS(16, 31)) /*  0x70026554[31:16] PCIE LTSSM */
#define PSOP_9_1_D01_WRITE_VALUE (0x7403 << 16) /* 0x70026554[31:16] PCIE LTSSM */
#define PSOP_9_1_D01_READ_ADDR 0x74030150 /* PCIE LTSSM */

#define PSOP_9_1_D02_READ_ADDR 0x74030154 /* PCIE LP state */
#define PSOP_9_1_D03_READ_ADDR 0x74030184 /* PCIE INT status */
#define PSOP_9_1_D04_READ_ADDR 0x74031010 /* PCIE Cfg BAR */


#define PSOP_9_1_D08_READ_ADDR 0x74031204
#define PSOP_9_1_D09_READ_ADDR 0x74031210
#define PSOP_9_1_D10_READ_ADDR 0x74030184
#define PSOP_9_1_D11_READ_ADDR 0x740331C0

#define PSOP_9_1_D12_READ_ADDR 0x74030d40 /* PHY_ERR_DEBUG_LANE0 */
#define PSOP_9_1_D13_READ_ADDR 0x740301d4 /* ISTATUS_PENDING_ADT */
#define PSOP_9_1_D14_READ_ADDR 0x74030e00 /* AXI0_ERR_ADDR_L */
#define PSOP_9_1_D15_READ_ADDR 0x74030e04 /* AXI0_ERR_ADDR_H */
#define PSOP_9_1_D16_READ_ADDR 0x74030e04 /* AXI0_ERR_INFO */
#define PSOP_9_1_D17_READ_ADDR 0x74030e40 /* PCIE_ERR_ADDR_L */
#define PSOP_9_1_D18_READ_ADDR 0x74030e44 /* PCIE_ERR_ADDR_H */
#define PSOP_9_1_D19_READ_ADDR 0x74030e48 /* PCIE_ERR_INFO */
#define PSOP_9_1_D20_READ_ADDR 0x74031488 /* PCIE_VENDOR_STS_0 */
#define PSOP_9_1_D21_READ_ADDR 0x7403148c /* PCIE_VENDOR_STS_1 */

/***********************/
/* SectionE - cbtop CR */
/***********************/
#define PSOP_9_1_E01_WRITE_ADDR 0x70026558 /* 0x70026558[15:0] cb_ckgen_top */
#define PSOP_9_1_E01_WRITE_MASK BITS(0, 15)    /* 0x70026558[15:0] cb_ckgen_top */
#define PSOP_9_1_E01_WRITE_VALUE 0x7000    /* 0x70026558[15:0] cb_ckgen_top */

#define PSOP_9_1_E01_READ_ADDR 0x7000020C /* cb_ckgen_top */
#define PSOP_9_1_E02_READ_ADDR 0x70000204 /* cb_ckgen_top */
#define PSOP_9_1_E03_READ_ADDR 0x70000218 /* cb_ckgen_top */
#define PSOP_9_1_E04_READ_ADDR 0x70000210 /* cb_ckgen_top */

/**********************/
/* SectionF - pcie #1 */
/**********************/
#define PSOP_9_1_F01_WRITE_ADDR 0x70026560 /*   0x70026560[31:16] PCIE LTSSM */
#define PSOP_9_1_F01_WRITE_MASK BITS(16, 31) /*  0x70026560[31:16] PCIE LTSSM */
#define PSOP_9_1_F01_WRITE_VALUE (0x7409 << 16) /* 0x70026560[31:16] PCIE LTSSM */
#define PSOP_9_1_F01_READ_ADDR 0x74090150 /* PCIE LTSSM */

#define PSOP_9_1_F02_READ_ADDR 0x74090154 /* PCIE LP state */
#define PSOP_9_1_F03_READ_ADDR 0x74090184 /* PCIE INT status */
#define PSOP_9_1_F04_READ_ADDR 0x74091010 /* PCIE Cfg BAR */

/* DE feedback useless */
#define PSOP_9_1_F08_READ_ADDR 0x74091204
#define PSOP_9_1_F09_READ_ADDR 0x74091210
#define PSOP_9_1_F10_READ_ADDR 0x74090184
#define PSOP_9_1_F11_READ_ADDR 0x740931C0
#define PSOP_9_1_F12_READ_ADDR 0x74090d40 /* PHY_ERR_DEBUG_LANE0 */
#define PSOP_9_1_F13_READ_ADDR 0x740901d4 /* ISTATUS_PENDING_ADT */
#define PSOP_9_1_F14_READ_ADDR 0x74090e00 /* AXI0_ERR_ADDR_L */
#define PSOP_9_1_F15_READ_ADDR 0x74090e04 /* AXI0_ERR_ADDR_H */
#define PSOP_9_1_F16_READ_ADDR 0x74090e08 /* AXI0_ERR_INFO */
#define PSOP_9_1_F17_READ_ADDR 0x74090e40 /* PCIE_ERR_ADDR_L */
#define PSOP_9_1_F18_READ_ADDR 0x74090e44 /* PCIE_ERR_ADDR_H */
#define PSOP_9_1_F19_READ_ADDR 0x74090e48 /* PCIE_ERR_INFO */
#define PSOP_9_1_F20_READ_ADDR 0x74091488 /* PCIE_VENDOR_STS_0 */
#define PSOP_9_1_F21_READ_ADDR 0x7409148c /* PCIE_VENDOR_STS_1 */

/***********************************/
/*                                 */
/* platform debug sop s/w define   */
/*                                 */
/***********************************/

enum DUMP_SIDE_{
	HOST_SIDE_BEFORE_ACCESS_CONSYS = 0,
	HOST_SIDE_INFRA_ON_READY = 1,
	OFF_SIDE_OFF_AND_ATOP = 2,
};

#define READ_CHECK_AP2CONN_INFRA_ON 	BIT(0)
#define READ_CHECK_AP2CONN_INFRA_OFF	BIT(1)
#define READ_CHECK_AP2WF				BIT(2)

#define DUMP_SECTION_A 		BIT(0)
#define DUMP_SECTION_B 		BIT(1)
#define DUMP_SECTION_C 		BIT(2)

#define CONN_INFRA_BUS_DBG_STATUS_RESULT 	BIT(3)
#define CONN_INFRA_BUS_DBG_SLPPROT_STATUS 	BIT(4)

#endif /* __MT7993_PLATFORM_DEBUG_SOP_H__ */
