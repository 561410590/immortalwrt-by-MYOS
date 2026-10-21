#ifndef __OP_CLASS_H__
#define __OP_CLASS_H__

#define OPERATING_CLASS_GLOBAL	0x04
#define OPERATING_CLASS_LOCAL	0x20

VOID build_oci_common_field(
	IN struct _RTMP_ADAPTER *ad,
	IN struct wifi_dev *wdev,
	IN UCHAR oct_support,
	OUT UCHAR *buf,
	OUT ULONG *buf_len);

VOID build_oci_ie(
	IN struct _RTMP_ADAPTER *ad,
	IN struct wifi_dev *wdev,
	OUT UCHAR *buf,
	OUT ULONG *buf_len);

UCHAR parse_oci_common_field(
	IN struct _RTMP_ADAPTER *ad,
	IN struct wifi_dev *wdev,
	IN UCHAR oct_support,
	IN UCHAR *buf,
	IN ULONG buf_len);

UCHAR parse_oci_ie(
	IN struct _RTMP_ADAPTER *ad,
	IN struct wifi_dev *wdev,
	IN UCHAR *buf,
	IN ULONG buf_len);


#endif /* __OP_CLASS_H__ */
