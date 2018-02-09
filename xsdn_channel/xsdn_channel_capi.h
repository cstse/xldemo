#ifndef _XSDN_CHANNEL_CAPI_H_20180208_
#define _XSDN_CHANNEL_CAPI_H_20180208_

#include <stdint.h>

#ifndef XSDN_CHANNEL_EXTERN_C
    #ifdef __cplusplus	
        #define XSDN_CHANNEL_EXTERN_C extern "C"
        #define XSDN_CHANNEL_BEGIN_EXTERN_C extern "C"{
        #define XSDN_CHANNEL_END_EXTERN_C }
    #else
        #define XSDN_CHANNEL_EXTERN_C 
        #define XSDN_CHANNEL_BEGIN_EXTERN_C
        #define XSDN_CHANNEL_END_EXTERN_C
    #endif // __cplusplus
#endif // XSDN_CHANNEL_EXTERN_C

#if defined(__GNUC__)
    #define XSDN_CHANNEL_API(x) XSDN_CHANNEL_EXTERN_C __attribute__((__visibility__("default")/*, __stdcall__*/)) x
#elif defined(_MSC_VER)
    #define XSDN_CHANNEL_API(x) XSDN_CHANNEL_EXTERN_C __declspec(dllexport) x /*__stdcall*/ 
#else
	#error "unsupport compiler!!!"
#endif // complier

#define XSDN_CHANNEL_INVALID_HANDLE ((void*)-1)
#define XSDN_CHANNEL_DECLARE_HANDLE(name) struct __XSDN_CHANNEL_SAFE_HANDLE_##name { int unused; }; typedef struct __XSDN_CHANNEL_SAFE_HANDLE_##name *name

XSDN_CHANNEL_BEGIN_EXTERN_C

// NOTE: use `xc` is short for xsdn channel.

typedef int xc_bool_t;

// initialize the libaray.
// @param[in]: root_dir - xc's root dir where configuration files located.
// @param[in]: peerid - local peerid.
// @param[in]: peerid_len - local peerid length.
// @return: 0 on success, other on fail.
XSDN_CHANNEL_API(int) xc_init(const char* root_dir, const char* peerid, int peerid_len);

// finalize the library
XSDN_CHANNEL_API(int) xc_uninit();

// test if a handle is valid.
#define xc_handle_is_valid(h) (((void*)(h)) != XSDN_CHANNEL_INVALID_HANDLE)

// `xc_channel_t` is handle refers a data channel which is full duplex.
XSDN_CHANNEL_DECLARE_HANDLE(xc_channel_t);

// `xc_acceptor_t` is handle refers a data channel acceptor that can create passive `xc_channel_t`.
XSDN_CHANNEL_DECLARE_HANDLE(xc_acceptor_t);

// prototype of channel acceptor's error callback.
// @param: self - this acceptor handle.
// @param: errcode - the error code.
typedef void (*xc_acceptor_on_error_callback_t)(xc_acceptor_t self, int errcode);

// prototype of channel acceptor's new channel callback.
// @param: self - this acceptor handle.
// @param: new_channel - the new(passive) channel has been created just now.
typedef void (*xc_acceptor_on_channel_callback_t)(xc_acceptor_t self, xc_channel_t new_channel);


// prototype of channel's error callback.
// @param: self - this channel handle.
// @param: errcode - the error code.
typedef void (*xc_channel_on_error_callback_t)(xc_channel_t self, int errcode);

// prototype of channel's data receiving callback.
// @param: self - this channel handle.
// @param: data - the data's pointer.
// @param: data_len - the data length.
typedef void (*xc_channel_on_recv_callback_t)(xc_channel_t self, const char* data, int data_len);

// prototype of channel's sending queue writable callback.
// !!!NOTE!!!: this could ONLY happen when the sending queue become available FROM FULL state!
// @param: self - this channel handle.
typedef void (*xc_channel_on_writable_callback_t)(xc_channel_t self);

// open a channel acceptor to accept passive channel.
// @param[in]: vport - the virtual service port NOT take effect now(2018-02-08), 0 is recommended.
// @param[in]: err_cb - this acceptor's error callback routine.
// @param[in]: new_cb - this acceptor's new channel callback routine.
// @param[in]: new_channel_err_cb - this new passive channel's error callback routine.
// @param[in]: new_channel_recv_cb - this new passive channel's receiving callback routine.
// @param[in]: new_channel_writable_cb - this new passive channel's writable event callback routine.
// @return: the created acceptor handle, `XSDN_CHANNEL_INVALID_HANDLE` on error.
XSDN_CHANNEL_API(xc_acceptor_t) xc_open_acceptor(uint16_t vport,
    xc_acceptor_on_error_callback_t err_cb,
    xc_acceptor_on_channel_callback_t new_cb,
    xc_channel_on_error_callback_t new_channel_err_cb,
    xc_channel_on_recv_callback_t new_channel_recv_cb,
    xc_channel_on_writable_callback_t new_channel_writable_cb);

// close a acceptor.
// @param[in]: self - the acceptor handle.
XSDN_CHANNEL_API(void) xc_close_acceptor(xc_acceptor_t self);

// open a full-duplex data channel.
// @param[in]: dst_peerid - the destination peerid which to connect to.
// @param[in]: dst_peerid_len - the `dst_peerid`'s length.
// @param[in]: vport - the virtual service port NOT take effect now(2018-02-08), 0 is recommended.
// @param[in]: err_cb - this channel's error callback routine.
// @param[in]: recv_cb - this channel's receiving callback routine.
// @param[in]: writable_cb - this channel's writable event callback routine.
XSDN_CHANNEL_API(xc_channel_t) xc_open_channel(const char* dst_peerid, int dst_peerid_len, uint16_t vport,
    xc_channel_on_error_callback_t err_cb,
    xc_channel_on_recv_callback_t recv_cb,
    xc_channel_on_writable_callback_t writable_cb);

// close a channel.
// @param[in]: self - the channel handle.
XSDN_CHANNEL_API(void) xc_close_channel(xc_channel_t self);

// get a channel's ID.
// @param[in]: self - the channel handle.
// @return: the channel's ID.
XSDN_CHANNEL_API(uint64_t) xc_channel_id(xc_channel_t self);

// send some data through a channel.
// @param[in]: self - the channel handle.
// @param[in]: data - the pointer to the data to be sent.
// @param[in]: data_len - the length of the data to be sent.
// @return: 0 on success, other on fail - ERRCODE_CHANNEL_SEND_LIST_FULL(2001) may be the regular return value.
XSDN_CHANNEL_API(int) xc_channel_send(xc_channel_t self, const char* data, int data_len);

// get data in speed from a channel.
// @param[in]: self - the channel handle.
// @return: the data in speed(Bytes/s) of the channel.
XSDN_CHANNEL_API(uint64_t) xc_channel_data_in_speed(xc_channel_t self);

// get data out speed from a channel.
// @param[in]: self - the channel handle.
// @return: the data out speed(Bytes/s) of the channel.
XSDN_CHANNEL_API(uint64_t) xc_channel_data_out_speed(xc_channel_t self);

// get protocol(header) in speed from a channel.
// @param[in]: self - the channel handle.
// @return: the protocol(header) in speed(Bytes/s) of the channel.
XSDN_CHANNEL_API(uint64_t) xc_channel_proto_in_speed(xc_channel_t self);

// get protocol(header) out speed from a channel.
// @param[in]: self - the channel handle.
// @return: the protocol(header) out speed(Bytes/s) of the channel.
XSDN_CHANNEL_API(uint64_t) xc_channel_proto_out_speed(xc_channel_t self);

// get RTT(round trip time) from a channel.
// @param[in]: self - the channel handle.
// @return: the RTT(ms) of the channel.
XSDN_CHANNEL_API(uint32_t) xc_channel_rtt(xc_channel_t self);

// get SRTT(Smooth round trip time) of a channel.
// @param[in]: self - the channel handle.
// @return: the SRTT(ms) of the channel.
XSDN_CHANNEL_API(uint32_t) xc_channel_srtt(xc_channel_t self);

// get destination peerid of a channel.
// @param[in]: self - the channel handle.
// @param[out]: peerid_out - a pointer to a buffer that will store the peerid.
// @param[in/out]: peerid_len - if not null, [in] gives the buffer length, [out] the exact peerid length.
// @return: the exact peerid length.
// @example: if you can't make sure how long the peerid will be:
//     int real_len = xc_channel_dst_peerid(channel, NULL, NULL);
//     char* buf = (char*)malloc(real_len);
//     xc_channel_dst_peerid(channel, buf, &real_len);
XSDN_CHANNEL_API(int) xc_channel_dst_peerid(xc_channel_t self, char* peerid_out, int* peerid_len /*in/out*/);

XSDN_CHANNEL_END_EXTERN_C

#endif//_XSDN_CHANNEL_CAPI_H_20180208_
