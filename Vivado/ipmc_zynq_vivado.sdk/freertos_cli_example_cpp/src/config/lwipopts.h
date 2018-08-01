/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
void ipmc_lwip_printf(const char *ctrl1, ...);
#ifdef __cplusplus
}
#endif

/* Configurationg specific for Zynq 7-series w/ FreeRTOS */
/* Based on:
 *  1) http://www.wiki.xilinx.com/Zynq-7000+AP+SoC+Performance+%E2%80%93+Gigabit+Ethernet+achieving+the+best+performance
 *  2) https://www.xilinx.com/support/documentation/application_notes/xapp1026.pdf */
#define PROCESSOR_LITTLE_ENDIAN		1
#define OS_IS_FREERTOS				1
#define SYS_LIGHTWEIGHT_PROT		1

#define TCPIP_THREAD_NAME			"_lwipd"
#define TCPIP_THREAD_PRIO			3 // TODO: Needs to be less than the socket app and more than EMAC driver - configLWIP_TASK_PRIORITY
#define TCPIP_THREAD_XEMACIFD_PRIO	(TCPIP_THREAD_PRIO)
#define TCPIP_THREAD_HIGH_PRIO		(TCPIP_THREAD_PRIO)
#define TCPIP_THREAD_STACKSIZE		1024 // TODO: Consider put somewhere else

#define DEFAULT_TCP_RECVMBOX_SIZE	200
#define DEFAULT_ACCEPTMBOX_SIZE		5
#define TCPIP_MBOX_SIZE				200
#define DEFAULT_UDP_RECVMBOX_SIZE	100
#define DEFAULT_RAW_RECVMBOX_SIZE	30

#define LWIP_COMPAT_MUTEX 0
#define LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT 1

#define LWIP_TCP_KEEPALIVE			1

#define LWIP_PROVIDE_ERRNO

// Unclear if this help or does harm, offload seems to be ON by default anyway
#define CHECKSUM_GEN_TCP 	0
#define CHECKSUM_GEN_UDP 	0
#define CHECKSUM_GEN_IP  	0
#define CHECKSUM_CHECK_TCP  1
#define CHECKSUM_CHECK_UDP  1
#define CHECKSUM_CHECK_IP 	1

#define MEMP_SEPARATE_POOLS		1
#define MEMP_NUM_FRAG_PBUF		256
#define IP_OPTIONS_ALLOWED		0
#define TCP_OVERSIZE			TCP_MSS

#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1

#define CONFIG_LINKSPEED_AUTODETECT 1


/* SSI options. */
//#define LWIP_DEBUG
#define NO_SYS							0
#define LWIP_SOCKET						(NO_SYS==0)
#define LWIP_COMPAT_SOCKETS             0
#define LWIP_NETCONN              		1
#define LWIP_SNMP						0
#define LWIP_IGMP						0
#define LWIP_ICMP						1
#define LWIP_DNS						1

/* -------- Debugging options -------- */
#ifdef LWIP_DEBUG
#define LWIP_DBG_MIN_LEVEL        LWIP_DBG_LEVEL_ALL // LWIP_DBG_LEVEL_SERIOUS
#define PPP_DEBUG                  LWIP_DBG_OFF
#define MEM_DEBUG                  LWIP_DBG_OFF
#define MEMP_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                 LWIP_DBG_OFF
#define API_LIB_DEBUG              LWIP_DBG_OFF
#define API_MSG_DEBUG              LWIP_DBG_OFF
#define TCPIP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                LWIP_DBG_OFF
#define SOCKETS_DEBUG              LWIP_DBG_OFF
#define DNS_DEBUG                  LWIP_DBG_OFF
#define AUTOIP_DEBUG               LWIP_DBG_OFF
#define DHCP_DEBUG                 LWIP_DBG_OFF
#define IP_DEBUG                   LWIP_DBG_OFF
#define IP_REASS_DEBUG             LWIP_DBG_OFF
#define ICMP_DEBUG                 LWIP_DBG_OFF
#define IGMP_DEBUG                 LWIP_DBG_OFF
#define UDP_DEBUG                  LWIP_DBG_OFF
#define TCP_DEBUG                  LWIP_DBG_OFF
#define TCP_INPUT_DEBUG            LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG           LWIP_DBG_OFF
#define TCP_RTO_DEBUG              LWIP_DBG_OFF
#define TCP_CWND_DEBUG             LWIP_DBG_OFF
#define TCP_WND_DEBUG              LWIP_DBG_OFF
#define TCP_FR_DEBUG               LWIP_DBG_OFF
#define TCP_QLEN_DEBUG             LWIP_DBG_OFF
#define TCP_RST_DEBUG              LWIP_DBG_OFF
#endif

#define LWIP_DBG_TYPES_ON         (LWIP_DBG_ON|LWIP_DBG_TRACE|LWIP_DBG_STATE|LWIP_DBG_FRESH|LWIP_DBG_HALT)


/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
/* MSVC port: intel processors don't need 4-byte alignment,
   but are faster that way! */
#define MEM_ALIGNMENT			64

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE				0x100000 // 1 MByte

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF			2048

/* MEMP_NUM_RAW_PCB: the number of UDP protocol control blocks. One
   per active RAW "connection". */
#define LWIP_RAW				0
#define MEMP_NUM_RAW_PCB		0

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB		256

/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB		128

/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN 16

/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG		1024

/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT	8

/* The following four are used only with the sequential API and can be
   set to 0 if the application only will use the raw API. */
/* MEMP_NUM_NETBUF: the number of struct netbufs. */
#define MEMP_NUM_NETBUF         8

/* MEMP_NUM_NETCONN: the number of struct netconns. */
#define MEMP_NUM_NETCONN        16

/* MEMP_NUM_TCPIP_MSG_*: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#define MEMP_NUM_TCPIP_MSG_API   32
#define MEMP_NUM_TCPIP_MSG_INPKT 128

#define MEMP_NUM_ARP_QUEUE		5

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#define PBUF_POOL_SIZE			4096

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
#define PBUF_POOL_BUFSIZE		1700

/* PBUF_LINK_HLEN: the number of bytes that should be allocated for a
   link level header. */
#define PBUF_LINK_HLEN			16

/* ---------- TCP options ---------- */
#define LWIP_TCP				1
#define TCP_TTL					255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ			1

/* TCP Maximum segment size. */
#define TCP_MSS					1460

/* TCP sender buffer space (bytes). */
#define TCP_SND_BUF				65535

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#define TCP_SND_QUEUELEN		( ( 16 * TCP_SND_BUF ) / TCP_MSS )

/* TCP writable space (bytes). This must be less than or equal
   to TCP_SND_BUF. It is the amount of space which must be
   available in the tcp snd_buf for select to return writable */
#define TCP_SNDLOWAT			(TCP_SND_BUF/2)

/* TCP receive window. */
#define TCP_WND					65535

/* Maximum number of retransmissions of data segments. */
#define TCP_MAXRTX				12

/* Maximum number of retransmissions of SYN segments. */
#define TCP_SYNMAXRTX			4


/* ---------- ARP options ---------- */
#define LWIP_ARP				1
#define ARP_TABLE_SIZE			10
#define ARP_QUEUEING			1

#define IP_OPTIONS 0

/* ---------- IP options ---------- */
/* Define IP_FORWARD to 1 if you wish to have the ability to forward
   IP packets across network interfaces. If you are going to run lwIP
   on a device with only one network interface, define this to 0. */
#define IP_FORWARD				0

/* IP reassembly and segmentation.These are orthogonal even
 * if they both deal with IP fragments */
#define IP_REASSEMBLY			1
#define IP_REASS_MAX_PBUFS		50 // If problems, change to 5760
#define MEMP_NUM_REASSDATA		15
#define IP_FRAG					1
#define IP_FRAG_MAX_MTU			1500
#define LWIP_CHKSUM_ALGORITHM	3


/* ---------- ICMP options ---------- */
#define ICMP_TTL				255


/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. */
#define LWIP_DHCP				1

/* 1 if you want to do an ARP check on the offered address
   (recommended). */
#define DHCP_DOES_ARP_CHECK		(LWIP_DHCP) // Xilinx on their BSP had this to 0


/* ---------- AUTOIP options ------- */
#define LWIP_AUTOIP				0
#define LWIP_DHCP_AUTOIP_COOP	(LWIP_DHCP && LWIP_AUTOIP)


/* ---------- UDP options ---------- */
#define LWIP_UDP				1
#define LWIP_UDPLITE			1
#define UDP_TTL					255


/* ---------- Statistics options ---------- */
#define LWIP_STATS				1
#define LWIP_STATS_DISPLAY		0

#if LWIP_STATS
	#define LINK_STATS				0
	#define IP_STATS				0
	#define ICMP_STATS				0
	#define IGMP_STATS				0
	#define IPFRAG_STATS			0
	#define UDP_STATS				1
	#define TCP_STATS				1
	#define MEM_STATS				0
	#define MEMP_STATS				0
	#define PBUF_STATS				0
	#define SYS_STATS				0
#endif /* LWIP_STATS */


/* ---------- PPP options ---------- */

#define PPP_SUPPORT			 0	  /* Set > 0 for PPP */

#if PPP_SUPPORT

	#define NUM_PPP					1	  /* Max PPP sessions. */

	/* Select modules to enable.  Ideally these would be set in the makefile but
	 * we're limited by the command line length so you need to modify the settings
	 * in this file.
	 */
	#define PPPOE_SUPPORT			1
	#define PPPOS_SUPPORT			1
	#define PAP_SUPPORT				1	  /* Set > 0 for PAP. */
	#define CHAP_SUPPORT			1	  /* Set > 0 for CHAP. */
	#define MSCHAP_SUPPORT			0	  /* Set > 0 for MSCHAP (NOT FUNCTIONAL!) */
	#define CBCP_SUPPORT			0	  /* Set > 0 for CBCP (NOT FUNCTIONAL!) */
	#define CCP_SUPPORT				0	  /* Set > 0 for CCP (NOT FUNCTIONAL!) */
	#define VJ_SUPPORT				1	  /* Set > 0 for VJ header compression. */
	#define MD5_SUPPORT				1	  /* Set > 0 for MD5 (see also CHAP) */

#endif /* PPP_SUPPORT */

#endif /* __LWIPOPTS_H__ */
