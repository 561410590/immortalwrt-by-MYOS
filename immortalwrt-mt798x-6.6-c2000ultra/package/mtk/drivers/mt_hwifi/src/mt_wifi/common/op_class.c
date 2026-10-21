#ifdef DOT11_N_SUPPORT
#include "rt_config.h"

typedef enum {
	Reserved,
	FREQ_2G407,
	FREQ_2G414,
	FREQ_3G00,
	FREQ_3G0025,
	FREQ_4G00,
	FREQ_4G0025,
	FREQ_4G85,
	FREQ_4G89,
	FREQ_4G9375,
	FREQ_5G00,
	FREQ_5G0025,
	FREQ_5G925,
	FREQ_5G95
} START_FREQ;

/*
	definition for Behavior limits set
	Reference 11n/11ac spec Table D-2
*/
typedef enum {
	NOMADICBEHAVIOR = (1 << 1),
	LICENSEEXEMPTBEHAVIOR = (1 << 10),
	PRIMARYCHANNELLOWERBEHAVIOR = (1 << 13),
	PRIMARYCHANNELUPPERBEHAVIOR = (1 << 14),
	CCA_EDBEHAVIOR = (1 << 15),
	DFS_50_100_BEHAVIOR = (1 << 16),
	ITS_NONMOBILE_OPERATIONS = (1 << 17),
	ITS_MOBILE_OPERATIONS = (1 << 18),
	PLUS_80 = (1 << 19),
	USEEIRPFORVHTTXPOWENV = (1 << 20),
	COMMON = 0xffffff
} BEHAVIOR_LIMITS_SET;


typedef struct {
	UCHAR		reg_class;			/* regulatory class */
	UCHAR		global_class;		/* Global operating class */
	START_FREQ	start_freq;			/* Channel starting frequency*/
	UCHAR		spacing;			/* 0: 20Mhz, 1: 40Mhz */
	UCHAR		channel_set[16];	/* max 15 channels, use 0 as terminator */
	BEHAVIOR_LIMITS_SET behavior_limit_set; /* Behavior limits set */
} REG_CLASS, *PREG_CLASS;

typedef struct {
	UCHAR		reg_class;			/* regulatory class */
	UCHAR		global_class;		/* Global operating class */
	START_FREQ	start_freq;			/* Channel starting frequency*/
	UCHAR		spacing;			/* 0: 20Mhz, 1: 40Mhz */
	UCHAR		channel_set[16];	/* max 15 channels, use 0 as terminator */
	UCHAR       center_freq[16];	/* max 15 channels, use 0 as terminator */
	BEHAVIOR_LIMITS_SET behavior_limit_set; /* Behavior limits set */
} REG_CLASS_VHT, *PREG_CLASS_VHT;

typedef struct {
	UCHAR		reg_class;			/* regulatory class */
	UCHAR		global_class;		/* Global operating class */
	START_FREQ	start_freq;			/* Channel starting frequency*/
	UCHAR		spacing;		/* 0: 20MHz, 1: 40MHz, 2: 80 MHz, 3: 160 MH, 10: 320 MHz */
	UCHAR		channel_set[60];	/* max 59 channels, use 0 as terminator */
	UCHAR		center_freq[60];	/* max 59 channels, use 0 as terminator */
	BEHAVIOR_LIMITS_SET behavior_limit_set; /* Behavior limits set */
} REG_CLASS_HE_EHT, *PREG_CLASS_HE_EHT;


struct REG_CLASS_GLOBAL {
	UCHAR		global_class;		/* Global operating class */
	START_FREQ	start_freq;			/* Channel starting frequency*/
	UCHAR		spacing;			/* reference BW_XX*/
	UCHAR		channel_set[60];	/* max 59 channels, use 0 as terminator */
	UCHAR		center_ch_set[60];	/* max 59 channels, use 0 as terminator */
	BEHAVIOR_LIMITS_SET behavior_limit_set; /* Behavior limits set */
};

#define OP_CLASS_81 81
#define OP_CLASS_84 84
#define OP_CLASS_115 115
#define OP_CLASS_127 127
#define OP_CLASS_129 129
#define OP_CLASS_125 125
#define OP_CLASS_130 130
#define OP_CLASS_135 135
#define OP_CLASS_136 136
#define OP_CLASS_137 137

#define IS_20M_2G(__opclass) \
	((__opclass) == 81 || (__opclass) == 82)

#define IS_20M_5G(__opclass) \
	((__opclass) == 115 || (__opclass) == 118 || (__opclass) == 121 \
	|| (__opclass) == 124 || (__opclass) == 125)

#define IS_20M_6G(__opclass) \
	((__opclass) == 131 || (__opclass) == 136)

#define IS_20M(__opclass) \
	(IS_20M_2G(__opclass) || IS_20M_5G(__opclass) || IS_20M_6G(__opclass))


#define IS_40M_2G_LOWER(__opclass) ((__opclass) == 83)

#define IS_40M_2G_UPPER(__opclass) ((__opclass) == 84)

#define IS_40M_5G_LOWER(__opclass) \
	((__opclass) == 116 || (__opclass) == 119 || (__opclass) == 122 || (__opclass) == 126)

#define IS_40M_5G_UPPER(__opclass) \
	((__opclass) == 117 || (__opclass) == 120 || (__opclass) == 123 || (__opclass) == 127)

#define IS_40M_6G(__opclass) ((__opclass) == 132)

#define IS_40M(__opclass) \
	(IS_40M_2G_LOWER(__opclass) || IS_40M_2G_UPPER(__opclass) \
	|| IS_40M_5G_LOWER(__opclass) || IS_40M_5G_UPPER(__opclass) \
	|| IS_40M_6G(__opclass))

#define IS_80M_5G(__opclass) \
	((__opclass) == 128)

#define IS_8080_5G(__opclass) \
	((__opclass) == 130)

#define IS_80M_6G(__opclass) \
	((__opclass) == 133)

#define IS_8080_6G(__opclass) \
	((__opclass) == 135)

#define IS_80M(__opclass) \
	(IS_80M_5G(__opclass) || IS_80M_6G(__opclass))

#define IS_8080(__opclass) \
	(IS_8080_5G(__opclass) || IS_8080_6G(__opclass))

#define IS_160M_5G(__opclass) ((__opclass) == 129)

#define IS_160M_6G(__opclass) ((__opclass) == 134)

#define IS_160M(__opclass) \
	(IS_160M_5G(__opclass) || IS_160M_6G(__opclass))

#define IS_320M(__opclass) ((__opclass) == 137)

#define IS_2G_OPCLASS(__opclass) \
	((__opclass) >= 81 && (__opclass) <= 84)

#define IS_5G_OPCLASS(__opclass) \
	((__opclass) >= 115 && (__opclass) <= 130)

#ifdef DOT11_EHT_BE
#define IS_6G_OPCLASS(__opclass) \
	((__opclass) >= 131 && (__opclass) <= 137)
#else
#define IS_6G_OPCLASS(__opclass) \
	((__opclass) >= 131 && (__opclass) <= 136)
#endif


static const UCHAR center_channels_40m_5ghz[] = {
	38, 46, 54, 62,
	102, 110, 118, 126, 134, 142,
	151, 159, 167, 175
};

static const UCHAR center_channels_80m_5ghz[] = {
	42, 58, 106, 122, 138, 155, 171
};

static const UCHAR center_channels_160m_5ghz[] = {
	50, 114, 163
};

static const UCHAR center_channels_40m_6ghz[] = {
	38, 46, 54, 62,
	102, 110, 118, 126, 134, 142,
	151, 159, 167, 175
};

static const UCHAR center_channels_80m_6ghz[] = {
	7, 23, 39, 55, 71, 87, 103, 119,
	135, 151, 167, 183, 199, 215
};

static const UCHAR center_channels_160m_6ghz[] = {
	15, 47, 79, 111, 143, 175, 207
};



#ifdef MAP_R2
#define TX_MAX_STREAM 4
#define RX_MAX_STREAM 4
#endif
extern COUNTRY_REGION_CH_DESC Country_Region_ChDesc_5GHZ[];
#ifdef MAP_6E_SUPPORT
extern COUNTRY_REGION_CH_DESC Country_Region_ChDesc_6GHZ[];
#endif

static const UCHAR wfa_ht_ch_ext[] = {
	36, EXTCHA_ABOVE, 40, EXTCHA_BELOW,
	44, EXTCHA_ABOVE, 48, EXTCHA_BELOW,
	52, EXTCHA_ABOVE, 56, EXTCHA_BELOW,
	60, EXTCHA_ABOVE, 64, EXTCHA_BELOW,
	100, EXTCHA_ABOVE, 104, EXTCHA_BELOW,
	108, EXTCHA_ABOVE, 112, EXTCHA_BELOW,
	116, EXTCHA_ABOVE, 120, EXTCHA_BELOW,
	124, EXTCHA_ABOVE, 128, EXTCHA_BELOW,
	132, EXTCHA_ABOVE, 136, EXTCHA_BELOW,
	140, EXTCHA_ABOVE, 144, EXTCHA_BELOW,
	149, EXTCHA_ABOVE, 153, EXTCHA_BELOW,
	157, EXTCHA_ABOVE, 161, EXTCHA_BELOW,
#ifndef IAP_VENDOR1_FEATURE_SUPPORT
	165, EXTCHA_ABOVE, 169, EXTCHA_BELOW,
	173, EXTCHA_ABOVE, 177, EXTCHA_BELOW,
#endif
	0, 0
};

static const UCHAR wfa_ht_ch_ext_6G[] = {
	1, EXTCHA_ABOVE, 5, EXTCHA_BELOW,
	9, EXTCHA_ABOVE, 13, EXTCHA_BELOW,
	17, EXTCHA_ABOVE, 21, EXTCHA_BELOW,
	25, EXTCHA_ABOVE, 29, EXTCHA_BELOW,
	33, EXTCHA_ABOVE, 37, EXTCHA_BELOW,
	41, EXTCHA_ABOVE, 45, EXTCHA_BELOW,
	49, EXTCHA_ABOVE, 53, EXTCHA_BELOW,
	57, EXTCHA_ABOVE, 61, EXTCHA_BELOW,
	65, EXTCHA_ABOVE, 69, EXTCHA_BELOW,
	73, EXTCHA_ABOVE, 77, EXTCHA_BELOW,
	81, EXTCHA_ABOVE, 85, EXTCHA_BELOW,
	89, EXTCHA_ABOVE, 93, EXTCHA_BELOW,
	97, EXTCHA_ABOVE, 101, EXTCHA_BELOW,
	105, EXTCHA_ABOVE, 109, EXTCHA_BELOW,
	113, EXTCHA_ABOVE, 117, EXTCHA_BELOW,
	121, EXTCHA_ABOVE, 125, EXTCHA_BELOW,
	129, EXTCHA_ABOVE, 133, EXTCHA_BELOW,
	137, EXTCHA_ABOVE, 141, EXTCHA_BELOW,
	145, EXTCHA_ABOVE, 149, EXTCHA_BELOW,
	153, EXTCHA_ABOVE, 157, EXTCHA_BELOW,
	161, EXTCHA_ABOVE, 165, EXTCHA_BELOW,
	169, EXTCHA_ABOVE, 173, EXTCHA_BELOW,
	177, EXTCHA_ABOVE, 181, EXTCHA_BELOW,
	185, EXTCHA_ABOVE, 189, EXTCHA_BELOW,
	193, EXTCHA_ABOVE, 197, EXTCHA_BELOW,
	201, EXTCHA_ABOVE, 205, EXTCHA_BELOW,
	209, EXTCHA_ABOVE, 213, EXTCHA_BELOW,
	217, EXTCHA_ABOVE, 221, EXTCHA_BELOW,
	225, EXTCHA_ABOVE, 229, EXTCHA_BELOW,
	0, 0
};

/*
	Table E-4 Global Operating classes( Only List 2.4G,5G,6G part)
*/
struct REG_CLASS_GLOBAL reg_class_global[] = {
	{0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{81,  FREQ_2G407, BW_25,	{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
								12, 13, 0}, {0},									COMMON},
	{82,  FREQ_2G414, BW_25,	{14, 0}, {0},										LICENSEEXEMPTBEHAVIOR },
	{83,  FREQ_2G407, BW_40,	{1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},				PRIMARYCHANNELLOWERBEHAVIOR},
	{84,  FREQ_2G407, BW_40,	{5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},			PRIMARYCHANNELUPPERBEHAVIOR},

	{115, FREQ_5G00, BW_20,		{36, 40, 44, 48, 0}, {0},							USEEIRPFORVHTTXPOWENV},
	{116, FREQ_5G00, BW_40,		{36, 44, 0}, {0},									PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{117, FREQ_5G00, BW_40,		{40, 48, 0}, {0},									PRIMARYCHANNELUPPERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{118, FREQ_5G00, BW_20,		{52, 56, 60, 64, 0}, {0},							DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{119, FREQ_5G00, BW_40,		{52, 60, 0}, {0},									PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{120, FREQ_5G00, BW_40,		{56, 64, 0}, {0},									PRIMARYCHANNELUPPERBEHAVIOR | DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{121, FREQ_5G00, BW_20,		{100, 104, 108, 112, 116, 120, 124, 128,
								132, 136, 140, 144, 0}, {0},						DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{122, FREQ_5G00, BW_40,		{100, 108, 116, 124, 132, 140, 0}, {0},				PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{123, FREQ_5G00, BW_40,		{104, 112, 120, 128, 136, 144, 0}, {0},				PRIMARYCHANNELUPPERBEHAVIOR | DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{124, FREQ_5G00, BW_20,		{149, 153, 157, 161, 0}, {0},						NOMADICBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{125, FREQ_5G00, BW_20,		{149, 153, 157, 161, 165, 169, 173, 177, 0}, {0},	LICENSEEXEMPTBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{126, FREQ_5G00, BW_40,		{149, 157, 165, 173, 0}, {0},						PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{127, FREQ_5G00, BW_40,		{153, 161, 169, 177, 0}, {0},						PRIMARYCHANNELUPPERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{128, FREQ_5G00, BW_80,		{0}, {42, 58, 106, 122, 138, 155, 171, 0},			USEEIRPFORVHTTXPOWENV},
	{129, FREQ_5G00, BW_160,	{0}, {50, 114, 163, 0},								USEEIRPFORVHTTXPOWENV},
	{130, FREQ_5G00, BW_80,		{0}, {42, 58, 106, 122, 138, 155, 171, 0},			PLUS_80 | USEEIRPFORVHTTXPOWENV},

	/* For 6G, the 20M op_class are 131 and 136. In table E4, the channels are in
	 * the "channel_set" rather than the "cent_ch_set". However, to simplify the
	 * code(since 20MHz channels are inherently their own center channels, the
	 * channels are placed in the "cent_ch_set". if reg > 127 and reg <= 137, use
	 * "cent_ch_set", else use channel_set.
	 */
	{131, FREQ_5G95, BW_20,		{0}, {1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
									  41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
									  81, 85, 89, 93, 97, 101, 105, 109, 113, 117,
									  121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
									  161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
									  201, 205, 209, 213, 217, 221, 225, 229, 233, 0},							COMMON},
	{132, FREQ_5G95, BW_40,		{0}, {3, 11, 19, 27, 35, 43, 51, 59, 67, 75,
									  83, 91, 99, 107, 115, 123, 131, 139, 147, 155,
									  163, 171, 179, 187, 195, 203, 211, 219, 227, 0},							COMMON},
	{133, FREQ_5G95, BW_80,		{0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151,
									  167, 183, 199, 215, 0},													COMMON},
	{134, FREQ_5G95, BW_160,	{0}, {15, 47, 79, 111, 143, 175, 207, 0},										COMMON},
	{135, FREQ_5G95, BW_80,		{0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151, 167, 183, 199, 215, 0},		PLUS_80},
	{136, FREQ_5G925, BW_20,	{0}, {2},																		COMMON},
#ifdef DOT11_EHT_BE
	{137, FREQ_5G95, BW_320,	{0}, {31, 63, 95, 127, 159, 191, 0},											COMMON},
#endif
	{0, 0, 0, {0}, {0}, 0},			/* End */
};

/*
	Table E-1?Operating classes in the United States  (11N)
*/
REG_CLASS reg_class_fcc[] = {
	{0, 0, 0, 0, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0},																DFS_50_100_BEHAVIOR},
	{3,  124, FREQ_5G00,   BW_20, {149, 153, 157, 161, 0},															NOMADICBEHAVIOR},
	{4,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0},						DFS_50_100_BEHAVIOR},
	{5,  125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 173, 177, 0},										LICENSEEXEMPTBEHAVIOR},
	{6,  103, FREQ_4G9375, BW_5,  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0},												COMMON},
	{7,  103, FREQ_4G9375, BW_5,  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0},												COMMON},
	{8,  102, FREQ_4G89,   BW_10, {11, 13, 15, 17, 19, 0},															COMMON},
	{9,  102, FREQ_4G89,   BW_10, {11, 13, 15, 17, 19, 0},															COMMON},
	{10, 101, FREQ_4G85,   BW_20, {21, 25, 0},																		COMMON},
	{11, 101, FREQ_4G85,   BW_20, {21, 25, 0},																		COMMON},
	{12,  81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0},											LICENSEEXEMPTBEHAVIOR},
	{13,  94, FREQ_3G00,   BW_20, {133, 137, 0},																	CCA_EDBEHAVIOR},
	{14,  95, FREQ_3G00,   BW_10, {132, 134, 136, 138, 0},															CCA_EDBEHAVIOR},
	{15,  96, FREQ_3G0025, BW_5,  {131, 132, 133, 134, 135, 136, 137, 138, 0},										CCA_EDBEHAVIOR},
	{16,   0, FREQ_5G0025, BW_5,  {170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0},	ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{18,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0},					ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{22, 116, FREQ_5G00,   BW_40, {36, 44, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{23, 119, FREQ_5G00,   BW_40, {52, 60, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{24, 122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 0},														PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{25, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{26, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173}, 																LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{27, 117, FREQ_5G00,   BW_40, {40, 48, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{28, 120, FREQ_5G00,   BW_40, {56, 64, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{29, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 0},													NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{30, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0},																	NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{31, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0},																	LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{32,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 0},														LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{33,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 0},														LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{128, 128, FREQ_5G00,  BW_80, {0},																			COMMON},
	{129, 129, FREQ_5G00,  BW_80, {0},																			COMMON},
	{130, 130, FREQ_5G00,  BW_80, {0},																			COMMON},

	{0, 0, 0, 0, {0}, 0}			/* end */
};

/*
	Table E-2?Operating classes in Europe (11N)
*/
	REG_CLASS reg_class_ce[] = {
		{0, 0, 0, 0, {0}, 0},			/* Invlid entry */
		{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0},																COMMON},
		{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0},																NOMADICBEHAVIOR},
		{3,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0},						COMMON},
		{4,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0},									LICENSEEXEMPTBEHAVIOR},
		{5,  116, FREQ_5G00,   BW_40, {36, 44, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
		{6,  119, FREQ_5G00,   BW_40, {52, 60, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
		{7,  122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 0},														PRIMARYCHANNELLOWERBEHAVIOR},
		{8,  117, FREQ_5G00,   BW_40, {40, 48, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
		{9,  120, FREQ_5G00,   BW_40, {56, 64, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
		{10, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 0},														PRIMARYCHANNELUPPERBEHAVIOR}, /* 10 */
		{11,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
		{12,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
		{13,   0, FREQ_5G0025, BW_5,  {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
		{14,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
		{15,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0},					ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
		{16,   0, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0},						ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
		{17, 125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 0},												COMMON},
		{0, 0, 0, 0, {0}, 0}			/* end */
	};

REG_CLASS reg_class_ce_support_ch144[] = {
	{0, 0, 0, 0, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0},																NOMADICBEHAVIOR},
	{3,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0},					COMMON},
	{4,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0},									LICENSEEXEMPTBEHAVIOR},
	{5,  116, FREQ_5G00,   BW_40, {36, 44, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{6,  119, FREQ_5G00,   BW_40, {52, 60, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{7,  122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0},												PRIMARYCHANNELLOWERBEHAVIOR},
	{8,  117, FREQ_5G00,   BW_40, {40, 48, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{9,  120, FREQ_5G00,   BW_40, {56, 64, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{10, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0},												PRIMARYCHANNELUPPERBEHAVIOR}, /* 10 */
	{11,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{12,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{13,   0, FREQ_5G0025, BW_5,  {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{14,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{15,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0},					ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{16,   0, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0},						ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17, 125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 0},												COMMON},
	{25, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{26, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0},																LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{30, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0},																NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{31, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0},																LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{0, 0, 0, 0, {0}, 0}			/* end */
};

/*
	Table E-3?Operating classes in Japan (11N)
*/
REG_CLASS reg_class_jp[] = {
	{0, 0, 0, 0, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {34, 36, 38, 40, 42, 44, 46, 48, 0},												COMMON},/* 1 */
	{8,  109, FREQ_4G00,   BW_20, {184, 188, 192, 196, 0},														COMMON},
	{11, 109, FREQ_4G00,   BW_20, {184, 188, 192, 196, 0},														COMMON},
	{17, 110, FREQ_4G00,   BW_10, {183, 184, 185, 187, 188, 189, 0},												COMMON},
	{20, 110, FREQ_4G00,   BW_10, {183, 184, 185, 187, 188, 189, 0},												COMMON},
	{25, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0},									COMMON},
	{26, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0},									COMMON},
	{29, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0},									COMMON},
	{30,  81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0},									LICENSEEXEMPTBEHAVIOR},
	{31,  82, FREQ_2G414,  BW_25, {14, 0},																			LICENSEEXEMPTBEHAVIOR},
	{32, 118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0},																COMMON},
	{33, 118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0},																COMMON},
	{34, 121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0},						DFS_50_100_BEHAVIOR},
	{36, 116, FREQ_5G00,   BW_40, {36, 44, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{37, 119, FREQ_5G00,   BW_40, {52, 60, 0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{39, 122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0},														PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{41, 117, FREQ_5G00,   BW_40, {40, 48, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{42, 120, FREQ_5G00,   BW_40, {56, 64, 0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{44, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0},														PRIMARYCHANNELUPPERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{46, 104, FREQ_4G00,   BW_40, {184, 192, 0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{51, 105, FREQ_4G00,   BW_40, {188, 196, 0},																	PRIMARYCHANNELUPPERBEHAVIOR},
	{56,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{57,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{58, 121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0},						NOMADICBEHAVIOR | LICENSEEXEMPTBEHAVIOR},
	{0, 0, 0, 0, {0}, 0}			/* end */
};

REG_CLASS reg_class_cn[] = {
	{0, 0, 0, 0, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0},																USEEIRPFORVHTTXPOWENV},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0},																DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{3,  125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 0},						USEEIRPFORVHTTXPOWENV},
	{4,  116, FREQ_5G00,   BW_40, {36, 44, 0},																		PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{5,  119, FREQ_5G00,   BW_40, {52, 60, 0},																		PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{6,  126, FREQ_5G00,   BW_40, {149, 157, 0},																	PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{7,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0},									LICENSEEXEMPTBEHAVIOR},
	{8,   83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{9,   84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{0, 0, 0, 0, {0}, 0}			/* end */
};

#ifdef DOT11_VHT_AC
/*
	Table E-1?Operating classes in the United States  (11AC)
*/
REG_CLASS_VHT reg_class_vht_fcc[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																DFS_50_100_BEHAVIOR},
	{3,  124, FREQ_5G00,   BW_20, {149, 153, 157, 161, 0}, {0},														NOMADICBEHAVIOR},
	{4,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0}, {0},				DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{5,  125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 173, 177, 0}, {0},                                                    LICENSEEXEMPTBEHAVIOR},
	{6,  103, FREQ_4G9375, BW_5,  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0}, {0},												COMMON},
	{7,  103, FREQ_4G9375, BW_5,  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0}, {0},												COMMON},
	{8,  102, FREQ_4G89,   BW_10, {11, 13, 15, 17, 19, 0}, {0},															COMMON},
	{9,  102, FREQ_4G89,   BW_10, {11, 13, 15, 17, 19, 0}, {0},														COMMON},
	{10, 101, FREQ_4G85,   BW_20, {21, 25, 0}, {0},																		COMMON},
	{11, 101, FREQ_4G85,   BW_20, {21, 25, 0}, {0},																		COMMON},
	{12,  81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0}, {0},											LICENSEEXEMPTBEHAVIOR},
	{13,  94, FREQ_3G00,   BW_20, {133, 137, 0}, {0},																	CCA_EDBEHAVIOR},
	{14,  95, FREQ_3G00,   BW_10, {132, 134, 136, 138, 0}, {0},															CCA_EDBEHAVIOR},
	{15,  96, FREQ_3G0025, BW_5,  {131, 132, 133, 134, 135, 136, 137, 138, 0}, {0},									CCA_EDBEHAVIOR},
	{16,   0, FREQ_5G0025, BW_5,  {170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},	ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{18,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0}, {0},				ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{22, 116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{23, 119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{24, 122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0},	{0},											DFS_50_100_BEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{25, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{26, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																	LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{27, 117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{28, 120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																	PRIMARYCHANNELUPPERBEHAVIOR},
	{29, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0}, {0},												NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{30, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0}, {0},																	NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{31, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0},	{0},																LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{32,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 0}, {0},														LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{33,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 0}, {0},														LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 138, 155, 171, 0},													USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 114, 163, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 138, 155, 171, 0},													PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};


/*
	Table E-2?Operating classes in Europe (11AC)
*/
REG_CLASS_VHT reg_class_vht_ce[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																NOMADICBEHAVIOR},
	{3,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						COMMON},
	{4,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{5,  116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{6,  119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{7,  122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 0}, {0},													PRIMARYCHANNELLOWERBEHAVIOR},
	{8,  117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{9,  120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{10, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 0}, {0},													PRIMARYCHANNELUPPERBEHAVIOR},
	{11,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{12,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{13,   0, FREQ_5G0025, BW_5,  {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{14,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{15,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0}, {0},					ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{16,   0, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17, 125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 0}, {0},												COMMON},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 114, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};

REG_CLASS_VHT reg_class_vht_ce_support_ch144[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																NOMADICBEHAVIOR},
	{3,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0}, {0},					COMMON},
	{4,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{5,  116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{6,  119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{7,  122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0}, {0},												PRIMARYCHANNELLOWERBEHAVIOR},
	{8,  117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{9,  120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{10, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0}, {0},												PRIMARYCHANNELUPPERBEHAVIOR},
	{11,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{12,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{13,   0, FREQ_5G0025, BW_5,  {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{14,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{15,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0}, {0},					ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{16,   0, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17, 125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 0}, {0},												COMMON},
	{25, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{26, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{30, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0}, {0},																NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{31, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0}, {0},															LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 138, 155, 0},												USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 114, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};

/*
	Table E-3?Operating classes in Japan (11N)
*/
REG_CLASS_VHT reg_class_vht_jp[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {34, 36, 38, 40, 42, 44, 46, 48, 0}, {0},											COMMON},
	{8,  109, FREQ_4G00,   BW_20, {184, 188, 192, 196, 0}, {0},														COMMON},
	{11, 109, FREQ_4G00,   BW_20, {184, 188, 192, 196, 0}, {0},														COMMON},
	{12, 113, FREQ_5G00,   BW_10, {7, 8, 9, 11, 0}, {0},																COMMON},
	{13, 113, FREQ_5G00,   BW_10, {7, 8, 9, 11, 0}, {0},																COMMON},
	{14, 113, FREQ_5G00,   BW_10, {7, 8, 9, 11, 0}, {0},																COMMON},
	{15, 113, FREQ_5G00,   BW_10, {7, 8, 9, 11, 0}, {0},																COMMON},
	{16, 110, FREQ_4G00,   BW_10, {183, 184, 185, 187, 188, 189, 0}, {0},											COMMON},
	{17, 110, FREQ_4G00,   BW_10, {183, 184, 185, 187, 188, 189, 0}, {0},											COMMON},
	{20, 110, FREQ_4G00,   BW_10, {183, 184, 185, 187, 188, 189, 0}, {0},											COMMON},
	{25, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0}, {0},									COMMON},
	{26, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0}, {0},									COMMON},
	{29, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0}, {0},									COMMON},
	{30,  81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{31,  82, FREQ_2G414,  BW_25, {14, 0}, {0},																		LICENSEEXEMPTBEHAVIOR},
	{32, 118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},															COMMON},
	{33, 118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},															COMMON},
	{34, 121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0}, {0},						DFS_50_100_BEHAVIOR},
	{36, 116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{37, 119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{39, 122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0}, {0},													PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{41, 117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																	PRIMARYCHANNELUPPERBEHAVIOR},
	{42, 120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{44, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0}, {0},													PRIMARYCHANNELUPPERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{46, 104, FREQ_4G00,   BW_40, {184, 192, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{51, 105, FREQ_4G00,   BW_40, {188, 196, 0}, {0},																	PRIMARYCHANNELUPPERBEHAVIOR},
	{56,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{57,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{58, 121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						NOMADICBEHAVIOR | LICENSEEXEMPTBEHAVIOR},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 138, 0},															USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 114, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};

REG_CLASS_VHT reg_class_vht_cn[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																USEEIRPFORVHTTXPOWENV},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{3,  125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 0}, {0},                                                    USEEIRPFORVHTTXPOWENV},
	{4,  116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{5,  119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{6,  126, FREQ_5G00,   BW_40, {149, 157, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{7,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{8,   83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{9,   84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 155, 0},																USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 0},																			USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 155, 0},																PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};

#endif /* DOT11_VHT_AC */
#ifdef DOT11_HE_AX
/*
	Table E-1?Operating classes in the United States  (11AC)
*/
REG_CLASS_HE_EHT reg_class_he_fcc[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																DFS_50_100_BEHAVIOR},
	{3,  124, FREQ_5G00,   BW_20, {149, 153, 157, 161, 0}, {0},														NOMADICBEHAVIOR},
	{4,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0}, {0},				DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{5,  125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 173, 177,  0}, {0},                                                    LICENSEEXEMPTBEHAVIOR},
	{6,  103, FREQ_4G9375, BW_5,  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0}, {0},												COMMON},
	{7,  103, FREQ_4G9375, BW_5,  {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0}, {0},												COMMON},
	{8,  102, FREQ_4G89,   BW_10, {11, 13, 15, 17, 19, 0}, {0},															COMMON},
	{9,  102, FREQ_4G89,   BW_10, {11, 13, 15, 17, 19, 0}, {0},														COMMON},
	{10, 101, FREQ_4G85,   BW_20, {21, 25, 0}, {0},																		COMMON},
	{11, 101, FREQ_4G85,   BW_20, {21, 25, 0}, {0},																		COMMON},
	{12,  81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0}, {0},											LICENSEEXEMPTBEHAVIOR},
	{13,  94, FREQ_3G00,   BW_20, {133, 137, 0}, {0},																	CCA_EDBEHAVIOR},
	{14,  95, FREQ_3G00,   BW_10, {132, 134, 136, 138, 0}, {0},															CCA_EDBEHAVIOR},
	{15,  96, FREQ_3G0025, BW_5,  {131, 132, 133, 134, 135, 136, 137, 138, 0}, {0},									CCA_EDBEHAVIOR},
	{16,   0, FREQ_5G0025, BW_5,  {170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},	ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{18,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0}, {0},				ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{22, 116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{23, 119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{24, 122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0},	{0},											DFS_50_100_BEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{25, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{26, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																	LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{27, 117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{28, 120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																	PRIMARYCHANNELUPPERBEHAVIOR},
	{29, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0}, {0},												NOMADICBEHAVIOR | DFS_50_100_BEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{30, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0}, {0},																	NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{31, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0},	{0},																LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{32,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 0}, {0},														LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{33,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 0}, {0},														LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{128, 128, FREQ_5G00,  BW_80, {0}, {42, 58, 106, 122, 138, 155, 171, 0},													USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,  BW_160, {0}, {50, 114, 163, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,  BW_80, {0}, {42, 58, 106, 122, 138, 155, 171, 0},													PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{131, 131, FREQ_5G95,  BW_20, {0}, {1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
										41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
										81, 85, 89, 93, 97, 101, 105, 109, 113, 117,
										121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
										161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
										201, 205, 209, 213, 217, 221, 225, 229, 233, 0},								COMMON},
	{132, 132, FREQ_5G95,  BW_40, {0}, {3, 11, 19, 27, 35, 43, 51, 59, 67, 75,
										83, 91, 99, 107, 115, 123, 131, 139, 147, 155,
										163, 171, 179, 187, 195, 203, 211, 219, 227, 0},								COMMON},
	{133, 133, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151,
										167, 183, 199, 215, 0},															COMMON},
	{134, 134, FREQ_5G95,  BW_160, {0}, {15, 47, 79, 111, 143, 175, 207, 0},											COMMON},
	{135, 135, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151, 167, 183, 199, 215, 0},				PLUS_80},
	{136, 136, FREQ_5G925,  BW_20, {0}, {2},											COMMON},
#ifdef DOT11_EHT_BE
	{137, 137, FREQ_5G95,  BW_320, {0}, {31, 63, 95, 127, 159, 191, 0},											COMMON},
#endif
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};


/*
	Table E-2?Operating classes in Europe (11AC)
*/
REG_CLASS_HE_EHT reg_class_he_ce[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																NOMADICBEHAVIOR},
	{3,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						COMMON},
	{4,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{5,  116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{6,  119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{7,  122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 0}, {0},													PRIMARYCHANNELLOWERBEHAVIOR},
	{8,  117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{9,  120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{10, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 0}, {0},													PRIMARYCHANNELUPPERBEHAVIOR},
	{11,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{12,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{13,   0, FREQ_5G0025, BW_5,  {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{14,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{15,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0}, {0},					ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{16,   0, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17, 125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 0}, {0},												COMMON},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 114, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{131, 131, FREQ_5G95,  BW_20, {0}, {1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
										41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
										81, 85, 89, 93, 0},							COMMON},
	{132, 132, FREQ_5G95,  BW_40, {0}, {3, 11, 19, 27, 35, 43, 51, 59, 67, 75,
										83, 91, 0},							COMMON},
	{133, 133, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 0},														COMMON},
	{134, 134, FREQ_5G95,  BW_160, {0}, {15, 47, 79, 0},											COMMON},
	{135, 135, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 0},				PLUS_80},
	{136, 136, FREQ_5G925,	BW_20, {0}, {2},											COMMON},
#ifdef DOT11_EHT_BE
	{137, 137, FREQ_5G95,  BW_320, {0}, {31, 63, 0},											COMMON},
#endif
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */

};

REG_CLASS_HE_EHT reg_class_he_ce_support_ch144[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																COMMON},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																NOMADICBEHAVIOR},
	{3,  121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0}, {0},						COMMON},
	{4,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{5,  116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{6,  119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{7,  122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0}, {0},													PRIMARYCHANNELLOWERBEHAVIOR},
	{8,  117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{9,  120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{10, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0}, {0},													PRIMARYCHANNELUPPERBEHAVIOR},
	{11,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{12,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{13,   0, FREQ_5G0025, BW_5,  {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{14,   0, FREQ_5G00,   BW_10, {171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 0}, {0},		ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{15,   0, FREQ_5G00,   BW_20, {172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 0}, {0},					ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{16,   0, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						ITS_NONMOBILE_OPERATIONS | ITS_MOBILE_OPERATIONS},
	{17, 125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 169, 0}, {0},												COMMON},
	{25, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{26, 126, FREQ_5G00,   BW_40, {149, 157, 165, 173, 0}, {0},																LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{30, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0}, {0},																NOMADICBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{31, 127, FREQ_5G00,   BW_40, {153, 161, 169, 177, 0}, {0},															LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 138, 155, 0},															USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 114, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{131, 131, FREQ_5G95,  BW_20, {0}, {1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
										41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
										81, 85, 89, 93, 0},								COMMON},
	{132, 132, FREQ_5G95,  BW_40, {0}, {3, 11, 19, 27, 35, 43, 51, 59, 67, 75,
										83, 91, 0},								COMMON},
	{133, 133, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 0},														COMMON},
	{134, 134, FREQ_5G95,  BW_160, {0}, {15, 47, 79, 0},											COMMON},
	{135, 135, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 0},				PLUS_80},
	{136, 136, FREQ_5G925,  BW_20, {0}, {2},											COMMON},
#ifdef DOT11_EHT_BE
	{137, 137, FREQ_5G95,  BW_320, {0}, {31, 63, 0},											COMMON},
#endif
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};

/*
	Table E-3?Operating classes in Japan (11N)
*/
REG_CLASS_HE_EHT reg_class_he_jp[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {34, 36, 38, 40, 42, 44, 46, 48, 0}, {0},											COMMON},
	{8,  109, FREQ_4G00,   BW_20, {184, 188, 192, 196, 0}, {0},														COMMON},
	{11, 109, FREQ_4G00,   BW_20, {184, 188, 192, 196, 0}, {0},														COMMON},
	{17, 110, FREQ_4G00,   BW_10, {183, 184, 185, 187, 188, 189, 0}, {0},											COMMON},
	{20, 110, FREQ_4G00,   BW_10, {183, 184, 185, 187, 188, 189, 0}, {0},											COMMON},
	{25, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0}, {0},									COMMON},
	{26, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0}, {0},									COMMON},
	{29, 111, FREQ_4G0025, BW_5,  {182, 183, 184, 185, 186, 187, 188, 189, 0}, {0},									COMMON},
	{30,  81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{31,  82, FREQ_2G414,  BW_25, {14, 0}, {0},																		LICENSEEXEMPTBEHAVIOR},
	{32, 118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},															COMMON},
	{33, 118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},															COMMON},
	{34, 121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 0}, {0},						DFS_50_100_BEHAVIOR},
	{36, 116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{37, 119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR},
	{39, 122, FREQ_5G00,   BW_40, {100, 108, 116, 124, 132, 140, 0}, {0},													PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{41, 117, FREQ_5G00,   BW_40, {40, 48, 0}, {0},																	PRIMARYCHANNELUPPERBEHAVIOR},
	{42, 120, FREQ_5G00,   BW_40, {56, 64, 0}, {0},																		PRIMARYCHANNELUPPERBEHAVIOR},
	{44, 123, FREQ_5G00,   BW_40, {104, 112, 120, 128, 136, 144, 0}, {0},													PRIMARYCHANNELUPPERBEHAVIOR | DFS_50_100_BEHAVIOR},
	{46, 104, FREQ_4G00,   BW_40, {184, 192, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR},
	{51, 105, FREQ_4G00,   BW_40, {188, 196, 0}, {0},																	PRIMARYCHANNELUPPERBEHAVIOR},
	{56,  83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{57,  84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{58, 121, FREQ_5G00,   BW_20, {100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 0}, {0},						NOMADICBEHAVIOR | LICENSEEXEMPTBEHAVIOR},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 138, 0},												USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 114, 0},																	USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 106, 122, 0},															PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{131, 131, FREQ_5G95,  BW_20, {0}, {1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
										41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
										81, 85, 89, 93, 97, 101, 105, 109, 113, 117,
										121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
										161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
										201, 205, 209, 213, 217, 221, 225, 229, 233, 0},								COMMON},
	{132, 132, FREQ_5G95,  BW_40, {0}, {3, 11, 19, 27, 35, 43, 51, 59, 67, 75,
										83, 91, 99, 107, 115, 123, 131, 139, 147, 155,
										163, 171, 179, 187, 195, 203, 211, 219, 227, 0},								COMMON},
	{133, 133, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151,
										167, 183, 199, 215, 0},														COMMON},
	{134, 134, FREQ_5G95,  BW_160, {0}, {15, 47, 79, 111, 143, 175, 207, 0},											COMMON},
	{135, 135, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151, 167, 183, 199, 215, 0},				PLUS_80},
	{136, 136, FREQ_5G925,  BW_20, {0}, {2},											COMMON},
#ifdef DOT11_EHT_BE
	{137, 137, FREQ_5G95,  BW_320, {0}, {31, 63, 95, 127, 159, 191, 0},											COMMON},
#endif
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};

REG_CLASS_HE_EHT reg_class_he_cn[] = {
	{0, 0, 0, 0, {0}, {0}, 0},			/* Invlid entry */
	{1,  115, FREQ_5G00,   BW_20, {36, 40, 44, 48, 0}, {0},																USEEIRPFORVHTTXPOWENV},
	{2,  118, FREQ_5G00,   BW_20, {52, 56, 60, 64, 0}, {0},																DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{3,  125, FREQ_5G00,   BW_20, {149, 153, 157, 161, 165, 0}, {0},                                                    USEEIRPFORVHTTXPOWENV},
	{4,  116, FREQ_5G00,   BW_40, {36, 44, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{5,  119, FREQ_5G00,   BW_40, {52, 60, 0}, {0},																		PRIMARYCHANNELLOWERBEHAVIOR | DFS_50_100_BEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{6,  126, FREQ_5G00,   BW_40, {149, 157, 0}, {0},																	PRIMARYCHANNELLOWERBEHAVIOR | USEEIRPFORVHTTXPOWENV},
	{7,   81, FREQ_2G407,  BW_25, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},									LICENSEEXEMPTBEHAVIOR},
	{8,   83, FREQ_2G407,  BW_40, {1, 2, 3, 4, 5, 6, 7, 8, 9, 0}, {0},													LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELLOWERBEHAVIOR},
	{9,   84, FREQ_2G407,  BW_40, {5, 6, 7, 8, 9, 10, 11, 12, 13, 0}, {0},												LICENSEEXEMPTBEHAVIOR | PRIMARYCHANNELUPPERBEHAVIOR},
	{128, 128, FREQ_5G00,   BW_80, {0}, {42, 58, 155, 0},																USEEIRPFORVHTTXPOWENV},
	{129, 129, FREQ_5G00,   BW_160, {0}, {50, 0},																			USEEIRPFORVHTTXPOWENV},
	{130, 130, FREQ_5G00,   BW_80, {0}, {42, 58, 155, 0},																PLUS_80 | USEEIRPFORVHTTXPOWENV},
	{131, 131, FREQ_5G95,  BW_20, {0}, {1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
										41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
										81, 85, 89, 93, 97, 101, 105, 109, 113, 117,
										121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
										161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
										201, 205, 209, 213, 217, 221, 225, 229, 233, 0},								COMMON},
	{132, 132, FREQ_5G95,  BW_40, {0}, {3, 11, 19, 27, 35, 43, 51, 59, 67, 75,
										83, 91, 99, 107, 115, 123, 131, 139, 147, 155,
										163, 171, 179, 187, 195, 203, 211, 219, 227, 0},								COMMON},
	{133, 133, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151,
										167, 183, 199, 215, 0},														COMMON},
	{134, 134, FREQ_5G95,  BW_160, {0}, {15, 47, 79, 111, 143, 175, 207, 0},											COMMON},
	{135, 135, FREQ_5G95,  BW_80, {0}, {7, 23, 39, 55, 71, 87, 103, 119, 135, 151, 167, 183, 199, 215, 0},				PLUS_80},
	{136, 136, FREQ_5G925,  BW_20, {0}, {2},											COMMON},
#ifdef DOT11_EHT_BE
	{137, 137, FREQ_5G95,  BW_320, {0}, {31, 63, 95, 127, 159, 191, 0},											COMMON},
#endif
	{0, 0, 0, 0, {0}, {0}, 0}			/* end */
};

#endif /* DOT11_VHT_AC */


BOOLEAN is_channel_in_channelset(UCHAR *ChannelSet, UCHAR Channel)
{
	int index = 0;

	while (ChannelSet[index] != 0) {
		/* TODO: Need to consider behavior limits set!! */
		if (ChannelSet[index] == Channel)
			return TRUE;

		index++;
	}

	return FALSE;
}

UCHAR get_channel_set_num(UCHAR *ChannelSet)
{
	int index = 0;

	if (ChannelSet == NULL)
		return 0;

	while (ChannelSet[index] != 0)
		index++;

	return index;
}

PVOID get_reg_table_by_country(UCHAR *CountryCode, USHORT PhyMode)
{
	UCHAR reg_region;
	PVOID reg_class_table = NULL;
	reg_region = GetCountryRegionFromCountryCode(CountryCode);

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(PhyMode)) {
		switch (reg_region) {
		case CE:
			if ((strncmp((RTMP_STRING *) CountryCode, "GB", 2) != 0)
				/*Allow United Kingdom by latest regulations:vns-203083*/
				&& (strncmp((RTMP_STRING *) CountryCode, "AU", 2) != 0)
				/*Allow Australia*/)
				reg_class_table = (PVOID)reg_class_he_ce;
			else
				reg_class_table = (PVOID)reg_class_he_ce_support_ch144;
			break;
		case JAP:
			reg_class_table = (PVOID)reg_class_he_jp;
			break;
		case CHN:
			reg_class_table = (PVOID)reg_class_he_cn;
			break;
		case FCC:
		default:
			reg_class_table = (PVOID)reg_class_he_fcc;
			break;
		}
	} else
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(PhyMode)) {
		switch (reg_region) {
		case CE:
			if ((strncmp((RTMP_STRING *) CountryCode, "GB", 2) != 0)
				/*Allow United Kingdom by latest regulations:vns-203083*/
				&& (strncmp((RTMP_STRING *) CountryCode, "AU", 2) != 0)
				/*Allow Australia*/)
				reg_class_table = (PVOID)reg_class_he_ce;
			else
				reg_class_table = (PVOID)reg_class_he_ce_support_ch144;
			break;
		case JAP:
			reg_class_table = (PVOID)reg_class_he_jp;
			break;
		case CHN:
			reg_class_table = (PVOID)reg_class_he_cn;
			break;
		case FCC:
		default:
			reg_class_table = (PVOID)reg_class_he_fcc;
			break;
		}
	} else
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
	if (WMODE_CAP_AC(PhyMode)) {
		switch (reg_region) {
		case CE:
			if ((strncmp((RTMP_STRING *) CountryCode, "GB", 2) != 0)
				/*Allow United Kingdom by latest regulations:vns-203083*/
				&& (strncmp((RTMP_STRING *) CountryCode, "AU", 2) != 0)
				/*Allow Australia*/)
				reg_class_table = (PVOID)reg_class_vht_ce;
			else
				reg_class_table = (PVOID)reg_class_vht_ce_support_ch144;
			break;

		case JAP:
			reg_class_table = (PVOID)reg_class_vht_jp;
			break;

		case CHN:
			reg_class_table = (PVOID)reg_class_vht_cn;
			break;

		case FCC:
		default:
			reg_class_table = (PVOID)reg_class_vht_fcc;
			break;
		}
	} else
#endif /* DOT11_VHT_AC */
	{
		switch (reg_region) {
		case CE:
			if ((strncmp((RTMP_STRING *) CountryCode, "GB", 2) != 0)
				/*Allow United Kingdom by latest regulations:vns-203083*/
				&& (strncmp((RTMP_STRING *) CountryCode, "AU", 2) != 0)
				/*Allow Australia*/)
				reg_class_table = (PVOID)reg_class_ce;
			else
				reg_class_table = (PVOID)reg_class_ce_support_ch144;
			break;

		case JAP:
			reg_class_table = (PVOID)reg_class_jp;
			break;

		case CHN:
			reg_class_table = (PVOID)reg_class_cn;
			break;

		case FCC:
		default:
			reg_class_table = (PVOID)reg_class_fcc;
			break;
		}
	}
	return reg_class_table;
}

VOID *get_reg_class_tab_by_reg_class(
	IN RTMP_ADAPTER * pAd,
	IN UINT8 RegulatoryClass,
	IN USHORT PhyMode)
{
	int i = 1; /* skip Invlid entry */

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_he) {
			do {
				/* find  channel_set */
				if ((reg_class_he[i].reg_class == RegulatoryClass)
					|| (reg_class_he[i].global_class == RegulatoryClass)) {
						return &reg_class_he[i];
				}
				i++;
			} while (reg_class_he[i].reg_class != 0);
		}
	} else
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_he) {
			do {
				/* find  channel_set */
				if ((reg_class_he[i].reg_class == RegulatoryClass)
					|| (reg_class_he[i].global_class == RegulatoryClass)) {
						return &reg_class_he[i];
				}
				i++;
			} while (reg_class_he[i].reg_class != 0);
		}
	} else
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
	if (WMODE_CAP_AC(PhyMode)) {
		PREG_CLASS_VHT reg_class_vht = (PREG_CLASS_VHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_vht) {
			do {
				/* find  channel_set */
				if ((reg_class_vht[i].reg_class == RegulatoryClass)
					|| (reg_class_vht[i].global_class == RegulatoryClass)) {
					return &reg_class_vht[i];
				}

				i++;
			} while (reg_class_vht[i].reg_class != 0);
		}
	} else
#endif /* DOT11_VHT_AC */
	{
		PREG_CLASS reg_class = (PREG_CLASS)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class) {
			do {
				/* find  channel_set */
				if ((reg_class[i].reg_class == RegulatoryClass)
					|| (reg_class[i].global_class == RegulatoryClass)) {
					return &reg_class[i];
				}
				i++;
			} while (reg_class[i].reg_class != 0);
		}
	}

	return NULL;
}

PUCHAR get_channelset_by_reg_class(
	IN RTMP_ADAPTER * pAd,
	IN UINT8 RegulatoryClass,
	IN USHORT PhyMode)
{
	PUCHAR channelset = NULL;
	VOID *reg_class_tab = NULL;

	reg_class_tab = get_reg_class_tab_by_reg_class(pAd, RegulatoryClass, PhyMode);

	if (reg_class_tab == NULL)
		return NULL;

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)reg_class_tab;

		if (RegulatoryClass > 127 && RegulatoryClass < 138)
			channelset = reg_class_he->center_freq;
		else
			channelset = reg_class_he->channel_set;
	} else
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)reg_class_tab;

		if (RegulatoryClass > 127 && RegulatoryClass < 138)// 136
			channelset = reg_class_he->center_freq;
		else
			channelset = reg_class_he->channel_set;
	} else
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
	if (WMODE_CAP_AC(PhyMode)) {
		PREG_CLASS_VHT reg_class_vht = (PREG_CLASS_VHT)reg_class_tab;

		if (RegulatoryClass > 127 && RegulatoryClass < 131) /* reference operating classes table (802.11) */
			channelset = reg_class_vht->center_freq;
		else
			channelset = reg_class_vht->channel_set;
	} else
#endif /* DOT11_VHT_AC */
	{
		PREG_CLASS reg_class = (PREG_CLASS)reg_class_tab;

		/* find  channel_set */
		channelset = reg_class->channel_set;
	}
	return channelset;
}

BOOLEAN get_spacing_by_reg_class(
	IN RTMP_ADAPTER * pAd,
	IN UINT8 RegulatoryClass,
	IN USHORT PhyMode,
	OUT UCHAR *spacing)
{
	VOID *reg_class_tab = NULL;
	PREG_CLASS reg_class = NULL;
	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
		"RegulatoryClass = %d!\n", RegulatoryClass);

	if (RegulatoryClass == 0) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"Invalid operation class!\n");
		return FALSE;
	}

	reg_class_tab = get_reg_class_tab_by_reg_class(pAd, RegulatoryClass, PhyMode);

	if (reg_class_tab == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR,
		"get reg_class failed!\n");
		return FALSE;
	}

	reg_class = (PREG_CLASS)reg_class_tab;

	/* find spacing */
	*spacing = reg_class->spacing; /* all the spacing in the same position in n/ac/ax structure */

	return TRUE;;
}


BOOLEAN is_channel_in_channelset_by_reg_class(
	IN RTMP_ADAPTER * pAd,
	IN UINT8 RegulatoryClass,
	IN USHORT PhyMode,
	IN UCHAR Channel)
{
	PUCHAR channelset = NULL;

	channelset = get_channelset_by_reg_class(pAd, RegulatoryClass, PhyMode);

	if (channelset == NULL)
		return FALSE;

	return is_channel_in_channelset(channelset, Channel);
}


#ifdef DFS_CAC_R2
int get_cac_mode (IN PRTMP_ADAPTER pAd, IN PDFS_PARAM pDfsParam, struct wifi_dev *wdev)
{
	if (pAd->CommonCfg.DfsParameter.bDfsEnable) {
		if (pDfsParam->bDedicatedZeroWaitSupport == TRUE
		&& hc_get_hw_band_idx(pAd) == pDfsParam->ZeroWaitBandidx)
			return DEDICATED_CAC;
		else if (pDfsParam->bDedicatedZeroWaitSupport &&
			hc_get_hw_band_idx(pAd) == pDfsParam->ZeroWaitBandidx &&
			wlan_config_get_rx_stream(wdev) == RX_MAX_STREAM &&
			wlan_config_get_tx_stream(wdev) == TX_MAX_STREAM)
			return REDUCED_MIMO_CAC;

		return CONTINUOUS_CAC;
	}
	return DISABLE_CAC;
}

static bool get_global_opclass_channel_range(
	UINT16 opclass,
	UINT16 ch,
	UINT16 *min_ch,
	UINT16 *max_ch,
	UINT16 *ch_cnt,
	UINT16 *bw)
{
	bool is_center_ch = FALSE;

	if (IS_6G_OPCLASS(opclass)) {
		if (IS_320M(opclass)) {
			*ch_cnt = 16;
			*bw = BW_320;
			is_center_ch = TRUE;
		} else if (IS_160M_6G(opclass)) {
			*ch_cnt = 8;
			*bw = BW_160;
			is_center_ch = TRUE;
		} else if (IS_80M_6G(opclass) || IS_8080_6G(opclass)) {
			*ch_cnt = 4;
			*bw = BW_80;
			is_center_ch = TRUE;
		} else if (IS_40M_6G(opclass)) {
			*ch_cnt = 2;
			*bw = BW_40;
			is_center_ch = TRUE;
		} else if (IS_20M_6G(opclass)) {
			*ch_cnt = 1;
			*min_ch = *max_ch = ch;
		}
	} else if (IS_5G_OPCLASS(opclass)) {
		if (IS_160M_5G(opclass)) {
			*ch_cnt = 8;
			*bw = BW_160;
			is_center_ch = TRUE;
		} else if (IS_80M_5G(opclass) || IS_8080_5G(opclass)) {
			*ch_cnt = 4;
			*bw = BW_80;
			is_center_ch = TRUE;
		} else if (IS_40M_5G_LOWER(opclass)) {
			*ch_cnt = 2;
			*min_ch = ch;
			*max_ch = ch + 4;
			*bw = BW_40;
		} else if (IS_40M_5G_UPPER(opclass)) {
			*ch_cnt = 2;
			*min_ch = ch - 4;
			*max_ch = ch;
			*bw = BW_40;
		} else if (IS_20M_5G(opclass)) {
			*ch_cnt = 1;
			*min_ch = *max_ch = ch;
			*bw = BW_20;
		}
	} else if (IS_2G_OPCLASS(opclass)) {
		if (IS_40M_2G_LOWER(opclass)) {
			*ch_cnt = 5;
			*min_ch = ch;
			*max_ch = ch + 4;
			*bw = BW_40;
		} else if (IS_40M_2G_UPPER(opclass)) {
			*ch_cnt = 5;
			*min_ch = ch - 4;
			*max_ch = ch;
			*bw = BW_40;
		} else if (IS_20M_2G(opclass)) {
			*ch_cnt = 1;
			*min_ch = *max_ch = ch;
			*bw = BW_20;
		}
	}

	if (*ch_cnt == 0)
		return FALSE;

	if (is_center_ch) {
		*min_ch = ch - (*ch_cnt / 2) * 4 + 2;
		*max_ch = ch + (*ch_cnt / 2) * 4 - 2;
	}

	return TRUE;
}

struct REG_CLASS_GLOBAL *get_global_reg_class(
	UINT16 opclass,
	UINT16 ch)
{
	struct REG_CLASS_GLOBAL *reg_class = NULL;
	int i = 0, class_idx = 0;
	bool is_ch_in_opclass = FALSE;

	do {
		if (reg_class_global[class_idx].global_class == opclass) {
			reg_class = &reg_class_global[class_idx];
			i = 0;
			while (reg_class->channel_set[i] != 0) {
				if (ch == reg_class->channel_set[i])  {
					is_ch_in_opclass = TRUE;
					break;
				}
				i++;
			}
			i = 0;
			while (reg_class->center_ch_set[i] != 0) {
				if (ch == reg_class->center_ch_set[i])  {
					is_ch_in_opclass = TRUE;
					break;
				}
				i++;
			}
			break;
		}

		class_idx++;
	} while (reg_class_global[class_idx].global_class != 0);

	if (reg_class == NULL || !is_ch_in_opclass)
		return NULL;

	return reg_class;
}

bool is_global_opclass_for_wdev(
	struct wifi_dev *wdev,
	UINT16 opclass)
{
	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_24G &&
		IS_2G_OPCLASS(opclass))
		return TRUE;

	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_5G &&
		IS_5G_OPCLASS(opclass))
		return TRUE;

	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_6G &&
		IS_6G_OPCLASS(opclass))
		return TRUE;

	return FALSE;
}

bool check_global_opclass_channel_available(
	IN PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev,
	UINT16 opclass,
	UINT16 ch)
{
	CHANNEL_CTRL *pChCtrl = NULL;
	struct REG_CLASS_GLOBAL *reg_class = NULL;
	UINT16 ch_cnt = 0;
	UINT16 min_ch = 0, max_ch = 0;
	UINT16 bw = 0;
	int i = 0;
	UINT8 match_cnt = 0;
	UINT32 exp_flag = 0;
	bool chk_flag = TRUE;
	UCHAR match_ch_list[20] = {0};
#ifdef CONFIG_MAP_SUPPORT
	UINT16 current_opr_bw = 0, opclass_bw = 0;
#endif

	if (ch == 0)
		return FALSE;

	if (pAd->hdev_ctrl == NULL)
		return FALSE;

	pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
	if (pChCtrl == NULL)
		return FALSE;

	if (!is_global_opclass_for_wdev(wdev, opclass)) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
			"op_class=%d is not match current band\n", opclass);
		return FALSE;
	}

	reg_class = get_global_reg_class(opclass, ch);
	if (reg_class == NULL) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
			"get_global_reg_class Failed op_class=%d ch=%d\n", opclass, ch);
		return FALSE;
	}

	/* The check currently is not depend on global_reg_class table. */
	if (!get_global_opclass_channel_range(opclass, ch, &min_ch, &max_ch, &ch_cnt, &bw)) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
			"get_global_opclass_channel_range Failed op_class=%d ch=%d\n", opclass, ch);
		return FALSE;
	}

#ifdef CONFIG_MAP_SUPPORT
	current_opr_bw = wlan_config_get_eht_bw(wdev);
	opclass_bw = bw;
#ifdef DOT11_EHT_BE
	if (opclass_bw == BW_320)
		opclass_bw = EHT_BW_320;
#endif
	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
		"check bw op_class=%d ch=%d bw=%d %d\n", opclass, ch, opclass_bw, current_opr_bw);
	if (IS_MAP_TURNKEY_ENABLE(pAd) && opclass_bw > current_opr_bw)
		return FALSE;
#endif
	switch (bw) {
	case BW_40:
		exp_flag = CHANNEL_40M_CAP;
		break;
	case BW_80:
		exp_flag = CHANNEL_80M_CAP;
		break;
	case BW_160:
		exp_flag = CHANNEL_160M_CAP;
		break;
	case BW_320:
		exp_flag = CHANNEL_320M_CAP;
		break;
	default:
		chk_flag = FALSE;
		break;
	}

	for (i = 0; i < pChCtrl->ChListNum; i++) {
		if (!(pChCtrl->ChList[i].Channel >= min_ch &&
			pChCtrl->ChList[i].Channel <= max_ch))
			continue;

		if (chk_flag) {
			/* FLAGS BW cap is already concern enable/capability/NOP/ch_prio */
			if (!(pChCtrl->ChList[i].Flags | exp_flag))
				continue;

			match_ch_list[match_cnt] = pChCtrl->ChList[i].Channel;
			match_cnt += 1;

		} else {
			match_ch_list[match_cnt] = pChCtrl->ChList[i].Channel;
			match_cnt += 1;
			break;
		}
	}
	match_ch_list[match_cnt] = 0;

	if (match_cnt == ch_cnt)
		return TRUE;

	return FALSE;
}

UCHAR get_global_opclass_channel_priority(
	IN PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev,
	UINT16 opclass,
	UINT16 ch)
{
	CHANNEL_CTRL *pChCtrl = NULL;
	struct REG_CLASS_GLOBAL *reg_class = NULL;
	UINT16 ch_cnt = 0;
	UINT16 min_ch = 0, max_ch = 0;
	UINT16 bw = 0;
	int i = 0;
	UCHAR max_prio = 0, ch_prio = 0;

	if (ch == 0)
		return 0;

	if (pAd->hdev_ctrl == NULL)
		return 0;

	pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
	if (pChCtrl == NULL)
		return 0;

	if (!is_global_opclass_for_wdev(wdev, opclass)) {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_WARN,
			"op_class=%d is not match current band\n", opclass);
		return FALSE;
	}

	reg_class = get_global_reg_class(opclass, ch);
	if (reg_class == NULL)
		return FALSE;

	if (!get_global_opclass_channel_range(opclass, ch, &min_ch, &max_ch, &ch_cnt, &bw))
		return 0;

	for (i = 0; i < pChCtrl->ChListNum; i++) {
		if (!(pChCtrl->ChList[i].Channel >= min_ch &&
			pChCtrl->ChList[i].Channel <= max_ch))
			continue;

		ch_prio = pChCtrl->ChList[i].Priority;
		if (ch_prio == 0xFF)
			max_prio = 0x0E;
		else if (ch_prio > 0 && ch_prio < 15 &&
				 (max_prio < ch_prio || max_prio == 0))
			max_prio = ch_prio;
	}

	return max_prio;
}

BOOLEAN wapp_CACAbandCheck_central_channel(IN PRTMP_ADAPTER pAd,
					   UINT16 opclass,
					   UINT16 center_ch, struct wifi_dev *wdev)
{
	return check_global_opclass_channel_available(pAd, wdev, opclass, center_ch);

}

void wapp_get_cac_cap(IN PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev,
	struct cac_capability_lib *cac_cap)
{
	/*TODO: Raghav : Need to re-write this code for harrier.*/
	UCHAR i = 0, j = 0, k = 0, l = 0, m = 0, dfs_ch = 0, channel = 0;
	PDFS_PARAM pDfsParam = &pAd->CommonCfg.DfsParameter;
	BOOLEAN ret = FALSE;
	PCHANNEL_CTRL pChCtrl = NULL;

#if defined(DOT11_HE_AX) || defined(DOT11_EHt_BE)
	if ((WMODE_CAP_AX(wdev->PhyMode)) || (WMODE_CAP_BE(wdev->PhyMode))) {
		PREG_CLASS_HE_EHT reg_class = (PREG_CLASS_HE_EHT)get_reg_table_by_country(
			pAd->CommonCfg.CountryCode, wdev->PhyMode);
		if (reg_class) {
			do {
				/* find channel_set */
				if (reg_class[i].start_freq == FREQ_5G00) {
					if ((reg_class[i].spacing == BW_80) ||
						(reg_class[i].spacing == BW_160))
						channel = reg_class[i].center_freq[j];
					else
						channel = reg_class[i].channel_set[j];

					for (j = 0; channel != 0 && (k < MAX_CAC_CHLIST_NUM);) {
#ifdef DOT11_VHT_AC
						if (reg_class[i].spacing == BW_80) {
							ret = RadarChannelCheck(pAd,
								(reg_class[i].center_freq[j] - 2));
							if (ret == TRUE) {
								ret = wapp_CACAbandCheck_central_channel(pAd,
											reg_class[i].global_class, channel, wdev);
							}
						} else if (reg_class[i].spacing == BW_160) {
							ret = RadarChannelCheckWrapper160(pAd, reg_class[i].center_freq[j] - 14);
							if (ret == TRUE) {
								ret = wapp_CACAbandCheck_central_channel(pAd,
											reg_class[i].global_class, channel, wdev);
							}
						} else if (reg_class[i].spacing == BW_40) {
							if ((RadarChannelCheck(pAd, reg_class[i].channel_set[j]) &&
								RadarChannelCheck(pAd, reg_class[i].channel_set[j] + 4) &&
								(reg_class[i].behavior_limit_set & PRIMARYCHANNELLOWERBEHAVIOR)) ||
								(RadarChannelCheck(pAd, reg_class[i].channel_set[j]) &&
								RadarChannelCheck(pAd, reg_class[i].channel_set[j] - 4) &&
								(reg_class[i].behavior_limit_set & PRIMARYCHANNELUPPERBEHAVIOR)))
								ret = TRUE;
							else
								ret = FALSE;
						} else
#endif
						{
							ret = RadarChannelCheck(pAd,
							reg_class[i].channel_set[j]);
						}

						if (ret == TRUE) {
							dfs_ch = TRUE;
							cac_cap->opcap[l].op_class = reg_class[i].global_class;
							cac_cap->opcap[l].ch_num++;
							if ((reg_class[i].spacing == BW_80) ||
								(reg_class[i].spacing == BW_160))
								cac_cap->opcap[l].ch_list[k] =
									reg_class[i].center_freq[j];
							else
								cac_cap->opcap[l].ch_list[k] =
									reg_class[i].channel_set[j];

							if ((pAd->CommonCfg.RDDurRegion == CE) &&
								DfsCacRestrictBandForCentralCh(pAd, wdev,
								reg_class[i].spacing,
								cac_cap->opcap[l].ch_list[k], 0)) {
								cac_cap->opcap[l].cac_time[k] = 650;
							} else
								cac_cap->opcap[l].cac_time[k] = 65;

							pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
							for (m = 0; m < pChCtrl->ChListNum; m++) {
								if (reg_class[i].spacing == BW_80 || reg_class[i].spacing == BW_160) {
									if (pChCtrl->ChList[m].Channel ==
										reg_class[i].center_freq[j] - 2) {
										cac_cap->opcap[l].non_occupancy_remain[k] =
											(pChCtrl->ChList[m].NonOccupancy == 0) ?
											pChCtrl->ChList[m].NOPSaveForClear :
											pChCtrl->ChList[m].NonOccupancy;
										break;
									}
								} else {
									if (pChCtrl->ChList[m].Channel ==
										reg_class[i].channel_set[j]) {
										cac_cap->opcap[l].non_occupancy_remain[k] =
											(pChCtrl->ChList[m].NonOccupancy == 0) ?
											pChCtrl->ChList[m].NOPSaveForClear :
											pChCtrl->ChList[m].NonOccupancy;
										break;
									}
								}
							}
							k++;
						}
						j++;
						if ((reg_class[i].spacing == BW_80) ||
							(reg_class[i].spacing == BW_160))
							channel = reg_class[i].center_freq[j];
						else
							channel = reg_class[i].channel_set[j];
					}
					k = 0;
					j = 0;
					if (dfs_ch == TRUE) {
						cac_cap->op_class_num += 1;
						l++;
					}
					dfs_ch = FALSE;
				}
				i++;
			} while (reg_class[i].reg_class != 0 && cac_cap->op_class_num < MAX_CAC_OPCAP_NUM);
		}
	} else
#endif
	if ((WMODE_CAP_AC(wdev->PhyMode))) {
		PREG_CLASS_VHT reg_class = (PREG_CLASS_VHT)get_reg_table_by_country(
			pAd->CommonCfg.CountryCode, wdev->PhyMode);
		if (reg_class) {
			do {
			/* find  channel_set */
				if (reg_class[i].start_freq == FREQ_5G00) {
					if ((reg_class[i].spacing == BW_80) ||
						(reg_class[i].spacing == BW_160))
						channel = reg_class[i].center_freq[j];
					else
						channel = reg_class[i].channel_set[j];
				for (j = 0; channel != 0 && (k < MAX_CAC_CHLIST_NUM);) {
#ifdef DOT11_VHT_AC
					if (reg_class[i].spacing == BW_80) {
						ret = RadarChannelCheck(pAd,
							(reg_class[i].center_freq[j] - 2));
						if (ret == TRUE) {
							ret = wapp_CACAbandCheck_central_channel(pAd,
										reg_class[i].global_class, channel, wdev);
						}
					} else if (reg_class[i].spacing == BW_160) {
						ret = RadarChannelCheckWrapper160(pAd, reg_class[i].center_freq[j] - 14);
						if (ret == TRUE) {
							ret = wapp_CACAbandCheck_central_channel(pAd,
										reg_class[i].global_class, channel, wdev);
						}
					} else if (reg_class[i].spacing == BW_40) {
						if ((RadarChannelCheck(pAd, reg_class[i].channel_set[j]) &&
							RadarChannelCheck(pAd, reg_class[i].channel_set[j] + 4) &&
							(reg_class[i].behavior_limit_set & PRIMARYCHANNELLOWERBEHAVIOR)) ||
							(RadarChannelCheck(pAd, reg_class[i].channel_set[j]) &&
							RadarChannelCheck(pAd, reg_class[i].channel_set[j] - 4) &&
							(reg_class[i].behavior_limit_set & PRIMARYCHANNELUPPERBEHAVIOR)))
							ret = TRUE;
						else
							ret = FALSE;
					} else
#endif
					{
						ret = RadarChannelCheck(pAd,
							reg_class[i].channel_set[j]);
					}
					if (ret == TRUE) {
						dfs_ch = TRUE;
						cac_cap->opcap[l].op_class = reg_class[i].global_class;
						cac_cap->opcap[l].ch_num++;
					if ((reg_class[i].spacing == BW_80) ||
						(reg_class[i].spacing == BW_160))
						cac_cap->opcap[l].ch_list[k] =
							reg_class[i].center_freq[j];
					else
						cac_cap->opcap[l].ch_list[k] =
								reg_class[i].channel_set[j];

					if ((pAd->CommonCfg.RDDurRegion == CE) &&
					DfsCacRestrictBandForCentralCh(pAd, wdev,
					reg_class[i].spacing,
					cac_cap->opcap[l].ch_list[k], 0)) {
						cac_cap->opcap[l].cac_time[k] = 650;
					} else
						cac_cap->opcap[l].cac_time[k] = 65;
					pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
					for (m = 0; m < pChCtrl->ChListNum; m++) {
						if (reg_class[i].spacing == BW_80 || reg_class[i].spacing == BW_160) {
							if (pChCtrl->ChList[m].Channel ==
								reg_class[i].center_freq[j] - 2) {
								cac_cap->opcap[l].non_occupancy_remain[k] =
									(pChCtrl->ChList[m].NonOccupancy == 0) ?
									pChCtrl->ChList[m].NOPSaveForClear :
									pChCtrl->ChList[m].NonOccupancy;
								break;
							}
						} else {
							if (pChCtrl->ChList[m].Channel ==
								reg_class[i].channel_set[j]) {
								cac_cap->opcap[l].non_occupancy_remain[k] =
									(pChCtrl->ChList[m].NonOccupancy == 0) ?
									pChCtrl->ChList[m].NOPSaveForClear :
									pChCtrl->ChList[m].NonOccupancy;
								break;
							}
						}
					}
					k++;
					}
					j++;
					if ((reg_class[i].spacing == BW_80) ||
						(reg_class[i].spacing == BW_160))
						channel = reg_class[i].center_freq[j];
					else
						channel = reg_class[i].channel_set[j];
				}
				k = 0;
				j = 0;
				if (dfs_ch == TRUE) {
					cac_cap->op_class_num += 1;
					l++;
				}
				dfs_ch = FALSE;
				}
			i++;
			} while (reg_class[i].reg_class != 0 && cac_cap->op_class_num < MAX_CAC_OPCAP_NUM);
		}
	} else {
		PREG_CLASS reg_class = (PREG_CLASS)get_reg_table_by_country(
			pAd->CommonCfg.CountryCode, wdev->PhyMode);
			do {
				/* find  channel_set */
				if (reg_class[i].start_freq == FREQ_5G00) {
					channel = reg_class[i].channel_set[j];
					for (j = 0; channel != 0 && (k < MAX_CAC_CHLIST_NUM);) {
						if (reg_class[i].spacing == BW_40) {
							if ((RadarChannelCheck(pAd, reg_class[i].channel_set[j]) &&
								RadarChannelCheck(pAd, reg_class[i].channel_set[j] + 4) &&
								(reg_class[i].behavior_limit_set & PRIMARYCHANNELLOWERBEHAVIOR)) ||
								(RadarChannelCheck(pAd, reg_class[i].channel_set[j]) &&
								RadarChannelCheck(pAd, reg_class[i].channel_set[j] - 4) &&
								(reg_class[i].behavior_limit_set & PRIMARYCHANNELUPPERBEHAVIOR)))
								ret = TRUE;
							else
								ret = FALSE;
						} else {
							ret = RadarChannelCheck(pAd,
								reg_class[i].channel_set[j]);
						}
						if (ret == TRUE) {
							dfs_ch = TRUE;
							cac_cap->opcap[l].op_class =
								reg_class[i].global_class;
							cac_cap->opcap[l].ch_num++;
							cac_cap->opcap[l].ch_list[k] =
								reg_class[i].channel_set[j];
						if ((pAd->CommonCfg.RDDurRegion == CE)
							&& DfsCacRestrictBandForCentralCh(pAd, wdev, reg_class[i].spacing,
										cac_cap->opcap[l].ch_list[k], 0)) {
							cac_cap->opcap[l].cac_time[k] = 650;
						} else
							cac_cap->opcap[l].cac_time[k] = 65;
						pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
						for (m = 0; m < pChCtrl->ChListNum; m++) {
							if (pChCtrl->ChList[m].Channel ==
								reg_class[i].channel_set[j]) {
								cac_cap->opcap[l].non_occupancy_remain[k] =
									(pChCtrl->ChList[m].NonOccupancy == 0) ?
									pChCtrl->ChList[m].NOPSaveForClear :
									pChCtrl->ChList[m].NonOccupancy;
								break;
							}
						}
					k++;
					}
				j++;
				channel = reg_class[i].channel_set[j];
			}
			k = 0;
			j = 0;
			if (dfs_ch == TRUE) {
				cac_cap->op_class_num += 1;
				l++;
			}
			dfs_ch = FALSE;
			}
			i++;
		} while (reg_class[i].reg_class != 0 && cac_cap->op_class_num < MAX_CAC_OPCAP_NUM);
	}
	cac_cap->cac_mode = get_cac_mode(pAd, pDfsParam, wdev);
}
#endif

UINT BW_VALUE[] = {20, 40, 80, 160, 10, 5, 162, 320, 60, 25};
UCHAR get_regulatory_class(RTMP_ADAPTER *pAd, UCHAR Channel, USHORT PhyMode, struct wifi_dev *wdev)
{
	int i = 1; /* skip Invlid entry */
	UCHAR regclass = 0;
	UINT16 bw = BW_20;
	UCHAR cfg_ht_bw = wlan_operate_get_ht_bw(wdev);
#ifdef DOT11_VHT_AC
	UCHAR cfg_vht_bw = wlan_operate_get_vht_bw(wdev);
#endif /*DOT11_VHT_AC*/
#ifdef DOT11_HE_AX
	UCHAR cfg_he_bw = wlan_operate_get_he_bw(wdev);
#endif /*DOT11_HE_AX*/
#ifdef DOT11_EHT_BE
	UCHAR cfg_eht_bw = wlan_operate_get_eht_bw(wdev);
#endif /*DOT11_EHT_BE*/

	UCHAR reg_domain = pAd->reg_domain;
	if (WMODE_CAP_N(PhyMode)) {
		if (cfg_ht_bw)
			bw = BW_40;
		else
			bw = BW_20;
	}

#ifdef DOT11_VHT_AC

	if (WMODE_CAP_AC(PhyMode)) {
		switch (cfg_vht_bw) {
		case VHT_BW_80:
			bw = BW_80;
			break;

		case VHT_BW_160:
			bw = BW_160;
			break;

		case VHT_BW_8080:
			bw = BW_8080;
			break;

		case VHT_BW_2040:
			if (cfg_ht_bw == BW_40)
				bw = BW_40;
			else
				bw = BW_20;
		default:
			break;
		}
	}

#endif /* DOT11_VHT_AC */

#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(PhyMode)) {
		switch (cfg_he_bw) {
		case HE_BW_80:
			bw = BW_80;
			break;
		case HE_BW_160:
			bw = BW_160;
			break;
		case HE_BW_8080:
			bw = BW_8080;
			break;
		case HE_BW_2040:
			if (cfg_ht_bw == BW_40)
				bw = BW_40;
			else
				bw = BW_20;
		default:
			break;
		}
	}
#endif /* DOT11_HE_AX */

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(PhyMode)) {
		switch (cfg_eht_bw) {
		case EHT_BW_80:
			bw = BW_80;
			break;
		case EHT_BW_160:
			bw = BW_160;
			break;
		case EHT_BW_320:
			bw = BW_320;
			break;
		case EHT_BW_2040:
			if (cfg_ht_bw == BW_40)
				bw = BW_40;
			else
				bw = BW_20;
			break;
		case EHT_BW_20:
			bw = BW_20;
			break;
		default:
			break;
		}
	}
#endif /* DOT11_EHT_BE */

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG,
 			 "Channel=%d,HT_BW=%d bw=%d\n", Channel, cfg_ht_bw, bw);

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_he) {
			UCHAR ch_band = wlan_config_get_ch_band(wdev);

			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG, "ch_band=%d\n", ch_band);

			do {
				if (BW_VALUE[reg_class_he[i].spacing] >= BW_VALUE[bw]) {

					if ((ch_band == CMD_CH_BAND_6G) && (reg_class_he[i].start_freq != FREQ_5G95)) {
						i++;
						continue;
					}

					/* find channel_set */
					if (is_channel_in_channelset(reg_class_he[i].channel_set, Channel) ||
						(is_channel_in_channelset(reg_class_he[i].center_freq, eht_cent_ch_freq(wdev, Channel, cfg_eht_bw, ch_band)))) {
						regclass = (reg_domain == REG_LOCAL) ? \
						reg_class_he[i].reg_class : \
						reg_class_he[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class_he[i].reg_class != 0);
		}
	} else
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_he) {
			UCHAR ch_band = wlan_config_get_ch_band(wdev);

			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG, "ch_band=%d\n", ch_band);

			do {
				if (BW_VALUE[reg_class_he[i].spacing] >= BW_VALUE[bw]) {

					if ((ch_band == CMD_CH_BAND_6G) && (reg_class_he[i].start_freq != FREQ_5G95)) {
						i++;
						continue;
					}

					/* find channel_set */
					if (is_channel_in_channelset(reg_class_he[i].channel_set, Channel) ||
						(is_channel_in_channelset(reg_class_he[i].center_freq, he_cent_ch_freq(Channel, cfg_he_bw, ch_band)))) {
						regclass = (reg_domain == REG_LOCAL) ? \
						reg_class_he[i].reg_class : \
						reg_class_he[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class_he[i].reg_class != 0);
		}
	} else
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
	if (WMODE_CAP_AC(PhyMode)) {
		PREG_CLASS_VHT reg_class_vht = (PREG_CLASS_VHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_vht) {
			UCHAR ch_band = wlan_config_get_ch_band(wdev);

			do {
				if (BW_VALUE[reg_class_vht[i].spacing] >= BW_VALUE[bw]) {
					/* find  channel_set */
					if (is_channel_in_channelset(reg_class_vht[i].channel_set, Channel) ||
						(is_channel_in_channelset(reg_class_vht[i].center_freq, vht_cent_ch_freq(Channel, cfg_vht_bw, ch_band)))) {
						regclass = (reg_domain == REG_LOCAL) ? \
						reg_class_vht[i].reg_class : \
						reg_class_vht[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class_vht[i].reg_class != 0);
		}
	} else
#endif /* DOT11_VHT_AC */
	{
		PREG_CLASS reg_class = (PREG_CLASS)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class) {
			do {
				if (BW_VALUE[reg_class[i].spacing] >= BW_VALUE[bw]) {
					/* find  channel_set */
					if (is_channel_in_channelset(reg_class[i].channel_set, Channel)) {
						regclass = (reg_domain == REG_LOCAL) ? \
						reg_class[i].reg_class : \
						reg_class[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class[i].reg_class != 0);
		}
	}

	/* ASSERT(regclass); */
	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG, "regulatory class is %d\n", regclass);
	return regclass;
}

BOOLEAN get_ch_by_global_class(
	struct _RTMP_ADAPTER *pAd,
	struct wifi_dev *wdev,
	UCHAR op_class,
	UCHAR *channel)
{
	int i = 1; /* skip Invlid entry */
	PVOID reg_class_table = NULL;
	USHORT phy_mode = wdev->PhyMode;

	if (!channel) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE, "invliad data input.\n");
		return FALSE;
	}

	reg_class_table = get_reg_table_by_country(pAd->CommonCfg.CountryCode, phy_mode);
	if (!reg_class_table) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_NOTICE, "no op class table\n");
		return FALSE;
	}

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(phy_mode)) {
		PREG_CLASS_HE_EHT reg_class_eht = (PREG_CLASS_HE_EHT)reg_class_table;

		do {
			if (reg_class_eht[i].global_class == op_class) {
				if (op_class > 127 && op_class < 138)
					*channel = reg_class_eht[i].center_freq[0];
				else
					*channel = reg_class_eht[i].channel_set[0];
				return TRUE;
			}
			i++;
		} while (reg_class_eht[i].reg_class != 0);
		return FALSE;
	}
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(phy_mode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)reg_class_table;

		do {
			if (reg_class_he[i].global_class == op_class) {
				if (op_class > 127 && op_class < 138)
					*channel = reg_class_he[i].center_freq[0];
				else
					*channel = reg_class_he[i].channel_set[0];
				return TRUE;
			}
			i++;
		} while (reg_class_he[i].reg_class != 0);
		return FALSE;
	}
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
	if (WMODE_CAP_AC(phy_mode)) {
		PREG_CLASS_VHT reg_class_vht = (PREG_CLASS_VHT)reg_class_table;

		do {
			if (reg_class_vht[i].global_class == op_class) {
				*channel = reg_class_vht[i].channel_set[0];
				return TRUE;
			}
			i++;
		} while (reg_class_vht[i].reg_class != 0);
		return FALSE;
	}
#endif /* DOT11_VHT_AC */
	{
		PREG_CLASS reg_class = (PREG_CLASS)reg_class_table;

		do {
			if (reg_class[i].global_class == op_class) {
				*channel = reg_class[i].channel_set[0];
				return TRUE;
			}
			i++;
		} while (reg_class[i].reg_class != 0);
	}
	return FALSE;
}

INT get_operating_class_list(RTMP_ADAPTER *pAd, UCHAR Channel, USHORT PhyMode, struct wifi_dev *wdev, UCHAR *SuppClasslist, INT *Len)
{
	UCHAR SupportedClass[] = {81, 83, 84, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129
		#ifdef CONFIG_6G_SUPPORT
			, 131, 132, 133, 134
		#endif
			};
	memcpy(SuppClasslist, SupportedClass, sizeof(SupportedClass));
	*Len = sizeof(SupportedClass);
	return TRUE;
}

UCHAR get_regulatory_class_for_newCh(RTMP_ADAPTER *pAd, UCHAR NewCh, USHORT PhyMode, struct wifi_dev *wdev)
{
	int i = 1; /* skip Invlid entry */
	UCHAR regclass = 0;
	UCHAR bw = BW_20;
	UCHAR reg_cap_bw = get_channel_bw_cap(wdev, NewCh);
	UCHAR cfg_ht_bw = wlan_config_get_ht_bw(wdev);
#ifdef DOT11_VHT_AC
	UCHAR cfg_vht_bw = wlan_config_get_vht_bw(wdev);
#endif /*DOT11_VHT_AC*/
#ifdef DOT11_HE_AX
	UCHAR cfg_he_bw = wlan_config_get_he_bw(wdev);
#endif /*DOT11_HE_AX*/
#ifdef DOT11_EHT_BE
	UCHAR cfg_eht_bw = wlan_config_get_eht_bw(wdev);
#endif /*DOT11_EHT_BE*/
	UCHAR reg_domain = pAd->reg_domain;

	if (WMODE_CAP_N(PhyMode)) {
		if (cfg_ht_bw)
			bw = BW_40;
		else
			bw = BW_20;
	}

#ifdef DOT11_VHT_AC

	if (WMODE_CAP_AC(PhyMode)) {
		switch (cfg_vht_bw) {
		case VHT_BW_80:
			bw = BW_80;
			break;

		case VHT_BW_160:
			bw = BW_160;
			break;

		case VHT_BW_8080:
			bw = BW_8080;
			break;

		case VHT_BW_2040:
			if (cfg_ht_bw == BW_40)
				bw = BW_40;
			else
				bw = BW_20;
		default:
			break;
		}
	}

#endif /* DOT11_VHT_AC */

#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(PhyMode)) {
		switch (cfg_he_bw) {
		case HE_BW_80:
			bw = BW_80;
			break;
		case HE_BW_160:
			bw = BW_160;
			break;
		case HE_BW_8080:
			bw = BW_8080;
			break;
		case HE_BW_2040:
			if (cfg_ht_bw == BW_40)
				bw = BW_40;
			else
				bw = BW_20;
		default:
			break;
		}
	}
#endif /* DOT11_HE_AX */

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(PhyMode)) {
		switch (cfg_eht_bw) {
		case EHT_BW_80:
			bw = BW_80;
			break;
		case EHT_BW_160:
			bw = BW_160;
			break;
		case EHT_BW_320:
			bw = BW_320;
			break;
		case HE_BW_2040:
			if (cfg_ht_bw == BW_40)
				bw = BW_40;
			else
				bw = BW_20;
			break;
		case HE_BW_20:
			bw = BW_20;
			break;
		default:
			break;
		}
	}
#endif /* DOT11_EHT_BE */

	/* if bw capability of NewCh is lower than .dat bw config, bw should follow reg_cap_bw*/
	if (bw > reg_cap_bw) {
		if (!(bw == BW_8080 && (reg_cap_bw == BW_80 || reg_cap_bw == BW_160))) {
			bw = reg_cap_bw;

		/*after bw is adjusted, cfg_he_bw/cfg_vht_bw also need adjust*/
			switch (bw) {
			case BW_20:
				cfg_vht_bw = VHT_BW_2040;
				cfg_he_bw = HE_BW_2040;
				break;
			case BW_40:
				cfg_vht_bw = VHT_BW_2040;
				cfg_he_bw = HE_BW_2040;
				break;
			case BW_80:
				cfg_vht_bw = VHT_BW_80;
				cfg_he_bw = HE_BW_80;
				break;
			case BW_160:
				cfg_vht_bw = VHT_BW_160;
				cfg_he_bw = HE_BW_160;
			default:
				break;
			}
		}
#ifdef DOT11_EHT_BE
		cfg_eht_bw = rf_bw_2_eht_bw(bw);
#endif
	}

	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG,
			"Channel=%d,HT_BW=%d bw=%d\n", NewCh, cfg_ht_bw, bw);

#ifdef DOT11_EHT_BE
	if (WMODE_CAP_BE(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_he) {
			UCHAR ch_band = wlan_config_get_ch_band(wdev);

			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG, "ch_band=%d\n", ch_band);

			do {
				if (BW_VALUE[reg_class_he[i].spacing] >= BW_VALUE[bw]) {

					if ((ch_band == CMD_CH_BAND_6G) && (reg_class_he[i].start_freq != FREQ_5G95)) {
						i++;
						continue;
					}

					/* find channel_set */
					if (is_channel_in_channelset(reg_class_he[i].channel_set, NewCh) ||
						(is_channel_in_channelset(reg_class_he[i].center_freq, eht_cent_ch_freq(wdev, NewCh, cfg_eht_bw, ch_band)))) {
						regclass = (reg_domain == REG_LOCAL) ? reg_class_he[i].reg_class : reg_class_he[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class_he[i].reg_class != 0);
		}
	} else
#endif /* DOT11_EHT_BE */
#ifdef DOT11_HE_AX
	if (WMODE_CAP_AX(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_he) {
			UCHAR ch_band = wlan_config_get_ch_band(wdev);

			MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG, "ch_band=%d\n", ch_band);

			do {
				if (BW_VALUE[reg_class_he[i].spacing] >= BW_VALUE[bw]) {

					if ((ch_band == CMD_CH_BAND_6G) && (reg_class_he[i].start_freq != FREQ_5G95)) {
						i++;
						continue;
					}

					/* find channel_set */
					if (is_channel_in_channelset(reg_class_he[i].channel_set, NewCh) ||
						(is_channel_in_channelset(reg_class_he[i].center_freq, he_cent_ch_freq(NewCh, cfg_he_bw, ch_band)))) {
						regclass = (reg_domain == REG_LOCAL) ? reg_class_he[i].reg_class : reg_class_he[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class_he[i].reg_class != 0);
		}
	} else
#endif /* DOT11_HE_AX */
#ifdef DOT11_VHT_AC
		if (WMODE_CAP_AC(PhyMode)) {
			PREG_CLASS_VHT reg_class_vht = (PREG_CLASS_VHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

			if (reg_class_vht) {
				UCHAR ch_band = wlan_config_get_ch_band(wdev);

			do {
				if (BW_VALUE[reg_class_vht[i].spacing] >= BW_VALUE[bw]) {
					/* find  channel_set */
					if (is_channel_in_channelset(reg_class_vht[i].channel_set, NewCh) ||
						(is_channel_in_channelset(reg_class_vht[i].center_freq, vht_cent_ch_freq(NewCh, cfg_vht_bw, ch_band)))) {
						regclass = (reg_domain == REG_LOCAL) ? reg_class_vht[i].reg_class : reg_class_vht[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class_vht[i].reg_class != 0);
		}
	} else
#endif /* DOT11_VHT_AC */
	{
		PREG_CLASS reg_class = (PREG_CLASS)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class) {
			do {
				if (BW_VALUE[reg_class[i].spacing] >= BW_VALUE[bw]) {
					/* find  channel_set */
					if (is_channel_in_channelset(reg_class[i].channel_set, NewCh)) {
						regclass = (reg_domain == REG_LOCAL) ? reg_class[i].reg_class : reg_class[i].global_class;
						break;
					}
				}
				i++;
			} while (reg_class[i].reg_class != 0);
		}
	}

	/* ASSERT(regclass); */
	MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG, "regulatory class is %d\n", regclass);
	return regclass;
}

INT Show_ChannelSet_Proc(RTMP_ADAPTER *pAd, RTMP_STRING *arg)
{
	int class_idx = 1; /* skip Invlid entry */
	int Channel = 0, Type = 0, phymode_idx = 0, OpClass = 0;
	RTMP_STRING *arg0_type = NULL, *arg1_channel = NULL, *arg2_phymode_idx = NULL;
	POS_COOKIE	pObj = (POS_COOKIE) pAd->OS_Cookie;
	struct wifi_dev *wdev = get_wdev_by_ioctl_idx_and_iftype(pAd, pObj->ioctl_if, pObj->ioctl_if_type);

	arg0_type = strsep(&arg, ":"); /*0: find op class by channel, 1: show channel set */
	arg1_channel = strsep(&arg, ":");
	arg2_phymode_idx = strsep(&arg, ":"); /* 0: AN, 1: AC, 2: AX, 3: BE */

	if (arg0_type == NULL || arg1_channel == NULL || arg2_phymode_idx == NULL) {
		MTWF_PRINT("Invalid parameters\n");
		MTWF_PRINT("TYPE:CHANNEL:ISAC\n");
		return FALSE;
	}

	Type = os_str_tol(arg0_type, 0, 10);
	Channel = os_str_tol(arg1_channel, 0, 10);
	phymode_idx = os_str_tol(arg2_phymode_idx, 0, 10);
	MTWF_PRINT("Type = %d Channel = %d phymode_idx = %d\n", Type, Channel, phymode_idx);

	if (Type != 0) {

		switch (phymode_idx) {
		case 0:
			OpClass = get_regulatory_class(pAd, Channel, WMODE_AN, wdev);
			break;

#ifdef DOT11_VHT_AC
		case 1:
			OpClass = get_regulatory_class(pAd, Channel, WMODE_AC, wdev);
			break;
#endif /* DOT11_VHT_AC */
#ifdef DOT11_HE_AX
		case 2:
			OpClass = get_regulatory_class(pAd, Channel, (WMODE_AX_24G | WMODE_AX_5G | WMODE_AX_6G), wdev);
			break;
#endif /* DOT11_HE_AX */
#ifdef DOT11_EHT_BE
		case 3:
			OpClass = get_regulatory_class(pAd, Channel, (WMODE_BE_24G | WMODE_BE_5G | WMODE_BE_6G), wdev);
			break;
#endif /* DOT11_EHT_BE */

		default:
			OpClass = get_regulatory_class(pAd, Channel, WMODE_AN, wdev);
			break;
		}

		MTWF_PRINT("OpClass(%d) Channel(%d)\n", OpClass, Channel);

		return TRUE;
	} else {
		MTWF_PRINT("\n%-8s%-8s%-16s\n", "Regclass", "Spacing", "Channelset/CenterFreq");

		switch (phymode_idx) {
		case 0: {
			PREG_CLASS reg_class = (PREG_CLASS)get_reg_table_by_country(pAd->CommonCfg.CountryCode, WMODE_AN);

			do {
				int chset_idx = 0;

				MTWF_PRINT("%-8d%-8d", reg_class[class_idx].reg_class, reg_class[class_idx].spacing);

				while (reg_class[class_idx].channel_set[chset_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class[class_idx].channel_set[chset_idx]);
					chset_idx++;
				}

				class_idx++;
				MTWF_PRINT("\n");
			} while (reg_class[class_idx].reg_class != 0);

		}
		break;

#ifdef DOT11_VHT_AC
		case 1: {
			PREG_CLASS_VHT reg_class_vht = (PREG_CLASS_VHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, WMODE_AC);

			do {
				int chset_idx = 0, cntr_freq_idx = 0;

				MTWF_PRINT("%-8d%-8d", reg_class_vht[class_idx].reg_class, reg_class_vht[class_idx].spacing);

				while (reg_class_vht[class_idx].channel_set[chset_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class_vht[class_idx].channel_set[chset_idx]);
					chset_idx++;
				}

				MTWF_PRINT("/");

				while (reg_class_vht[class_idx].center_freq[cntr_freq_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class_vht[class_idx].center_freq[cntr_freq_idx]);
					cntr_freq_idx++;
				}

				class_idx++;
				MTWF_PRINT("\n");
			} while (reg_class_vht[class_idx].reg_class != 0);

		}
		break;
#endif /* DOT11_VHT_AC */
#ifdef DOT11_HE_AX
		case 2: {
			PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, (WMODE_AX_24G | WMODE_AX_5G | WMODE_AX_6G));

			do {
				int chset_idx = 0, cntr_freq_idx = 0;

				MTWF_PRINT("%-8d%-8d", reg_class_he[class_idx].reg_class, reg_class_he[class_idx].spacing);

				while (reg_class_he[class_idx].channel_set[chset_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class_he[class_idx].channel_set[chset_idx]);
					chset_idx++;
				}

				MTWF_PRINT("/");

				while (reg_class_he[class_idx].center_freq[cntr_freq_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class_he[class_idx].center_freq[cntr_freq_idx]);
					cntr_freq_idx++;
				}

				class_idx++;
				MTWF_PRINT("\n");
			} while (reg_class_he[class_idx].reg_class != 0);
		}
		break;
#endif /* DOT11_HE_AX */
#ifdef DOT11_EHT_BE
		case 3: {
			PREG_CLASS_HE_EHT reg_class_eht = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, (WMODE_BE_24G | WMODE_BE_5G | WMODE_BE_6G));

			do {
				int chset_idx = 0, cntr_freq_idx = 0;

				MTWF_PRINT("%-8d%-8d", reg_class_eht[class_idx].reg_class, reg_class_eht[class_idx].spacing);

				while (reg_class_eht[class_idx].channel_set[chset_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class_eht[class_idx].channel_set[chset_idx]);
					chset_idx++;
				}

				MTWF_PRINT("/");

				while (reg_class_eht[class_idx].center_freq[cntr_freq_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class_eht[class_idx].center_freq[cntr_freq_idx]);
					cntr_freq_idx++;
				}

				class_idx++;
				MTWF_PRINT("\n");
			} while (reg_class_eht[class_idx].reg_class != 0);
		}
		break;
#endif /* DOT11_EHT_BE */
		default: {
			PREG_CLASS reg_class = (PREG_CLASS)get_reg_table_by_country(pAd->CommonCfg.CountryCode, WMODE_AN);

			do {
				int chset_idx = 0;

				MTWF_PRINT("%-8d%-8d", reg_class[class_idx].reg_class, reg_class[class_idx].spacing);

				while (reg_class[class_idx].channel_set[chset_idx] != 0) {
					MTWF_PRINT("%-2d,", reg_class[class_idx].channel_set[chset_idx]);
					chset_idx++;
				}

				class_idx++;
				MTWF_PRINT("\n");
			} while (reg_class[class_idx].reg_class != 0);
		}
		break;

		}

	}

	return TRUE;
}

INT Set_Reg_Domain_Proc(
	IN	PRTMP_ADAPTER			pAd,
       IN      RTMP_STRING     *arg)
{
	pAd->reg_domain = simple_strtol(arg, 0, 10);
	/* 0: REG_LOCAL  1:REG_GLOBAL */
	MTWF_PRINT("reg_domain = %u\n", pAd->reg_domain);
	return TRUE;
}

VOID get_reg_class_list_for_6g(
	IN RTMP_ADAPTER * pAd,
	IN USHORT PhyMode,
	PUCHAR reg_class_value)
{
	PREG_CLASS_HE_EHT reg_class = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);
	UINT i = 0, j = 0;

	if (reg_class == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR, "reg_class is NULL for 6G!!!\n");
		return;
	}

	for (i = 1; reg_class[i].reg_class != 0 && j < 5; i++) {
		if ((reg_class[i].start_freq == FREQ_5G95 &&
#ifndef DOT11_EHT_BE
			reg_class[i].spacing <= BW_160)) {
#else
			reg_class[i].spacing <= BW_320)) {
#endif
			reg_class_value[j] = reg_class[i].reg_class;
			j++;
		}
	}
}

#ifdef CONFIG_MAP_SUPPORT
#ifdef MAP_VENDOR_SUPPORT
UCHAR is_40M_bw_disable(IN PRTMP_ADAPTER pAd)
{
	CHANNEL_CTRL *pChCtrl = NULL;
	int ret = 0;

	pChCtrl = hc_get_channel_ctrl(pAd->hdev_ctrl);
	if (pChCtrl->ch_cfg.bw40_forbid == TRUE) {
		ret = 1;
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG,
			"%s 40M BW Disable", __func__);
	} else {
		MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_DEBUG,
			"%s 40M BW Enable", __func__);
		ret = 0;
	}

	return ret;
}
#endif /* MAP_VENDOR_SUPPORT */

#ifdef MAP_6E_SUPPORT
UCHAR map_set_op_class_info(
	PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev,
	wdev_op_class_info_ext *op_class)
{
	UCHAR i = 0, j = 0, k = 0;
	UCHAR op_index = 0, seen = 0;
	UCHAR chnNum = 0, maxChNum = 0;
	USHORT PhyMode = wdev->PhyMode;
	PREG_CLASS reg_class = NULL;
	PREG_CLASS_VHT reg_class_vht = NULL;
	UCHAR chValid;
	UCHAR chk_ch = 0;
	UCHAR chk_global_class = 0;
	UCHAR chk_nonglobal_class = 0;
	UCHAR vht_bw = 0;
	UCHAR ht_bw = 0;
	UCHAR eht_bw = 0;
#if defined(DOT11_HE_AX) || defined(DOT11_EHT_BE)
	PREG_CLASS_HE_EHT reg_class_he = NULL;
#ifdef MAP_VENDOR_SUPPORT
	UCHAR bw_40M_disable = 0;
	int max_priority = 0;
#endif /* MAP_VENDOR_SUPPORT */
#endif /* DOT11_HE_AX */

#if defined(DOT11_HE_AX) || defined(DOT11_EHT_BE)
	if (WMODE_CAP_AX(PhyMode))
		reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);
	else if (WMODE_CAP_AC(PhyMode))
#else
	if (WMODE_CAP_AC(PhyMode))
#endif /* DOT11_HE_AX */
		reg_class_vht = (PREG_CLASS_VHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);
	else if (WMODE_CAP_2G(PhyMode) || WMODE_CAP_5G(PhyMode))
		reg_class = (PREG_CLASS)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);
	else
		return 0;

	vht_bw = wlan_config_get_vht_bw(wdev);
	ht_bw = wlan_config_get_ht_bw(wdev);
	eht_bw = wlan_config_get_eht_bw(wdev);

#if defined(DOT11_HE_AX) || defined(DOT11_EHT_BE)
#ifdef MAP_VENDOR_SUPPORT
	bw_40M_disable = is_40M_bw_disable(pAd);
	op_class->is_40M_bw_disable = bw_40M_disable;
#endif /* MAP_VENDOR_SUPPORT */
#endif /* DOT11_HE_AX */

	for (i = 1;; i++) {
		seen = 0;
#if defined(DOT11_HE_AX) || defined(DOT11_EHT_BE)
		if (WMODE_CAP_AX(PhyMode)) {
			chk_global_class = reg_class_he[i].global_class;
			chk_nonglobal_class = reg_class_he[i].reg_class;
		} else if (WMODE_CAP_AC(PhyMode)) {
#else
		if (WMODE_CAP_AC(PhyMode)) {
#endif /* DOT11_HE_AX */
			chk_global_class = reg_class_vht[i].global_class;
			chk_nonglobal_class = reg_class_vht[i].reg_class;
		} else if (WMODE_CAP_2G(PhyMode) || WMODE_CAP_5G(PhyMode)) {
			chk_global_class = reg_class[i].global_class;
			chk_nonglobal_class = reg_class[i].reg_class;
		}

		/*End of Op Class Table Reached*/
		if (chk_nonglobal_class == 0)
			break;

		if (IS_6G_OPCLASS(chk_global_class))
			maxChNum = 59;
		else if (IS_2G_OPCLASS(chk_global_class) ||
				 IS_5G_OPCLASS(chk_global_class))
			maxChNum = 13;
		else
			continue;

		for (k = 0; k < op_index; k++) {
			if (chk_global_class == op_class->opClassInfoExt[k].op_class) {
				/*This is a duplicate entry in opclass list*/
				seen = 1;
				break;
			}
		}

		if (seen)
			continue;

		if (WMODE_CAP_BE(PhyMode)) {
			if (eht_bw != EHT_BW_320 && IS_320M(chk_global_class))
				continue;

			if (eht_bw != EHT_BW_160 && IS_160M(chk_global_class))
				continue;

			if (eht_bw < EHT_BW_80 && IS_80M(chk_global_class))
				continue;
		} else {
			if (vht_bw != VHT_BW_160 && IS_160M(chk_global_class))
				continue;

			if (vht_bw < VHT_BW_80 && IS_80M(chk_global_class))
				continue;
		}

		if (vht_bw != VHT_BW_8080 && IS_8080(chk_global_class))
			continue;

		if (ht_bw != HT_BW_40 && IS_40M(chk_global_class))
			continue;

#ifdef MAP_VENDOR_SUPPORT
		if (WMODE_CAP_5G(PhyMode) && bw_40M_disable &&
			IS_40M(chk_global_class)) {
			MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"%s skip for 40 M opclass\n",  __func__);
			continue;
		}
#endif /* MAP_VENDOR_SUPPORT */

		if (!is_global_opclass_for_wdev(wdev, chk_global_class)) {
			MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"chk_nonglobal_class=%d chk_global_class=%d is not match current band\n",
				chk_nonglobal_class, chk_global_class);
			continue;
		}

		chnNum = 0;
		op_class->opClassInfoExt[op_index].op_class = chk_global_class;

		for (j = 0; j <= maxChNum; j++) {
#ifdef DOT11_HE_AX
			if (WMODE_CAP_AX(PhyMode)) {
				if (chk_global_class <= OP_CLASS_127)
					chk_ch = reg_class_he[i].channel_set[j];
				else
					chk_ch = reg_class_he[i].center_freq[j];
			} else if (WMODE_CAP_AC(PhyMode)) {
#else
			if (WMODE_CAP_AC(PhyMode)) {
#endif /* DOT11_HE_AX */
				if (chk_global_class <= OP_CLASS_127)
					chk_ch = reg_class_vht[i].channel_set[j];
				else
					chk_ch = reg_class_vht[i].center_freq[j];
			} else
				chk_ch = reg_class[i].channel_set[j];

			if (chk_ch == 0)
				break;

			chValid = (UCHAR)check_global_opclass_channel_available(
							pAd, wdev, chk_global_class, chk_ch);
			if (chValid) {
				op_class->opClassInfoExt[op_index].ch_list[chnNum] = chk_ch;
#ifdef MAP_VENDOR_SUPPORT
				max_priority = get_global_opclass_channel_priority(
								pAd, wdev, chk_global_class, chk_ch);
				op_class->opClassInfoExt[op_index].ch_pref[chnNum] = max_priority;
#endif /* MAP_VENDOR_SUPPORT */
				chnNum++;
			}
		}
		if (chnNum) {
			MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"chk_nonglobal_class=%d chk_global_class=%d chnNum=%d, op_index=%d\n",
			chk_nonglobal_class, chk_global_class, chnNum, op_index);
			op_class->opClassInfoExt[op_index].num_of_ch = chnNum;
			op_index++;
		}
	}
	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"total_op_class = %d\n", op_index);
	return op_index;
}

UCHAR map_set_op_class_info_6g(
	PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev,
	wdev_op_class_info_ext *op_class)
{
	return map_set_op_class_info(pAd, wdev, op_class);
}
#else
UCHAR map_set_op_class_info(
	PRTMP_ADAPTER pAd,
	struct wifi_dev *wdev,
	wdev_op_class_info *op_class)
{
	UCHAR i = 0, j = 0, k = 0;
	UCHAR op_index = 0, seen = 0;
	UCHAR chnNum = 0, maxChNum = 0;
	USHORT PhyMode = wdev->PhyMode;
	PREG_CLASS reg_class = NULL;
	PREG_CLASS_VHT reg_class_vht = NULL;
#ifdef DFS_CAC_R2
	UCHAR chValid;
#endif /* DFS_CAC_R2 */
	UCHAR chk_ch = 0;
	UCHAR chk_global_class = 0;
	UCHAR chk_nonglobal_class = 0;
	UCHAR vht_bw = 0;
	UCHAR ht_bw = 0;
	UCHAR eht_bw = 0;
#if defined(DOT11_HE_AX) || defined(DOT11_EHT_BE)
	PREG_CLASS_HE_EHT reg_class_he = NULL;
#endif /* DOT11_HE_AX */

#if defined(DOT11_HE_AX) || defined(DOT11_EHT_BE)
	if (WMODE_CAP_AX(PhyMode)) {
		reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);
	} else
#endif /* DOT11_HE_AX */
	if (WMODE_CAP_AC(PhyMode))
		reg_class_vht = (PREG_CLASS_VHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);
	else if (WMODE_CAP_2G(PhyMode) || WMODE_CAP_5G(PhyMode))
		reg_class = (PREG_CLASS)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);
	else
		return 0;

	vht_bw = wlan_config_get_vht_bw(wdev);
	ht_bw = wlan_config_get_ht_bw(wdev);
	eht_bw = wlan_config_get_eht_bw(wdev);

	for (i = 1;; i++) {
		seen = 0;
#if defined(DOT11_HE_AX) || defined(DOT11_EHT_BE)
		if (WMODE_CAP_AX(PhyMode)) {
			chk_global_class = reg_class_he[i].global_class;
			chk_nonglobal_class = reg_class_he[i].reg_class;
		} else
#endif /* DOT11_HE_AX */
		if (WMODE_CAP_AC(PhyMode)) {
			chk_global_class = reg_class_vht[i].global_class;
			chk_nonglobal_class = reg_class_vht[i].reg_class;
		} else if (WMODE_CAP_2G(PhyMode) || WMODE_CAP_5G(PhyMode)) {
			chk_global_class = reg_class[i].global_class;
			chk_nonglobal_class = reg_class[i].reg_class;
		}

		if (chk_nonglobal_class == 0) {
			/*End of Op Class Table Reached*/
			break;
		}

		if (IS_2G_OPCLASS(chk_global_class) ||
			IS_5G_OPCLASS(chk_global_class))
			maxChNum = 13;
		else
			continue;

		for (k = 0; k < op_index; k++) {
			if (chk_global_class == op_class->opClassInfo[k].op_class) {
				/*This is a duplicate entry in opclass list*/
				seen = 1;
				break;
			}
		}

		if (seen)
			continue;

		if (WMODE_CAP_BE(PhyMode)) {
			if (eht_bw != EHT_BW_160 && IS_160M(chk_global_class))
				continue;

			if (eht_bw < EHT_BW_80 && IS_80M(chk_global_class))
				continue;
		} else {
			if (vht_bw != VHT_BW_160 && IS_160M(chk_global_class))
				continue;

			if (vht_bw < VHT_BW_80 && IS_80M(chk_global_class))
				continue;
		}

		if (vht_bw != VHT_BW_8080 && IS_8080(chk_global_class))
			continue;

		if (ht_bw != HT_BW_40 && IS_40M(chk_global_class))
			continue;

#ifdef DFS_CAC_R2
		if (!is_global_opclass_for_wdev(wdev, chk_global_class)) {
			MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
				"chk_nonglobal_class=%d chk_global_class=%d is not match current band\n",
				chk_nonglobal_class, chk_global_class);
			continue;
		}
#endif /* DFS_CAC_R2 */

		chnNum = 0;
		op_class->opClassInfo[op_index].op_class = chk_global_class;

		for (j = 0; j <= maxChNum; j++) {
#ifdef DOT11_HE_AX
			if (WMODE_CAP_AX(PhyMode)) {
				if (chk_global_class <= OP_CLASS_127)
					chk_ch = reg_class_he[i].channel_set[j];
				else
					chk_ch = reg_class_he[i].center_freq[j];
			} else if (WMODE_CAP_AC(PhyMode)) {
#else
			if (WMODE_CAP_AC(PhyMode)) {
#endif /* DOT11_HE_AX */
				if (chk_global_class <= OP_CLASS_127)
					chk_ch = reg_class_vht[i].channel_set[j];
				else
					chk_ch = reg_class_vht[i].center_freq[j];
			} else
				chk_ch = reg_class[i].channel_set[j];

			if (chk_ch == 0)
				break;
#ifdef DFS_CAC_R2
			chValid = (UCHAR)check_global_opclass_channel_available(
							pAd, wdev, chk_global_class, chk_ch);
			if (chValid) {
				op_class->opClassInfo[op_index].ch_list[chnNum] = chk_ch;
				chnNum++;
			}
#endif /* DFS_CAC_R2 */
		}
		if (chnNum) {
			MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"chk_nonglobal_class=%d chk_global_class=%d chnNum=%d, op_index=%d\n",
			chk_nonglobal_class, chk_global_class, chnNum, op_index);
			op_class->opClassInfo[op_index].num_of_ch = chnNum;
			op_index++;
		}
	}
	MTWF_DBG(NULL, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
			"total_op_class = %d\n", op_index);
	return op_index;
}
#endif /* MAP_6E_SUPPORT */

INT ShowGlobalOpCLassChInfo(RTMP_ADAPTER *pAd, RTMP_STRING *arg)
{
#ifdef DFS_CAC_R2
	struct wifi_dev *wdev = NULL;
	struct REG_CLASS_GLOBAL *reg_class = NULL;
	INT i = 0, class_idx = 0;
	UINT16 min = 0, max = 0, grp_ch_cnt = 0, grp_bw = 0;
	bool rc = FALSE;

	for (i = 0; i < WDEV_NUM_MAX; i++) {
		if (pAd->wdev_list[i] && HcIsRadioAcq(pAd->wdev_list[i])) {
			wdev = pAd->wdev_list[i];
			break;
		}
	}

	if (wdev == NULL)
		return FALSE;

	do {
		i = 0;
		reg_class = &reg_class_global[class_idx];
		if (!is_global_opclass_for_wdev(wdev, reg_class->global_class)) {
			class_idx++;
			continue;
		}
		MTWF_PRINT("op_class=%-3d\n", reg_class->global_class);
		while (reg_class->channel_set[i] != 0) {
			rc = check_global_opclass_channel_available(
					pAd, wdev,
					reg_class->global_class,
					reg_class->channel_set[i]);
			get_global_opclass_channel_range(
					reg_class->global_class,
					reg_class->channel_set[i],
					&min, &max, &grp_ch_cnt, &grp_bw);

			MTWF_PRINT(" - channel_set[%d]=%d (%d-%d, cnt=%d, bw=%d), rc=%s\n",
					i, reg_class->channel_set[i],
					min, max, grp_ch_cnt, grp_bw,
					rc == TRUE ? "TRUE":"FALSE");
			i++;
		}

		i = 0;
		while (reg_class->center_ch_set[i] != 0) {
			rc = check_global_opclass_channel_available(
					pAd, wdev,
					reg_class->global_class,
					reg_class->center_ch_set[i]);
			get_global_opclass_channel_range(
					reg_class->global_class,
					reg_class->center_ch_set[i],
					&min, &max, &grp_ch_cnt, &grp_bw);

			MTWF_PRINT(" - center_ch_set[%d]=%d (%d-%d, cnt=%d, bw=%d), rc=%s\n",
					i, reg_class->center_ch_set[i],
					min, max, grp_ch_cnt, grp_bw,
					rc == TRUE ? "TRUE":"FALSE");
			i++;
		}

		class_idx++;
		MTWF_PRINT("\n");
	} while (reg_class_global[class_idx].global_class != 0);
#endif /* DFS_CAC_R2 */

	return TRUE;
}

#ifdef MAP_6E_SUPPORT
INT ShowOpClassInfo(RTMP_ADAPTER *pAd, RTMP_STRING *arg)
{
	struct wifi_dev *wdev = NULL;
	wdev_op_class_info_ext *info = NULL;
	int i = 0, j = 0, len = 0;
	UCHAR *tmp_str = NULL;
	UCHAR *pos = NULL;
	UCHAR *end = NULL;
	UCHAR vht_bw = 0;
	UCHAR ht_bw = 0;
	UCHAR eht_bw = 0;

	for (i = 0; i < WDEV_NUM_MAX; i++) {
		if (pAd->wdev_list[i] && HcIsRadioAcq(pAd->wdev_list[i])) {
			wdev = pAd->wdev_list[i];
			break;
		}
	}

	if (wdev == NULL)
		return FALSE;

	vht_bw = wlan_config_get_vht_bw(wdev);
	ht_bw = wlan_config_get_ht_bw(wdev);
	eht_bw = wlan_config_get_eht_bw(wdev);

	os_alloc_mem(NULL, (UCHAR **)&tmp_str, 1000);
	os_alloc_mem(NULL, (UCHAR **)&info, sizeof(wdev_op_class_info_ext));

	if (tmp_str == NULL || info == NULL) {
		MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_ERROR, "Memory allocation failed\n");
		if (tmp_str != NULL)
			os_free_mem(tmp_str);
		if (info != NULL)
			os_free_mem(info);
		return FALSE;
	}

	memset(info, 0, sizeof(wdev_op_class_info_ext));

	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_24G ||
		wlan_config_get_ch_band(wdev) == CMD_CH_BAND_5G ||
		wlan_config_get_ch_band(wdev) == CMD_CH_BAND_6G)
		info->num_of_op_class = map_set_op_class_info(pAd, wdev, info);

	MTWF_PRINT("phy_mode=%x ht_bw=%d vht_bw=%d eht_bw=%d\n",
				wdev->PhyMode, ht_bw, vht_bw, eht_bw);
	MTWF_PRINT("num_of_op_class=%d, is_40M_bw_disable=%d\n",
				info->num_of_op_class,
				info->is_40M_bw_disable);
	for (i = 0; i < info->num_of_op_class; i++) {
		MTWF_PRINT("  op_class=%d, num_of_ch=%d\n",
				info->opClassInfoExt[i].op_class,
				info->opClassInfoExt[i].num_of_ch);
		pos = tmp_str;
		end = pos + 1000;
		for (j = 0; j < info->opClassInfoExt[i].num_of_ch; j++) {
			len = snprintf(pos, end-pos, "%3d,", info->opClassInfoExt[i].ch_list[j]);
			if (os_snprintf_error(end-pos, len)) {
				os_free_mem(tmp_str);
				os_free_mem(info);
				return FALSE;
			}
			pos += len;
		}

		if (j > 0)
			pos--;

		*pos = '\0';

		MTWF_PRINT("  - ch_list:%s\n", tmp_str);

		pos = tmp_str;
		for (j = 0; j < info->opClassInfoExt[i].num_of_ch; j++) {
			len = snprintf(pos, 1000, "%3d,", info->opClassInfoExt[i].ch_pref[j]);
			if (os_snprintf_error(end-pos, len)) {
				os_free_mem(tmp_str);
				os_free_mem(info);
				return FALSE;
			}
			pos += len;
		}

		if (j > 0) {
			pos--;
			*pos = '\0';
		} else
			*pos = '\0';
		MTWF_PRINT("  - ch_pref:%s\n", tmp_str);
	}

	os_free_mem(tmp_str);
	os_free_mem(info);
	return TRUE;
}
#else
INT ShowOpClassInfo(RTMP_ADAPTER *pAd, RTMP_STRING *arg)
{
	struct wifi_dev *wdev = NULL;
	wdev_op_class_info *info = NULL;
	int i = 0, j = 0, len = 0;
	UCHAR *tmp_str = NULL;
	UCHAR *pos = NULL;
	UCHAR *end = NULL;
	UCHAR vht_bw = 0;
	UCHAR ht_bw = 0;
	UCHAR eht_bw = 0;

	for (i = 0; i < WDEV_NUM_MAX; i++) {
		if (pAd->wdev_list[i] && HcIsRadioAcq(pAd->wdev_list[i])) {
			wdev = pAd->wdev_list[i];
			break;
		}
	}

	if (wdev == NULL)
		return FALSE;

	vht_bw = wlan_config_get_vht_bw(wdev);
	ht_bw = wlan_config_get_ht_bw(wdev);
	eht_bw = wlan_config_get_eht_bw(wdev);

	os_alloc_mem(NULL, (UCHAR **)&tmp_str, 1000);
	os_alloc_mem(NULL, (UCHAR **)&info, sizeof(wdev_op_class_info));

	memset(info, 0, sizeof(wdev_op_class_info));

	if (wlan_config_get_ch_band(wdev) == CMD_CH_BAND_24G ||
		wlan_config_get_ch_band(wdev) == CMD_CH_BAND_5G)
		info->num_of_op_class = map_set_op_class_info(pAd, wdev, info);


	MTWF_PRINT("phy_mode=%x ht_bw=%d vht_bw=%d eht_bw=%d\n",
				wdev->PhyMode, ht_bw, vht_bw, eht_bw);
	MTWF_PRINT("num_of_op_class=%d\n", info->num_of_op_class);
	for (i = 0; i < info->num_of_op_class; i++) {
		MTWF_PRINT("  op_class=%d, num_of_ch=%d\n",
				info->opClassInfo[i].op_class,
				info->opClassInfo[i].num_of_ch);
		pos = tmp_str;
		end = pos + 1000;
		for (j = 0; j < info->opClassInfo[i].num_of_ch; j++) {
			len = snprintf(pos, end-pos, "%3d,", info->opClassInfo[i].ch_list[j]);
			if (os_snprintf_error(end-pos, len)) {
				os_free_mem(tmp_str);
				os_free_mem(info);
				return FALSE;
			}
			pos += len;
		}

		if (j > 0)
			pos--;

		*pos = '\0';

		MTWF_PRINT("  - ch_list:%s\n", tmp_str);
	}

	os_free_mem(tmp_str);
	os_free_mem(info);
	return TRUE;
}
#endif /* MAP_6E_SUPPORT */
#endif /* CONFIG_MAP_SUPPORT */

#ifdef DOT11_HE_AX
BOOLEAN is_ru26_disable_channel(RTMP_ADAPTER *pAd, UCHAR Channel, USHORT PhyMode)
{
	int i = 1; /* skip Invlid entry */

	if (WMODE_CAP_AX(PhyMode)) {
		PREG_CLASS_HE_EHT reg_class_he = (PREG_CLASS_HE_EHT)get_reg_table_by_country(pAd->CommonCfg.CountryCode, PhyMode);

		if (reg_class_he) {
			do {
				/* find  channel_set */
				if (is_channel_in_channelset(reg_class_he[i].channel_set, Channel)) {
					if ((reg_class_he[i].behavior_limit_set != COMMON) && (reg_class_he[i].behavior_limit_set & DFS_50_100_BEHAVIOR) != 0) {
						MTWF_DBG(pAd, DBG_CAT_CHN, CATCHN_CHN, DBG_LVL_INFO,
						 "Channel=%d, CountryCode %s, reg_class %d  hit RU26 disable\n",
						  Channel, pAd->CommonCfg.CountryCode, reg_class_he[i].reg_class);
						return TRUE;
					}
				}
				i++;
			} while (reg_class_he[i].reg_class != 0);
		}
	}

	return FALSE;
}

#endif /* DOT11_HE_AX */

VOID make_channel_usage_ie(
	RTMP_ADAPTER *pAd, struct wifi_dev *wdev)
{
	UCHAR ie_len = 0;
	UCHAR channel;
	BOOLEAN valid = FALSE;
	UCHAR *ie_info;
	struct ch_usage *ch_usage_info;

	wdev->ch_usage_info.ie_len = 0;
	if (wdev->ch_usage_info.mode == CH_USAGE_MODE_OFF)
		return;

	ch_usage_info = &wdev->ch_usage_info;
	ie_info = &ch_usage_info->ie_info[0];
	ie_info[ie_len] = EID_CHANNEL_USAGE;
	ie_len += 2; /* update len later */
	ie_info[ie_len] = ch_usage_info->mode;
	ie_len++;
	if (ch_usage_info->bw20_op_class
		&& get_ch_by_global_class(pAd, wdev, ch_usage_info->bw20_op_class, &channel)) {
		ie_info[ie_len] = ch_usage_info->bw20_op_class;
		ie_len++;
		/*
		 * TGbeR2-4.59.1: Operating Class = 81 (2.4 GHz/20 MHz)
		 * The channel number shall be device own channel setting.
		 */
		if (WMODE_CAP_2G(wdev->PhyMode))
			ie_info[ie_len] = wdev->channel;
		else
			ie_info[ie_len] = channel;
		ie_len++;
		valid = TRUE;
	}
	if (ch_usage_info->bw40_op_class
		&& get_ch_by_global_class(pAd, wdev, ch_usage_info->bw40_op_class, &channel)) {
		ie_info[ie_len] = ch_usage_info->bw40_op_class;
		ie_len++;
		ie_info[ie_len] = channel;
		ie_len++;
		valid = TRUE;
	}
	if (ch_usage_info->bw80_op_class
		&& get_ch_by_global_class(pAd, wdev, ch_usage_info->bw80_op_class, &channel)) {
		ie_info[ie_len] = ch_usage_info->bw80_op_class;
		ie_len++;
		ie_info[ie_len] = channel;
		ie_len++;
		valid = TRUE;
	}

	if (valid) {
		ie_info[1] = ie_len-2;
		ch_usage_info->ie_len = ie_len;
		hex_dump_with_cat_and_lvl("CH USAGE IE", ie_info, ie_len,
			DBG_CAT_CHN, CATCHN_CHN, /*DBG_LVL_INFO*/DBG_LVL_NOTICE);
	}
}


#endif /* DOT11_N_SUPPORT */

/* not support oct now */
VOID build_oci_common_field(
	struct _RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	unsigned char oct_support,
	unsigned char *buf,
	unsigned long *buf_len)
{
	/* Operating Class */
	buf[0] = get_regulatory_class(ad, wdev->channel, wdev->PhyMode, wdev);

	/* Primary Channel Number */
	buf[1] = (wpa3_test_ctrl == 12) ? wpa3_test_ctrl2 : wdev->channel;

	/* Frequency Segment 1 Channel Number */
	buf[2] = wlan_operate_get_cen_ch_2(wdev);

	*buf_len = 3;

	if (oct_support)
		; /* not support now */
}

VOID build_oci_ie(
	struct _RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	unsigned char *buf,
	unsigned long *buf_len)
{
	ULONG len;

	if (wdev == NULL)
		return;

	buf[0] = EID_EXTENSION;
	buf[2] = EID_EXT_OCI;

	build_oci_common_field(ad, wdev, FALSE, buf + 3, &len);
	buf[1] = len + 1;
	*buf_len = len + 3;

	hex_dump_with_cat_and_lvl("oci ie", buf,
		*buf_len, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_INFO);
}

UCHAR parse_oci_common_field(
	struct _RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	unsigned char oct_support,
	unsigned char *buf,
	unsigned long buf_len)
{
	UCHAR bw = wlan_operate_get_bw(wdev);
	UCHAR spacing;

	hex_dump_with_cat_and_lvl("peer oci", buf, buf_len, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_INFO);

	if (buf_len < 3) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
			"invalid oci len(%lu)\n",
			buf_len);
		return FALSE;
	}

	if (!is_channel_in_channelset_by_reg_class(ad, buf[0], wdev->PhyMode, wdev->channel)) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
			"operating class(%d) check fail, chanel isn't in channel list\n",
			buf[0]);
		return FALSE;
	}

	if (get_spacing_by_reg_class(ad, buf[0], wdev->PhyMode, &spacing) == FALSE) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
			"get reg_class table fail(op class = %d)\n",
			buf[0]);
		return FALSE;
	}

	if (bw > spacing) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
			"bandwidth mismatch (peer: %d, our: %d)\n",
			spacing, bw);
		return FALSE;
	}

	if (buf[1] != wdev->channel) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
			"primary ch(%d) check fail, primary ch(%d)\n",
			buf[1], wdev->channel);
		return FALSE;
	}

	if (buf[2] != wlan_operate_get_cen_ch_2(wdev)) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
			"second central ch(%d) check fail, second central ch(%d)\n",
			buf[2], wlan_operate_get_cen_ch_2(wdev));
		return FALSE;
	}

	if (!oct_support && buf_len > 3) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
			"oci field len is wrong\n");
		return FALSE;
	}

	return TRUE;
}

UCHAR parse_oci_ie(
	struct _RTMP_ADAPTER *ad,
	struct wifi_dev *wdev,
	unsigned char *buf,
	unsigned long buf_len)
{

	if (buf_len < MIN_OCI_LEN) {
		MTWF_DBG(ad, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR,
				"buf_len(%lu) is less than min oci len\n", buf_len);
		return FALSE;
	}

	if (buf[0] != EID_EXTENSION ||
		buf[2] != EID_EXT_OCI) {
		hex_dump_with_cat_and_lvl("oci_ie fail, peer oci ie", buf,
			buf_len, DBG_CAT_SEC, CATSEC_OCV, DBG_LVL_ERROR);
		return FALSE;
	}

	return parse_oci_common_field(ad, wdev, TRUE, buf + 3, buf[1] - 1);
}
