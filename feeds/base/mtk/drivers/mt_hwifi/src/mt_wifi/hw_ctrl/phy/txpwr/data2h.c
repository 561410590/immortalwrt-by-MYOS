/*
 * Copyright (c) [2020], MediaTek Inc. All rights reserved.
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws.
 * The information contained herein is confidential and proprietary to
 * MediaTek Inc. and/or its licensors.
 * Except as otherwise provided in the applicable licensing terms with
 * MediaTek Inc. and/or its licensors, any reproduction, modification, use or
 * disclosure of MediaTek Software, and information contained herein, in whole
 * or in part, shall be strictly prohibited.
*/
/*
 ***************************************************************************
 ***************************************************************************

*/

/*******************************************************************************
 *    INCLUDED FILES
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*******************************************************************************
 *    DEFINITIONS
 ******************************************************************************/
#define PATH_OF_SKU_TABLE_IN   "/hw_ctrl/phy/txpwr/sku_tables/"
#define PATH_OF_SKU_TABEL_OUT  "/include/hw_ctrl/phy/txpwr/"
#define HEAD_FILE_NAME_OF_SKU  "PowerLimit_sku.h"

#define INPUT_FILE_PATH_SIZE        512
#define OUTPUT_FILE_PATH_SIZE       512
#define DEFAULT_FILE_PATH_SIZE      512
#define VARIABLE_NAME_SIZE          128

#ifndef MAX_SKUTABLE_NUM
#define MAX_SKUTABLE_NUM             21
#endif

/*******************************************************************************
 *    TYPES
 ******************************************************************************/
typedef enum _POWER_LIMIT_FILE_TYPE {
	INPUT_FILE = 0,
	OUTPUT_FILE,
	DEFAULT_FILE,
	FILE_TYPE_NUM
} POWER_LIMIT_FILE_TYPE, *P_POWER_LIMIT_FILE_TYPE;

typedef enum _POWER_LIMIT_TABLE {
	POWER_LIMIT_TABLE_TYPE_SKU = 0,
	POWER_LIMIT_TABLE_TYPE_BACKOFF,
	POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP,
	POWER_LIMIT_TABLE_TYPE_CONSTRAINT = POWER_LIMIT_TABLE_TYPE_LG_OFDM_DUP,
	POWER_LIMIT_TABLE_TYPE_NUM
} POWER_LIMIT_TABLE, *P_POWER_LIMIT_TABLE;

typedef enum _FILE_TRANSFORM_STATE {
	FILE_TRANSFORM_PARSING_STATE = 0,
	FILE_TRANSFORM_CHECK_STATE,
	FILE_TRANSFORM_STATE_NUM
} FILE_TRANSFORM_STATE, *P_FILE_TRANSFORM_STATE;

/*******************************************************************************
 *    PUBLIC DATA
 ******************************************************************************/
char **_PwrLimitType;

int sku_table_num;
int sku_5mhz_shift;
/*******************************************************************************
 *    PRIVATE FUNCTIONS
 ******************************************************************************/
static int snprintf_error(size_t size, int res)
{
	return res < 0 || (unsigned int) res >= size;
}

void create_sku_table_array(char *outfname)
{
	char u4PwrLimitTblIdx = 0, u4PwrLimitType = 0;
	char *sku_table_array[POWER_LIMIT_TABLE_TYPE_NUM] = {
		"struct pwr_table_list *pcptrSkuTbl[] = {",
		"struct pwr_table_list *pcptrBackoffTbl[] = {",
		"struct pwr_table_list *pcptrLgOfdmDupTbl[] = {"};
	FILE *outfile = NULL;
	char **PowerLimitType;
	int rc = 0;

	/* get pointer array from global pointer */
	PowerLimitType = _PwrLimitType;

	/* Open output file */
	outfile = fopen(outfname, "a+");
	if (outfile == (FILE *) NULL) {
		perror("File opening failed");
		return;
	}

	for (u4PwrLimitType = 0; u4PwrLimitType < POWER_LIMIT_TABLE_TYPE_CONSTRAINT; u4PwrLimitType++) {
		rc = fprintf(outfile, "%s\n\t", sku_table_array[u4PwrLimitType]);
		if (rc < 0)
			printf("error fprintf\n");

		for (u4PwrLimitTblIdx = 1; u4PwrLimitTblIdx <= sku_table_num; u4PwrLimitTblIdx++) {
			rc = fprintf(outfile, "%s_%02d", *(PowerLimitType + u4PwrLimitType),
				u4PwrLimitTblIdx);
			if (rc < 0)
				printf("error fprintf\n");

			if (u4PwrLimitTblIdx == sku_table_num) {
				rc = fprintf(outfile, "};\n\n");
				if (rc < 0)
					printf("error fprintf\n");
			} else {
				rc = fprintf(outfile, (u4PwrLimitTblIdx % 5) ? ", " : ",\n\t");
				if (rc < 0)
					printf("error fprintf\n");
			}
		}
	}

	rc = fprintf(outfile, "\n#undef MAX_SKU_TABLE_NUM\n");
	if (rc < 0)
		printf("error fprintf\n");

	rc = fprintf(outfile, "\n#define MAX_SKU_TABLE_NUM		%d\n", sku_table_num);
	if (rc < 0)
		printf("error fprintf\n");

	rc = fprintf(outfile, "\n#undef AFC_STD_PWR_SKUTABLE_IDX");
	if (rc < 0)
		printf("error fprintf\n");

	rc = fprintf(outfile, "\n#define AFC_STD_PWR_SKUTABLE_IDX	%d\n", sku_table_num - 1);
	if (rc < 0)
		printf("error fprintf\n");

	if (fclose(outfile) != 0)
		printf("fclose error\n");

}

int DataToHeaderTrans(char *infname, char *outfname, char *varname, char *deffname, const char *mode)
{
	int ret = 0;
	FILE *infile, *outfile;
	int c;
	int fgDefTable = 0;

	/* Open input file */
	infile = fopen(infname, "r");

	/* Check open file status for input file */
	if (infile == (FILE *) NULL) {
		printf("Can't read file %s\n", infname);
		printf("System would automatically apply default table !!\n");

		/* Flag for use Default SKU table */
		fgDefTable = 1;

		/* Open default input file */
		infile = fopen(deffname, "r");

		/* Check open file status for default file */
		if (infile == (FILE *) NULL) {
			printf("Can't read default file %s\n", deffname);
			return -1;
		}
	}

	/* Open output file */
	outfile = fopen(outfname, mode);

	/* Check open file status for output file */
	if (outfile == (FILE *) NULL) {
		printf("Can't open write file %s\n", outfname);

		/* Close input file */
		if (fclose(infile) != 0)
			printf("fclose error\n");
		return -1;
	}

	if (strcmp(mode, "w") == 0) {
		ret = fprintf(outfile,
			"struct pwr_table_list {UINT8 chBand; UINT8 channel; INT8 pwrLimit[494];};\n");
		if (ret < 0)
			printf("error fprintf\n");
	}

	/* contents in header file */
	ret = fprintf(outfile, "struct pwr_table_list %s[] = ", varname);
	if (ret < 0)
		printf("error fprintf\n");

	while (1) {
		char cTempStringBuffer, cTempStringBuffer2;
		char cString[2];

		/* read one character from input file */
		c = getc(infile);

		if (c < 0)
			printf("\n");

		/* end-of-file check */
		if (c == EOF && feof(infile))
			break;

		/* backup character to temp buffer variable (prevent buffer used in "getc" to be re-assigned for function "sprintf") */
		cTempStringBuffer2 = c;

		if (c == '\n') {
			if (fputs("\t\n", outfile) < 0)
				printf("error fputs\n");
		} else {
			ret = snprintf(cString, sizeof(cString), "%c", cTempStringBuffer2);
			if (snprintf_error(sizeof(cString), ret))
				printf("snprintf error!\n");

			if (fputs(cString, outfile) < 0)
				printf("error fputs\n");
		}
	}

	/* end-of-file symbol processing */
	if (fputs("\n", outfile) < 0)
		printf("error fputs\n");

	/* end with new line for different variables */
	if (fputs("\n", outfile) < 0)
		printf("error fputs\n");

	/* close input file */
	if (fclose(infile) != 0)
		printf("fclose error\n");

	/* close output file */
	if (fclose(outfile) != 0)
		printf("fclose error\n");
	return 0;
}

int PowerLimitTypeDefine(void)
{
	char **PowerLimitType;
	char *Sku = NULL, *Backoff = NULL, *Dup = NULL;
	char SkuString[16] = "Sku";
	char BackoffString[16] = "Backoff";
	char LgOfdmDupString[16] = "Dup";
	int sRet;


	if (sku_5mhz_shift) {
		memcpy(SkuString, "Sku_5mhz\0", strlen("Sku_5mhz")+1);
		memcpy(BackoffString, "Backoff_5mhz\0", strlen("Backoff_5mhz")+1);
		memcpy(LgOfdmDupString, "Dup_5mhz\0", strlen("Dup_5mhz")+1);
	}

	/* allocate memory for pointer array for Power Limit Type string */
	PowerLimitType = (char **)malloc(sizeof(char *)*POWER_LIMIT_TABLE_TYPE_NUM);
	if (PowerLimitType == NULL) {
		printf("%s():allocate memory for PowerLimitType failed!\n", __func__);
		return 1;
	}

	/* allocate memory for Sku string */
	Sku = (char *)malloc(sizeof(char)*(strlen(SkuString) + 1));
	if (Sku == NULL) {
		printf("%s():allocate memory for Sku failed!\n", __func__);
		free(*PowerLimitType);
		return 1;
	}
	sRet = snprintf(Sku, (strlen(SkuString) + 1), "%s", SkuString);
	if (snprintf_error(strlen(SkuString) + 1, sRet))
		printf("snprintf error!\n");
	*PowerLimitType = Sku;

	/* allocate memory for Backoff string */
	Backoff = (char *)malloc(sizeof(char)*(strlen(BackoffString) + 1));
	if (Backoff == NULL) {
		printf("%s():allocate memory for Backoff failed!\n", __func__);
		free(*PowerLimitType);
		free(Sku);
		return 1;
	}
	sRet = snprintf(Backoff, (strlen(BackoffString) + 1), "%s", BackoffString);
	if (snprintf_error((strlen(BackoffString) + 1), sRet))
		printf("snprintf error!\n");
	*(PowerLimitType + 1) = Backoff;

	/* allocate memory for LG OFDM DUP string */
	Dup = (char *)malloc(sizeof(char)*(strlen(LgOfdmDupString) + 1));
	if (Dup == NULL) {
		printf("%s():allocate memory for Dup failed!\n", __func__);
		free(*PowerLimitType);
		free(Sku);
		free(Backoff);
		return 1;
	}
	sRet = snprintf(Dup, (strlen(LgOfdmDupString) + 1), "%s", LgOfdmDupString);
	if (snprintf_error((strlen(LgOfdmDupString) + 1), sRet))
		printf("snprintf error!\n");
	*(PowerLimitType + 2) = Dup;

	/* Link pointer array to global pointer for further reference */
	_PwrLimitType = PowerLimitType;
	return 0;
}

void PowerLimitTypeFree(void)
{
	char **PowerLimitType;

	/* get pointer array from global pointer */
	PowerLimitType = _PwrLimitType;

	/* free memory for Sku String */
	free(*PowerLimitType);

	/* free memory for Backoff String */
	free(*(PowerLimitType + 1));

	/* free memory for Dup String */
	free(*(PowerLimitType + 2));

	/* free memory for pointer array for Power Limit Type string */
	free(PowerLimitType);
}

char *PowerLimitTypeString(int u4Type)
{
	/* get pointer array from global pointer */
	char **PowerLimitType;
	PowerLimitType = _PwrLimitType;

	return *(PowerLimitType + u4Type);
}

void FilePathArrangeProc(char *pcFilePath, int u4FilePathSize, char *EnvironDirectoryPath, char *DirectoryPath, int u4Type, int u4PwrLimitType, int u4PwrLimitTblIdx)
{
	char cTempStringBuffer[20];
	int sRet = 0;

	/* configure file address and file name */
	memset(pcFilePath, 0, u4FilePathSize);
	sRet = snprintf(pcFilePath, u4FilePathSize, "%s", EnvironDirectoryPath);
	if (snprintf_error(u4FilePathSize, sRet))
		printf("snprintf error!\n");

	sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath), "%s", DirectoryPath);
	if (snprintf_error(strlen(pcFilePath), sRet))
		printf("snprintf error!\n");

	switch (u4Type) {
	case INPUT_FILE:
		sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath),
			"%s", PowerLimitTypeString(u4PwrLimitType));
		if (snprintf_error(strlen(pcFilePath), sRet))
			printf("snprintf error!\n");

		sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath), "%s", "_");
		if (snprintf_error(strlen(pcFilePath), sRet))
			printf("snprintf error!\n");

		sRet = snprintf(cTempStringBuffer, 3, "%02d", u4PwrLimitTblIdx);
		if (snprintf_error(3, sRet))
			printf("snprintf error!\n");

		sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath), "%s", cTempStringBuffer);
		if (snprintf_error(strlen(pcFilePath), sRet))
			printf("snprintf error!\n");

		sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath), "%s", ".dat");
		if (snprintf_error(strlen(pcFilePath), sRet))
			printf("snprintf error!\n");

		printf("Input: [%d] %s\n", u4PwrLimitTblIdx, pcFilePath);
		break;

	case OUTPUT_FILE:
		sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath),
			 "%s", HEAD_FILE_NAME_OF_SKU);
		if (snprintf_error(strlen(pcFilePath), sRet))
			printf("snprintf error!\n");

		printf("Output: %s\n", pcFilePath);
		break;

	case DEFAULT_FILE:
		sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath),
		 "%s", PowerLimitTypeString(u4PwrLimitType));
		if (snprintf_error(strlen(pcFilePath), sRet))
			printf("snprintf error!\n");

		sRet = snprintf(pcFilePath + strlen(pcFilePath), u4FilePathSize - strlen(pcFilePath), "%s", "_default.dat");
		if (snprintf_error(strlen(pcFilePath), sRet))
			printf("snprintf error!\n");

		printf("Def Input: [%d] %s\n", u4PwrLimitTblIdx, pcFilePath);
		break;

	default:
		break;
	}
}

void VariableNameArrangeProc(char *pcVariableName, int u4VariableNameSize, int u4PwrLimitType, int u4PwrLimitTblIdx)
{
	char cTempStringBuffer[20];
	int sRet = 0;

	memset(pcVariableName, 0, u4VariableNameSize);
	sRet = snprintf(pcVariableName, u4VariableNameSize, "%s", PowerLimitTypeString(u4PwrLimitType));
	if (snprintf_error(u4VariableNameSize, sRet))
		printf("snprintf error!\n");

	sRet = snprintf(pcVariableName + strlen(pcVariableName), u4VariableNameSize - strlen(pcVariableName), "%s", "_");
	if (snprintf_error(strlen(pcVariableName), sRet))
		printf("snprintf error!\n");

	sRet = snprintf(cTempStringBuffer, 3, "%02d", u4PwrLimitTblIdx);
	if (snprintf_error(3, sRet))
		printf("snprintf error!\n");

	sRet = snprintf(pcVariableName + strlen(pcVariableName), u4VariableNameSize - strlen(pcVariableName),
	 "%s", cTempStringBuffer);
	if (snprintf_error(strlen(pcVariableName), sRet))
		printf("snprintf error!\n");

}

int main(int argc, char *argv[])
{
	char infname[INPUT_FILE_PATH_SIZE];
	char outfname[OUTPUT_FILE_PATH_SIZE];
	char deffname[DEFAULT_FILE_PATH_SIZE];
	char varname[VARIABLE_NAME_SIZE];
	char *top_dir, *psku_num = NULL, *_5mhz = NULL;
	int  u4PwrLimitTblIdx = 0, u4PwrLimitType = 0;
	int  fgStart;

	sku_table_num = MAX_SKUTABLE_NUM;
	sku_5mhz_shift = 0;

	/* get directory path from environemnt variable */
	top_dir = (char *)getenv("TOP_DIR");
	if (top_dir == NULL)
		return -1;

	_5mhz =	(char *)getenv("SKU_5MHZ_SHIFT");
	if (_5mhz != NULL)
		printf("\x1b[41m_5mhz = %s\x1b[m\n", _5mhz);
	if (_5mhz != NULL && !strncmp(_5mhz, "y", 1)) {
		//sku_5mhz_shift = strtol(_5mhz, 0, 10);
		sku_5mhz_shift = 1;// strtol(_5mhz, 0, 10);
		printf("\x1b[41msku_5mhz_shift = %d\x1b[m\n", sku_5mhz_shift);
	}

	/* get directory path from environemnt variable */
	psku_num = (char *)getenv("MAX_SKU_TABLE_NUM");
	if (psku_num != NULL) {
		sku_table_num = strtol(psku_num, 0, 10);
		if (sku_table_num > MAX_SKUTABLE_NUM)
			sku_table_num = MAX_SKUTABLE_NUM;
		printf("\x1b[41msku_table_num = %d\x1b[m\n", sku_table_num);
	}

	/* Power Limit Type String Define */
	PowerLimitTypeDefine();

	/* configure output file address and file name */
	FilePathArrangeProc(outfname, OUTPUT_FILE_PATH_SIZE, top_dir, PATH_OF_SKU_TABEL_OUT, OUTPUT_FILE, u4PwrLimitType, u4PwrLimitTblIdx);

	/* Trasform SKU table data file to header file */
	for (u4PwrLimitType = 0, fgStart = 1; u4PwrLimitType < POWER_LIMIT_TABLE_TYPE_CONSTRAINT; u4PwrLimitType++) {
		for (u4PwrLimitTblIdx = 1; u4PwrLimitTblIdx <= sku_table_num; u4PwrLimitTblIdx++) {
			/* configure input file address and file name */
			FilePathArrangeProc(infname, INPUT_FILE_PATH_SIZE, top_dir, PATH_OF_SKU_TABLE_IN, INPUT_FILE, u4PwrLimitType, u4PwrLimitTblIdx);

			/* configure default input file address and file name */
			FilePathArrangeProc(deffname, DEFAULT_FILE_PATH_SIZE, top_dir, PATH_OF_SKU_TABLE_IN, DEFAULT_FILE, u4PwrLimitType, u4PwrLimitTblIdx);

			/* Configure variable name for SKU contents in header file */
			VariableNameArrangeProc(varname, VARIABLE_NAME_SIZE, u4PwrLimitType, u4PwrLimitTblIdx);

			/* Transform data file to header file */
			if (fgStart) {
				/* disable first round flag */
				fgStart = 0;
				/* create output file and write corresponding power limit table */
				DataToHeaderTrans(infname, outfname, varname, deffname, "w");
			} else
				/* append corresponding power limit table to existed output file */
				DataToHeaderTrans(infname, outfname, varname, deffname, "a+");
		}
	}

	create_sku_table_array(outfname);

	/* Power Limit Type String Free */
	PowerLimitTypeFree();
}
