/*
 * Copyright (c) 2010 Joseph Gaeddert
 * Copyright (c) 2010 Virginia Polytechnic Institute & State University
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// framestats_tx.cc
//
// frame statistics transmitter
//

#include <iostream>
#include <complex>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <liquid/liquid.h>

#include "usrp_io.h"
 
#define USRP_CHANNEL        (0)

void usage() {
    printf("flexframe_tx:\n");
    printf("  f     :   center frequency [Hz]\n");
    printf("  b     :   bandwidth [Hz]\n");
    printf("  g     :   transmit power gain [dB] (default -3dB)\n");
    printf("  t     :   run time [seconds]\n");
    printf("  n     :   payload length (bytes)\n");
    printf("  m     :   mod. scheme: <psk>, dpsk, ask, qam, apsk...\n");
    printf("  p     :   mod. depth: <1>,2,...8\n");
    printf("  s     :   packet spacing <0>\n");
    printf("  r     :   ramp up/dn length <64>\n");
    printf("  c     :   fec coding scheme (inner)\n");
    printf("  k     :   fec coding scheme (outer)\n");
    // print all available FEC schemes
    unsigned int i;
    for (i=0; i<LIQUID_FEC_NUM_SCHEMES; i++)
        printf("              %s\n", fec_scheme_str[i][0]);
    printf("  q     :   quiet\n");
    printf("  v     :   verbose\n");
    printf("  u,h   :   usage/help\n");
}

int main (int argc, char **argv)
{
    bool verbose = true;

    float min_bandwidth = (32e6 / 512.0);
    float max_bandwidth = (32e6 /   4.0);

    float frequency = 462.0e6;
    float bandwidth = min_bandwidth;
    float num_seconds = 5.0f;
    float gmin_dB   = -25.0f;
    float gmax_dB   =   0.0f;
    float txgain_dB = gmax_dB;

    unsigned int packet_spacing=0;
    unsigned int payload_len=200;
    crc_scheme check = LIQUID_CRC_32;
    fec_scheme fec0 = LIQUID_FEC_NONE;
    fec_scheme fec1 = LIQUID_FEC_HAMMING74;
    modulation_scheme mod_scheme = LIQUID_MODEM_QAM;
    unsigned int mod_depth = 2;
    unsigned int ramp_len = 64;

    //
    int d;
    while ((d = getopt(argc,argv,"f:b:g:t:n:m:p:s:r:c:k:qvuh")) != EOF) {
        switch (d) {
        case 'f':   frequency = atof(optarg);       break;
        case 'b':   bandwidth = atof(optarg);       break;
        case 'g':   txgain_dB = atof(optarg);       break;
        case 't':   num_seconds = atof(optarg);     break;
        case 'n':   payload_len = atoi(optarg);     break;
        case 'm':
            mod_scheme = liquid_getopt_str2mod(optarg);
            if (mod_scheme == LIQUID_MODEM_UNKNOWN) {
                printf("error: unknown/unsupported mod. scheme: %s\n", optarg);
                mod_scheme = LIQUID_MODEM_UNKNOWN;
            }
            break;
        case 'p':   mod_depth = atoi(optarg);       break;
        case 's':   packet_spacing = atoi(optarg);  break;
        case 'r':   ramp_len = atoi(optarg);        break;
        case 'c':   fec0 = liquid_getopt_str2fec(optarg);         break;
        case 'k':   fec1 = liquid_getopt_str2fec(optarg);         break;
        case 'q':   verbose = false;                break;
        case 'v':   verbose = true;                 break;
        case 'u':
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (bandwidth > max_bandwidth) {
        printf("error: maximum bandwidth exceeded (%8.4f MHz)\n", max_bandwidth*1e-6);
        return 0;
    } else if (bandwidth < min_bandwidth) {
        printf("error: minimum bandwidth exceeded (%8.4f kHz)\n", min_bandwidth*1e-3);
        return 0;
    } else if (payload_len > (1<<16)) {
        printf("error: maximum payload length exceeded: %u > %u\n", payload_len, 1<<16);
        return 0;
    } else if (fec0 == LIQUID_FEC_UNKNOWN || fec1 == LIQUID_FEC_UNKNOWN) {
        usage();
        return 0;
    } else if (mod_scheme == LIQUID_MODEM_UNKNOWN) {
        usage();
        return 0;
    }

    printf("frequency   :   %12.8f [MHz]\n", frequency*1e-6f);
    printf("bandwidth   :   %12.8f [kHz]\n", bandwidth*1e-3f);
    printf("tx gain     :   %12.8f [dB]\n", txgain_dB);
    printf("verbosity   :   %s\n", (verbose?"enabled":"disabled"));

    // create usrp_io object and set properties
    usrp_io * uio = new usrp_io();
    uio->set_tx_freq(USRP_CHANNEL, frequency);
    uio->set_tx_samplerate(2.0f*bandwidth);
    uio->enable_auto_tx(USRP_CHANNEL);

    // retrieve tx port
    gport port_tx = uio->get_tx_port(USRP_CHANNEL);

    // packetizer
    packetizer p = packetizer_create(payload_len,check,fec0,fec1);
    unsigned int packet_len = packetizer_compute_enc_msg_len(payload_len,check,fec0,fec1);
    packetizer_print(p);

    // create flexframegen object
    flexframegenprops_s fgprops;
    fgprops.rampup_len = ramp_len;
    fgprops.phasing_len = 64;
    fgprops.payload_len = packet_len;
    fgprops.mod_scheme = mod_scheme;
    fgprops.mod_bps = mod_depth;
    fgprops.rampdn_len = ramp_len;
    flexframegen fg = flexframegen_create(&fgprops);
    flexframegen_print(fg);


    // framing buffers
    unsigned int frame_len = flexframegen_getframelen(fg);
    std::complex<float> frame[frame_len];
    std::complex<float> mfbuffer[2*frame_len];

    printf("frame length        :   %u\n", frame_len);

    unsigned int num_blocks = (unsigned int)((4.0f*bandwidth*num_seconds)/(4*frame_len));

    // create pulse-shaping interpolator
    unsigned int m=3;
    float beta=0.7f;
    interp_crcf mfinterp = interp_crcf_create_rnyquist(LIQUID_RNYQUIST_RRC,2,m,beta,0);

    // data buffers
    unsigned char header[9];
    unsigned char payload[payload_len];
    unsigned char packet[packet_len];

    // start usrp data transfer
    uio->start_tx(USRP_CHANNEL);

    // transmitter gain (linear)
    float gstep_dB = 0.1f;
    float g = powf(10.0f, txgain_dB/10.0f);
 
    unsigned int i, j, pid=0;
    // start transmitter
    for (i=0; i<num_blocks; i++) {
        // generate the frame / transmit silence
        if ((i%(packet_spacing+1))==0) {
            // generate random tx gain
            txgain_dB -= gstep_dB;
            if (txgain_dB < gmin_dB)
                txgain_dB += gmax_dB - gmin_dB;
            g = powf(10.0f, txgain_dB/10.0f);

            // generate random data
            // TODO : encode using forward error-correction codec
            for (j=0; j<payload_len; j++)
                payload[j] = rand() % 256;
            // assemble packet
            packetizer_encode(p,payload,packet);
            // write header
            header[0] = (pid >> 8) & 0xff;
            header[1] = (pid     ) & 0xff;
            header[2] = (payload_len >> 8) & 0xff;
            header[3] = (payload_len     ) & 0xff;
            header[4] = (unsigned char)(fec0);
            header[5] = (unsigned char)(fec1);
            if (verbose)
                printf("packet id: %6u\n", pid);
                //printf("packet id: %6u, packet len : %6u\n", pid, packet_len);
            /*
            for (j=0; j<payload_len; j++)
                printf("%.2x ",payload[j]);
            printf("\n");
            for (j=0; j<packet_len; j++)
                printf("%.2x ",packet[j]);
            printf("\n");
            */
            pid = (pid+1) & 0xffff;

            flexframegen_execute(fg, header, packet, frame);

            // interpolate using matched filter
            for (j=0; j<frame_len; j++) {
                frame[j] *= g;
                std::complex<float> x = j<frame_len ? frame[j] : 0.0f;
                interp_crcf_execute(mfinterp, x, &mfbuffer[2*j]);
            }

        } else {
            // flush interpolator with zeros
            for (j=0; j<frame_len; j++)
                interp_crcf_execute(mfinterp, 0.0f, &mfbuffer[2*j]);
        }

        // send data to usrp via port
        gport_produce(port_tx,(void*)mfbuffer,2*frame_len);
 
    }
 
 
    uio->stop_tx(USRP_CHANNEL);  // Stop data transfer

    // clean it up
    packetizer_destroy(p);
    flexframegen_destroy(fg);
    interp_crcf_destroy(mfinterp);
    delete uio;
    return 0;
}

