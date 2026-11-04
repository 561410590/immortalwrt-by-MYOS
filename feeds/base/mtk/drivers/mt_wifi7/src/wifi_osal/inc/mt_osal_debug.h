/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/* Copyright (C) 2024 MediaTek Inc. */


#ifndef __MT_OSAL_DEBUG_H_
#define __MT_OSAL_DEBUG_H_

int mt_debug_print(const char *fmt, ...);
int mt_debug_err(const char *fmt, ...);
int mt_debug_warn(const char *fmt, ...);
int mt_debug_notice(const char *fmt, ...);
int mt_debug_info(const char *fmt, ...);
int mt_debug_dbg(const char *fmt, ...);

#ifndef SUBMODULE
#define SUBMODULE
#endif

#define MT_DEBUG_WARN(fmt, args...)	mt_debug_warn( SUBMODULE ":%s:%d:" fmt,	\
							__func__,		\
							__LINE__,		\
							## args)

#define MT_DEBUG_ERROR(fmt, args...)	mt_debug_err( SUBMODULE ":%s:%d:" fmt,	\
							__func__,		\
							__LINE__,		\
							## args)


#define MT_ASSERT(__condition, __return_val, fmt, args...)			\
		if ( __condition ) {						\
			MT_DEBUG_ERROR(fmt ", called func addr:%p",		\
				## args, __builtin_return_address(0));		\
			return __return_val;					\
		}

#ifdef MT_DEBUG
#define MT_DEBUG_NOTICE(fmt, args...)	mt_debug_notice( SUBMODULE ":%s:%d:" fmt,	\
							__func__,		\
							__LINE__,		\
							## args)

#define MT_DEBUG_INFO(fmt, args...)	mt_debug_info( SUBMODULE ":%s:%d:" fmt,	\
							__func__,		\
							__LINE__,		\
							## args)

#define MT_DEBUG_TRACE(fmt, args...)	mt_debug_dbg( SUBMODULE ":%s:%d:" fmt,	\
							__func__,		\
							__LINE__,		\
							## args)

#define MT_DEBUG_TRACE_IN(fmt, args...)	mt_debug_dbg( SUBMODULE ":%s:%d: --> " fmt " called addr:%p",   \
							__func__,		\
							__LINE__,		\
							## args,		\
							__builtin_return_address(0))

#define MT_DEBUG_TRACE_OUT(fmt, args...) mt_debug_dbg( SUBMODULE ":%s:%d: <-- " fmt ,   \
							__func__,		\
							__LINE__,		\
							## args,		\
							__builtin_return_address(0))

#else
#define MT_DEBUG_NOTICE(fmt, args...)
#define MT_DEBUG_INFO(fmt, args...)
#define MT_DEBUG_TRACE(fmt, args...)
#define MT_DEBUG_TRACE_IN(fmt, args...)
#define MT_DEBUG_TRACE_OUT(fmt, args...)
#endif /* MT_DEBUG */

#endif /* __MT_OSAL_DEBUG_H_ */
