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

#ifndef __STATIC_ID_HDR__
#define __STATIC_ID_HDR__

#include "rt_config.h"

void *static_id_alloc(int size);
void static_id_free(void *ctx);
int static_id_store(void *ctx, unsigned long id, void *data, void (*)(void *));
int static_id_erase(void *ctx, unsigned long id);
void *static_id_load(void *ctx, unsigned long id);

#endif /* __STATIC_ID_HDR__*/
