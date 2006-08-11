/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <trellis_viterbi_combined_b.h>
#include <gr_io_signature.h>
#include <assert.h>
#include <iostream>
  
static const float INF = 1.0e9;

trellis_viterbi_combined_b_sptr 
trellis_make_viterbi_combined_b (
    const fsm &FSM,
    const int D,
    const std::vector<float> &TABLE,
    const int K,
    const int S0,
    const int SK,
    const trellis_metric_type_t TYPE)
{
  return trellis_viterbi_combined_b_sptr (new trellis_viterbi_combined_b (FSM,D,TABLE,K,S0,SK,TYPE));
}

trellis_viterbi_combined_b::trellis_viterbi_combined_b (
    const fsm &FSM,
    const int D,
    const std::vector<float> &TABLE,
    const int K,
    const int S0,
    const int SK,
    const trellis_metric_type_t TYPE)
  : gr_block ("viterbi_combined_b",
			  gr_make_io_signature (1, -1, sizeof (float)),
			  gr_make_io_signature (1, -1, sizeof (unsigned char))),  
  d_FSM (FSM),
  d_D (D),
  d_TABLE (TABLE),
  d_K (K),
  d_S0 (S0),
  d_SK (SK),
  d_TYPE (TYPE),
  d_trace(FSM.S()*K)
{
    set_relative_rate (1.0 / ((double) d_D));
    set_output_multiple (d_K);
}


void
trellis_viterbi_combined_b::forecast (int noutput_items, gr_vector_int &ninput_items_required)
{
  assert (noutput_items % d_K == 0);
  int input_required =  d_D * noutput_items ;
  unsigned ninputs = ninput_items_required.size();
  for (unsigned int i = 0; i < ninputs; i++) {
    ninput_items_required[i] = input_required;
  }
}




void viterbi_algorithm_combined(const int I, const int S, const int O, 
             const std::vector<int> &NS,
             const std::vector<int> &OS,
             const std::vector<int> &PS,
             const std::vector<int> &PI,
             const int D,
             const std::vector<float> &TABLE,
             const int K,
             const int S0,const int SK,
             const trellis_metric_type_t TYPE,
             const float *in, unsigned char *out,
             std::vector<int> &trace) 
{
  std::vector<float> alpha(S*2);
  float *metric = new float[O];
  int alphai;
  float norm,mm,minm;
  int minmi;
  int st;

  if(S0<0) { // initial state not specified
      for(int i=0;i<S;i++) alpha[0*S+i]=0;
  }
  else {
      for(int i=0;i<S;i++) alpha[0*S+i]=INF;
      alpha[0*S+S0]=0.0;
  }

  alphai=0;
  for(int k=0;k<K;k++) {
      calc_metric_f(O, D, TABLE, &(in[k*D]), metric,TYPE); // calc metrics
      norm=INF;
      for(int j=0;j<S;j++) { // for each next state do ACS
          minm=INF;
          minmi=0;
          for(int i=0;i<I;i++) {
              int i0 = j*I+i;
              if((mm=alpha[alphai*S+PS[i0]]+metric[OS[PS[i0]*I+PI[i0]]])<minm)
                  minm=mm,minmi=i;
          }
          trace[k*S+j]=minmi;
          alpha[((alphai+1)%2)*S+j]=minm;
          if(minm<norm) norm=minm;
      }
      for(int j=0;j<S;j++) 
          alpha[((alphai+1)%2)*S+j]-=norm; // normalize total metrics so they do not explode
      alphai=(alphai+1)%2;
  }

  if(SK<0) { // final state not specified
      minm=INF;
      minmi=0;
      for(int i=0;i<S;i++)
          if((mm=alpha[alphai*S+i])<minm) minm=mm,minmi=i;
      st=minmi;
  }
  else {
      st=SK;
  }

  for(int k=K-1;k>=0;k--) { // traceback
      int i0=st*I+trace[k*S+st];
      out[k]= (unsigned char) PI[i0];
      st=PS[i0];
  }
  
  delete [] metric;

}






int
trellis_viterbi_combined_b::general_work (int noutput_items,
                        gr_vector_int &ninput_items,
                        gr_vector_const_void_star &input_items,
                        gr_vector_void_star &output_items)
{
  assert (input_items.size() == output_items.size());
  int nstreams = input_items.size();
  assert (noutput_items % d_K == 0);
  int nblocks = noutput_items / d_K;

  for (int m=0;m<nstreams;m++) {
    const float *in = (const float *) input_items[m];
    unsigned char *out = (unsigned char *) output_items[m];
    for (int n=0;n<nblocks;n++) {
      viterbi_algorithm_combined(d_FSM.I(),d_FSM.S(),d_FSM.O(),d_FSM.NS(),d_FSM.OS(),d_FSM.PS(),d_FSM.PI(),d_D,d_TABLE,d_K,d_S0,d_SK,d_TYPE,&(in[n*d_K*d_D]),&(out[n*d_K]),d_trace);
    }
  }

  consume_each (d_D * noutput_items );
  return noutput_items;
}
