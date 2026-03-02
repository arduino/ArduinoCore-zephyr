/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <strings.h>
#include <zephyr/llext/symbol.h>
#include <zephyr/usb/usb_device.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <zephyr/kernel.h>

#define FORCE_EXPORT_SYM(name) \
       extern void name(void); \
       EXPORT_SYMBOL(name);

/*
 * Libc functions are exported with a __real_ prefix so that the sketch
 * core can provide long-call trampolines under the original names.
 * This avoids R_ARM_THM_JUMP24 / R_ARM_THM_CALL relocations that would
 * fail when the LLEXT is loaded in a different memory region (>16 MB away).
 */
#define EXPORT_LIBC_SYM(name) EXPORT_SYMBOL_NAMED(name, __real_##name)

// string.h
EXPORT_LIBC_SYM(memcpy);
EXPORT_LIBC_SYM(memmove);
EXPORT_LIBC_SYM(strrchr);
EXPORT_LIBC_SYM(strstr);
EXPORT_LIBC_SYM(strncmp);
EXPORT_LIBC_SYM(strncpy);
EXPORT_LIBC_SYM(strcasecmp);
EXPORT_LIBC_SYM(strcmp);
EXPORT_LIBC_SYM(strlen);
EXPORT_LIBC_SYM(strnlen);
EXPORT_LIBC_SYM(strchr);
EXPORT_LIBC_SYM(strcat);

// stdlib.h
EXPORT_LIBC_SYM(malloc);
EXPORT_LIBC_SYM(realloc);
EXPORT_LIBC_SYM(calloc);
EXPORT_LIBC_SYM(free);
EXPORT_LIBC_SYM(rand);
EXPORT_LIBC_SYM(srand);
EXPORT_LIBC_SYM(strtod);
EXPORT_LIBC_SYM(strtol);
EXPORT_LIBC_SYM(strtoul);
EXPORT_LIBC_SYM(atoi);
EXPORT_LIBC_SYM(atof);
EXPORT_LIBC_SYM(atol);
EXPORT_LIBC_SYM(abort);
EXPORT_LIBC_SYM(exit);
EXPORT_LIBC_SYM(atexit);
extern void _exit(int);
EXPORT_LIBC_SYM(_exit);

// ctype.h
EXPORT_LIBC_SYM(isspace);
EXPORT_LIBC_SYM(isalnum);
EXPORT_LIBC_SYM(tolower);
EXPORT_LIBC_SYM(toupper);
EXPORT_LIBC_SYM(isalpha);
EXPORT_LIBC_SYM(iscntrl);
EXPORT_LIBC_SYM(isdigit);
EXPORT_LIBC_SYM(isgraph);
EXPORT_LIBC_SYM(isprint);
EXPORT_LIBC_SYM(isupper);
EXPORT_LIBC_SYM(islower);
EXPORT_LIBC_SYM(isxdigit);

// math.h
EXPORT_LIBC_SYM(acos);
EXPORT_LIBC_SYM(acosf);
EXPORT_LIBC_SYM(asin);
EXPORT_LIBC_SYM(asinf);
EXPORT_LIBC_SYM(atan);
EXPORT_LIBC_SYM(atan2);
EXPORT_LIBC_SYM(atan2f);
EXPORT_LIBC_SYM(atanf);
EXPORT_LIBC_SYM(cos);
EXPORT_LIBC_SYM(cosf);
EXPORT_LIBC_SYM(exp);
EXPORT_LIBC_SYM(exp2);
EXPORT_LIBC_SYM(log);
EXPORT_LIBC_SYM(logf);
EXPORT_LIBC_SYM(log2);
EXPORT_LIBC_SYM(log10);
EXPORT_LIBC_SYM(pow);
EXPORT_LIBC_SYM(sin);
EXPORT_LIBC_SYM(sinf);
EXPORT_LIBC_SYM(sqrt);
EXPORT_LIBC_SYM(sqrtf);
EXPORT_LIBC_SYM(tan);
EXPORT_LIBC_SYM(tanf);

// stdio.h
EXPORT_LIBC_SYM(puts);
EXPORT_LIBC_SYM(putchar);
EXPORT_LIBC_SYM(vsnprintf);

EXPORT_SYMBOL(k_malloc);
EXPORT_SYMBOL(k_free);
EXPORT_SYMBOL(k_sched_lock);
EXPORT_SYMBOL(k_sched_unlock);

#if defined(CONFIG_USB_DEVICE_STACK)
EXPORT_SYMBOL(usb_enable);
EXPORT_SYMBOL(usb_disable);
#endif

#if CONFIG_LOG
EXPORT_SYMBOL(z_log_msg_runtime_vcreate);
FORCE_EXPORT_SYM(log_const_sketch)
#endif

#if defined(CONFIG_LOG_RUNTIME_FILTERING)
FORCE_EXPORT_SYM(log_dynamic_sketch)
#endif

#if defined(CONFIG_NETWORKING)
FORCE_EXPORT_SYM(net_if_foreach);
FORCE_EXPORT_SYM(net_if_down);
FORCE_EXPORT_SYM(net_if_up);
FORCE_EXPORT_SYM(net_if_get_by_iface);
#if defined(CONFIG_NET_IPV4)
FORCE_EXPORT_SYM(net_if_ipv4_maddr_add);
FORCE_EXPORT_SYM(net_if_ipv4_maddr_join);
FORCE_EXPORT_SYM(net_if_ipv4_set_gw);
FORCE_EXPORT_SYM(net_if_ipv4_addr_add);
FORCE_EXPORT_SYM(net_if_ipv4_set_netmask);
FORCE_EXPORT_SYM(net_if_ipv4_set_netmask_by_addr);
#endif
FORCE_EXPORT_SYM(net_if_lookup_by_dev);
#endif

#if defined(CONFIG_NET_L2_ETHERNET)
FORCE_EXPORT_SYM(_net_l2_ETHERNET);
FORCE_EXPORT_SYM(net_mgmt_NET_REQUEST_ETHERNET_SET_MAC_ADDRESS);
#endif

#if defined(CONFIG_NET_DHCPV4)
FORCE_EXPORT_SYM(net_dhcpv4_start);
#if defined(CONFIG_NET_DHCPV4_OPTION_CALLBACKS)
FORCE_EXPORT_SYM(net_dhcpv4_add_option_callback);
#endif
#endif

#if defined(CONFIG_NET_DHCPV4_SERVER)
FORCE_EXPORT_SYM(net_dhcpv4_server_start);
#endif

#if defined(CONFIG_NET_MGMT_EVENT)
FORCE_EXPORT_SYM(net_mgmt_add_event_callback);
FORCE_EXPORT_SYM(net_mgmt_event_wait_on_iface);
#endif

#if defined(CONFIG_MBEDTLS)
FORCE_EXPORT_SYM(tls_credential_add);
FORCE_EXPORT_SYM(tls_credential_get);
#endif

#if defined(CONFIG_WIFI)
FORCE_EXPORT_SYM(net_if_get_wifi_sta);
FORCE_EXPORT_SYM(net_if_get_wifi_sap);
FORCE_EXPORT_SYM(net_mgmt_NET_REQUEST_WIFI_CONNECT);
FORCE_EXPORT_SYM(net_mgmt_NET_REQUEST_WIFI_IFACE_STATUS);
FORCE_EXPORT_SYM(net_mgmt_NET_REQUEST_WIFI_AP_ENABLE);
#endif

#if defined(CONFIG_BT)
FORCE_EXPORT_SYM(bt_enable_raw);
FORCE_EXPORT_SYM(bt_send);
FORCE_EXPORT_SYM(bt_buf_get_tx);
FORCE_EXPORT_SYM(net_buf_simple_pull);
FORCE_EXPORT_SYM(net_buf_simple_add_mem);
FORCE_EXPORT_SYM(net_buf_simple_pull_mem);
FORCE_EXPORT_SYM(net_buf_unref);
#if defined(CONFIG_BT_HCI_SETUP)
FORCE_EXPORT_SYM(bt_h4_vnd_setup);
#endif
#if defined(CONFIG_CYW4343W_MURATA_1DX)
FORCE_EXPORT_SYM(brcm_patchram_buf);
FORCE_EXPORT_SYM(brcm_patch_ram_length);
#endif
#if defined(CONFIG_BT_LL_SW_SPLIT)
FORCE_EXPORT_SYM(bt_ctlr_set_public_addr);
#endif
#endif

#if defined(CONFIG_STACK_CANARIES)
FORCE_EXPORT_SYM(__stack_chk_guard);
FORCE_EXPORT_SYM(__stack_chk_fail);
#endif

#if defined(CONFIG_VIDEO)
FORCE_EXPORT_SYM(video_buffer_aligned_alloc);
FORCE_EXPORT_SYM(video_buffer_alloc);
FORCE_EXPORT_SYM(video_buffer_release);
FORCE_EXPORT_SYM(video_set_ctrl);
#endif
#if defined(CONFIG_INPUT)
FORCE_EXPORT_SYM(zephyr_input_register_callback);
#endif

#if defined(CONFIG_SHARED_MULTI_HEAP)
FORCE_EXPORT_SYM(shared_multi_heap_aligned_alloc);
FORCE_EXPORT_SYM(shared_multi_heap_free);
#endif

#if defined(CONFIG_NET_SOCKETS)
FORCE_EXPORT_SYM(getaddrinfo);
FORCE_EXPORT_SYM(freeaddrinfo)
FORCE_EXPORT_SYM(socket);
FORCE_EXPORT_SYM(connect);
FORCE_EXPORT_SYM(send);
FORCE_EXPORT_SYM(recv);
FORCE_EXPORT_SYM(open);
FORCE_EXPORT_SYM(close);
FORCE_EXPORT_SYM(accept);
FORCE_EXPORT_SYM(bind);
FORCE_EXPORT_SYM(listen);
FORCE_EXPORT_SYM(inet_pton);
FORCE_EXPORT_SYM(sendto);
FORCE_EXPORT_SYM(recvfrom);
FORCE_EXPORT_SYM(setsockopt);
FORCE_EXPORT_SYM(getpeername);
FORCE_EXPORT_SYM(inet_ntop);
#endif

#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
FORCE_EXPORT_SYM(cdc_acm_dte_rate_callback_set);
#endif

#if defined(CONFIG_USB_DEVICE_STACK_NEXT)
FORCE_EXPORT_SYM(usbd_init);
FORCE_EXPORT_SYM(usbd_add_descriptor);
FORCE_EXPORT_SYM(usbd_device_set_bcd_usb);
FORCE_EXPORT_SYM(usbd_msg_register_cb);
FORCE_EXPORT_SYM(usbd_device_set_code_triple);
FORCE_EXPORT_SYM(usbd_register_all_classes);
FORCE_EXPORT_SYM(usbd_add_configuration);
FORCE_EXPORT_SYM(usbd_caps_speed);
FORCE_EXPORT_SYM(usbd_can_detect_vbus);
FORCE_EXPORT_SYM(usbd_enable);
FORCE_EXPORT_SYM(usbd_disable);
#endif

#if defined(CONFIG_SHARED_MULTI_HEAP)
FORCE_EXPORT_SYM(shared_multi_heap_alloc);
#endif

EXPORT_SYMBOL(k_timer_init);
EXPORT_SYMBOL(k_fatal_halt);
EXPORT_SYMBOL(k_work_schedule);
//FORCE_EXPORT_SYM(k_timer_user_data_set);
//FORCE_EXPORT_SYM(k_timer_start);

EXPORT_SYMBOL(printf);
EXPORT_SYMBOL(sprintf);
EXPORT_SYMBOL(snprintf);
EXPORT_SYMBOL(cbvprintf);
EXPORT_SYMBOL(sscanf);
FORCE_EXPORT_SYM(__assert_no_args);
EXPORT_SYMBOL(stdin);
EXPORT_SYMBOL(stdout);
EXPORT_SYMBOL(stderr);

#if defined(CONFIG_RING_BUFFER)
EXPORT_SYMBOL(ring_buf_get);
EXPORT_SYMBOL(ring_buf_peek);
EXPORT_SYMBOL(ring_buf_put);
EXPORT_SYMBOL(ring_buf_area_claim);
EXPORT_SYMBOL(ring_buf_area_finish);
#endif

EXPORT_SYMBOL(sys_clock_cycle_get_32);
FORCE_EXPORT_SYM(__aeabi_dcmpun);
FORCE_EXPORT_SYM(__aeabi_dcmple);
FORCE_EXPORT_SYM(__aeabi_d2lz);
FORCE_EXPORT_SYM(__aeabi_uldivmod);
FORCE_EXPORT_SYM(__aeabi_ui2d);
FORCE_EXPORT_SYM(__aeabi_dcmplt);
FORCE_EXPORT_SYM(__aeabi_ddiv);
FORCE_EXPORT_SYM(__aeabi_dmul);
FORCE_EXPORT_SYM(__aeabi_d2f);
FORCE_EXPORT_SYM(__aeabi_fcmpun);
FORCE_EXPORT_SYM(__aeabi_dadd);
FORCE_EXPORT_SYM(__aeabi_fcmple);
FORCE_EXPORT_SYM(__aeabi_idiv);
FORCE_EXPORT_SYM(__aeabi_dcmpgt);
FORCE_EXPORT_SYM(__aeabi_dsub);
FORCE_EXPORT_SYM(__aeabi_i2d);
FORCE_EXPORT_SYM(__aeabi_uidiv);
FORCE_EXPORT_SYM(__aeabi_l2d);
FORCE_EXPORT_SYM(__aeabi_d2uiz);
FORCE_EXPORT_SYM(__aeabi_uidivmod);
FORCE_EXPORT_SYM(__aeabi_dcmpeq);
FORCE_EXPORT_SYM(__aeabi_d2iz);
FORCE_EXPORT_SYM(__aeabi_f2d);
FORCE_EXPORT_SYM(__aeabi_ul2d);
FORCE_EXPORT_SYM(__aeabi_l2f);
FORCE_EXPORT_SYM(__aeabi_idivmod);
FORCE_EXPORT_SYM(__aeabi_ldivmod);
FORCE_EXPORT_SYM(__aeabi_ul2f);
FORCE_EXPORT_SYM(__aeabi_dcmpge);

#if defined (CONFIG_CPP)
FORCE_EXPORT_SYM(__cxa_pure_virtual);
#endif

#if defined(CONFIG_BOARD_ARDUINO_UNO_Q)
FORCE_EXPORT_SYM(matrixBegin);
FORCE_EXPORT_SYM(matrixWrite);
FORCE_EXPORT_SYM(matrixPlay);
FORCE_EXPORT_SYM(matrixGrayscaleWrite);
FORCE_EXPORT_SYM(matrixSetGrayscaleBits);
FORCE_EXPORT_SYM(matrixEnd);
#endif

#if defined(CONFIG_FLASH)
FORCE_EXPORT_SYM(flash_area_open);
FORCE_EXPORT_SYM(flash_area_read);
FORCE_EXPORT_SYM(flash_area_write);
FORCE_EXPORT_SYM(flash_area_erase);
FORCE_EXPORT_SYM(flash_area_close);
#endif

#if defined(CONFIG_FILE_SYSTEM)
FORCE_EXPORT_SYM(fs_open);
FORCE_EXPORT_SYM(fs_close);
FORCE_EXPORT_SYM(fs_unlink);
FORCE_EXPORT_SYM(fs_rename);
FORCE_EXPORT_SYM(fs_read);
FORCE_EXPORT_SYM(fs_write);
FORCE_EXPORT_SYM(fs_seek);
FORCE_EXPORT_SYM(fs_tell);
FORCE_EXPORT_SYM(fs_truncate);
FORCE_EXPORT_SYM(fs_sync);
FORCE_EXPORT_SYM(fs_mkdir);
FORCE_EXPORT_SYM(fs_opendir);
FORCE_EXPORT_SYM(fs_readdir);
FORCE_EXPORT_SYM(fs_closedir);
FORCE_EXPORT_SYM(fs_mount);
FORCE_EXPORT_SYM(fs_unmount);
FORCE_EXPORT_SYM(fs_readmount);
FORCE_EXPORT_SYM(fs_stat);
FORCE_EXPORT_SYM(fs_statvfs);
#if defined(CONFIG_FILE_SYSTEM_MKFS)
FORCE_EXPORT_SYM(fs_mkfs);
#endif
FORCE_EXPORT_SYM(fs_register);
FORCE_EXPORT_SYM(fs_unregister);
#endif
