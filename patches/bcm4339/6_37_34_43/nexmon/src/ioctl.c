/***************************************************************************
 *                                                                         *
 *          ###########   ###########   ##########    ##########           *
 *         ############  ############  ############  ############          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ###########   ####  ######  ##   ##   ##  ##    ######          *
 *          ###########  ####  #       ##   ##   ##  ##    #    #          *
 *                   ##  ##    ######  ##   ##   ##  ##    #    #          *
 *                   ##  ##    #       ##   ##   ##  ##    #    #          *
 *         ############  ##### ######  ##   ##   ##  ##### ######          *
 *         ###########    ###########  ##   ##   ##   ##########           *
 *                                                                         *
 *            S E C U R E   M O B I L E   N E T W O R K I N G              *
 *                                                                         *
 * Warning:                                                                *
 *                                                                         *
 * Our software may damage your hardware and may void your hardware’s      *
 * warranty! You use our tools at your own risk and responsibility!        *
 *                                                                         *
 * License:                                                                *
 * Copyright (c) 2015 NexMon Team                                          *
 *                                                                         *
 * Permission is hereby granted, free of charge, to any person obtaining   *
 * a copy of this software and associated documentation files (the         *
 * "Software"), to deal in the Software without restriction, including     *
 * without limitation the rights to use, copy, modify, merge, publish,     *
 * distribute copies of the Software, and to permit persons to whom the    *
 * Software is furnished to do so, subject to the following conditions:    *
 *                                                                         *
 * The above copyright notice and this permission notice shall be included *
 * in all copies or substantial portions of the Software.                  *
 *                                                                         *
 * Any use of the Software which results in an academic publication or     *
 * other publication which includes a bibliography must include a citation *
 * to the author's publication "M. Schulz, D. Wegemer and M. Hollick.      *
 * NexMon: A Cookbook for Firmware Modifications on Smartphones to Enable  *
 * Monitor Mode.".                                                         *
 *                                                                         *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  *
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY    *
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,    *
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE       *
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                  *
 *                                                                         *
 **************************************************************************/

#pragma NEXMON targetregion "patch"

#include <firmware_version.h>   // definition of firmware version macros
#include <debug.h>              // contains macros to access the debug hardware
#include <wrapper.h>            // wrapper definitions for functions that already exist in the firmware
#include <structs.h>            // structures that are used by the code in the firmware
#include <helper.h>             // useful helper functions
#include <patcher.h>            // macros used to craete patches such as BLPatch, BPatch, ...
#include <rates.h>              // rates used to build the ratespec for frame injection
#include <nexioctls.h>          // ioctls added in the nexmon patch
#include <capabilities.h>       // capabilities included in a nexmon patch
#include <sendframe.h>          // sendframe functionality

struct beacon {
  char dummy[40];
  char ssid_len;
  char ssid[32];
} beacon = {
  .dummy = {
    0x80, 0x00, 0x00, 0x00, 0xff, 
    0xff, 0xff, 0xff, 0xff, 0xff, 
    0x00, 0x11, 0x22, 0x33, 0x44, 
    0x55, 0x00, 0x11, 0x22, 0x33, 
    0x44, 0x55, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x01, 0x01, 0x82, 0x00
  },
  .ssid_len = 5,
  .ssid = { 'H', 'E', 'L', 'L', 'O' }
};

void
send_beacon(struct wlc_info *wlc)
{
    int len = sizeof(beacon) - 32 + beacon.ssid_len;
    sk_buff *p = pkt_buf_get_skb(wlc->osh, len + 202);
    struct beacon *beacon_skb;
    beacon_skb = (struct beacon *) skb_pull(p, 202);
    memcpy(beacon_skb, &beacon, len);
    sendframe(wlc, p, 1, 0);
}

struct hndrte_timer *t = 0;

void
timer_handler(struct hndrte_timer * t)
{
    send_beacon(t->data);
}

int
handle_ct_ioctl(struct wlc_info *wlc, char *arg, int len)
{
    if (!t) {
        t = hndrte_init_timer(0, wlc, timer_handler, 0);
        hndrte_add_timer(t, 100, 1);
    }

    if (len > 0 && *arg != 0) {
        arg[len - 1] = 0;

        printf("IOCTL: %s\n", arg);

        memcpy(beacon.ssid, arg, len - 1);
        beacon.ssid_len = len - 1;
    }

    return IOCTL_SUCCESS;
}

int 
wlc_ioctl_hook(struct wlc_info *wlc, int cmd, char *arg, int len, void *wlc_if)
{
    int ret = IOCTL_ERROR;

    switch (cmd) {
        case NEX_GET_CAPABILITIES:
            if (len == 4) {
                memcpy(arg, &capabilities, 4);
                ret = IOCTL_SUCCESS;
            }
            break;

        case NEX_WRITE_TO_CONSOLE:
            if (len > 0) {
                arg[len-1] = 0;
                printf("ioctl: %s\n", arg);
                ret = IOCTL_SUCCESS; 
            }
            break;

        case NEX_CT_EXPERIMENTS:
            ret = handle_ct_ioctl(wlc, arg, len);
            break;

        default:
            ret = wlc_ioctl(wlc, cmd, arg, len, wlc_if);
    }

    return ret;
}

__attribute__((at(0x1F3488, "", CHIP_VER_BCM4339, FW_VER_6_37_32_RC23_34_43_r639704)))
GenericPatch4(wlc_ioctl_hook, wlc_ioctl_hook + 1);
