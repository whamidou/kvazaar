/*****************************************************************************
 * This file is part of Kvazaar HEVC encoder.
 *
 * Copyright (C) 2013-2015 Tampere University of Technology and others (see
 * COPYING file).
 *
 * Kvazaar is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * Kvazaar is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Kvazaar.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#include "filter.h"

#include <stdlib.h>

#include "cu.h"
#include "encoder.h"
#include "kvazaar.h"
#include "transform.h"
#include "videoframe.h"


//////////////////////////////////////////////////////////////////////////
// INITIALIZATIONS
const uint8_t kvz_g_tc_table_8x8[54] =
{
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  1,  1,
   1,  1,  1,  1,  1,  1,  1,  2,  2,  2,
   2,  3,  3,  3,  3,  4,  4,  4,  5,  5,
   6,  6,  7,  8,  9, 10, 11, 13, 14, 16,
  18, 20, 22, 24
};

const uint8_t kvz_g_beta_table_8x8[52] =
{
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  6,  7,  8,  9,
  10, 11, 12, 13, 14, 15, 16, 17, 18, 20,
  22, 24, 26, 28, 30, 32, 34, 36, 38, 40,
  42, 44, 46, 48, 50, 52, 54, 56, 58, 60,
  62, 64
};

const int8_t kvz_g_luma_filter[4][8] =
{
  {  0, 0,   0, 64,  0,   0, 0,  0 },
  { -1, 4, -10, 58, 17,  -5, 1,  0 },
  { -1, 4, -11, 40, 40, -11, 4, -1 },
  {  0, 1,  -5, 17, 58, -10, 4, -1 }
};

const int8_t kvz_g_chroma_filter[8][4] =
{
  {  0, 64,  0,  0 },
  { -2, 58, 10, -2 },
  { -4, 54, 16, -2 },
  { -6, 46, 28, -4 },
  { -4, 36, 36, -4 },
  { -4, 28, 46, -6 },
  { -2, 16, 54, -4 },
  { -2, 10, 58, -2 }
};

//////////////////////////////////////////////////////////////////////////
// FUNCTIONS

/**
 * \brief
 */
static INLINE void kvz_filter_deblock_luma(const encoder_control_t * const encoder,
                                           kvz_pixel *src,
                                           int32_t offset,
                                           int32_t tc,
                                           int8_t sw,
                                           int8_t part_P_nofilter,
                                           int8_t part_Q_nofilter,
                                           int32_t thr_cut,
                                           int8_t filter_second_P,
                                           int8_t filter_second_Q)
{
  int32_t delta;

  int16_t m0 = src[-offset * 4];
  int16_t m1 = src[-offset * 3];
  int16_t m2 = src[-offset * 2];
  int16_t m3 = src[-offset];
  int16_t m4 = src[0];
  int16_t m5 = src[offset];
  int16_t m6 = src[offset * 2];
  int16_t m7 = src[offset * 3];

  if (sw) {
    src[-offset * 3] = CLIP(m1 - 2*tc, m1 + 2*tc, (2*m0 + 3*m1 +   m2 +   m3 +   m4 + 4) >> 3);
    src[-offset * 2] = CLIP(m2 - 2*tc, m2 + 2*tc, (  m1 +   m2 +   m3 +   m4        + 2) >> 2);
    src[-offset]     = CLIP(m3 - 2*tc, m3 + 2*tc, (  m1 + 2*m2 + 2*m3 + 2*m4 +   m5 + 4) >> 3);
    src[0]           = CLIP(m4 - 2*tc, m4 + 2*tc, (  m2 + 2*m3 + 2*m4 + 2*m5 +   m6 + 4) >> 3);
    src[offset]      = CLIP(m5 - 2*tc, m5 + 2*tc, (  m3 +   m4 +   m5 +   m6        + 2) >> 2);
    src[offset * 2]  = CLIP(m6 - 2*tc, m6 + 2*tc, (  m3 +   m4 +   m5 + 3*m6 + 2*m7 + 4) >> 3);
  } else {
    // Weak filter
    delta = (9*(m4 - m3) - 3*(m5 - m2) + 8) >> 4;

    if (abs(delta) < thr_cut) {
      int32_t tc2  = tc >> 1;
      delta        = CLIP(-tc, tc, delta);
      src[-offset] = CLIP(0, (1 << encoder->bitdepth) - 1, (m3 + delta));
      src[0]       = CLIP(0, (1 << encoder->bitdepth) - 1, (m4 - delta));

      if(filter_second_P) {
        int32_t delta1   = CLIP(-tc2, tc2, (((m1 + m3 + 1) >> 1) - m2 + delta) >> 1);
        src[-offset * 2] = CLIP(0, (1 << encoder->bitdepth) - 1, m2 + delta1);
      }
      if(filter_second_Q) {
        int32_t delta2 = CLIP(-tc2, tc2, (((m6 + m4 + 1) >> 1) - m5 - delta) >> 1);
        src[offset]    = CLIP(0, (1 << encoder->bitdepth) - 1, m5 + delta2);
      }
    }
  }

  if(part_P_nofilter) {
    src[-offset]     = (kvz_pixel)m3;
    src[-offset * 2] = (kvz_pixel)m2;
    src[-offset * 3] = (kvz_pixel)m1;
  }
  if(part_Q_nofilter) {
    src[0]          = (kvz_pixel)m4;
    src[offset]     = (kvz_pixel)m5;
    src[offset * 2] = (kvz_pixel)m6;
  }
}

/**
 * \brief
 */
static INLINE void kvz_filter_deblock_chroma(const encoder_control_t * const encoder,
                                             kvz_pixel *src,
                                             int32_t offset,
                                             int32_t tc,
                                             int8_t part_P_nofilter,
                                             int8_t part_Q_nofilter)
{
  int32_t delta;
  int16_t m2 = src[-offset * 2];
  int16_t m3 = src[-offset];
  int16_t m4 = src[0];
  int16_t m5 = src[offset];

  delta = CLIP(-tc,tc, (((m4 - m3) << 2) + m2 - m5 + 4 ) >> 3);
  if(!part_P_nofilter) {
    src[-offset] = CLIP(0, (1 << encoder->bitdepth) - 1, m3 + delta);
  }
  if(!part_Q_nofilter) {
    src[0] = CLIP(0, (1 << encoder->bitdepth) - 1, m4 - delta);
  }
}


/**
 * \brief Check whether an edge is a TU boundary.
 *
 * \param state   encoder state
 * \param x       x-coordinate of the scu in pixels
 * \param y       y-coordinate of the scu in pixels
 * \param dir     direction of the edge to check
 * \return        true, if the edge is a TU boundary, otherwise false
 */
static bool is_tu_boundary(const encoder_state_t *const state,
                           int32_t x,
                           int32_t y,
                           edge_dir dir)
{
  const cu_info_t *const scu = kvz_videoframe_get_cu(state->tile->frame,
                                                     x >> MIN_SIZE,
                                                     y >> MIN_SIZE);
  const int tu_width = LCU_WIDTH >> scu->tr_depth;

  if (dir == EDGE_HOR) {
    return (y & (tu_width - 1)) == 0;
  } else {
    return (x & (tu_width - 1)) == 0;
  }
}


/**
 * \brief Check whether an edge is a PU boundary.
 *
 * \param state   encoder state
 * \param x       x-coordinate of the scu in pixels
 * \param y       y-coordinate of the scu in pixels
 * \param dir     direction of the edge to check
 * \return        true, if the edge is a TU boundary, otherwise false
 */
static bool is_pu_boundary(const encoder_state_t *const state,
                           int32_t x,
                           int32_t y,
                           edge_dir dir)
{
  const cu_info_t *const scu = kvz_videoframe_get_cu(state->tile->frame,
                                                     x >> MIN_SIZE,
                                                     y >> MIN_SIZE);
  // Get the containing CU.
  const int32_t cu_width = LCU_WIDTH >> scu->depth;
  const int32_t x_cu = x & ~(cu_width - 1);
  const int32_t y_cu = y & ~(cu_width - 1);
  const cu_info_t *const cu = kvz_videoframe_get_cu(state->tile->frame,
                                                    x_cu >> MIN_SIZE,
                                                    y_cu >> MIN_SIZE);

  const int num_pu = kvz_part_mode_num_parts[cu->part_size];
  for (int i = 0; i < num_pu; i++) {
    if (dir == EDGE_HOR) {
      int y_pu = PU_GET_Y(cu->part_size, cu_width, y_cu, i);
      if (y_pu == y) return true;

    } else {
      int x_pu = PU_GET_X(cu->part_size, cu_width, x_cu, i);
      if (x_pu == x) return true;
    }
  }

  return false;
}


/**
 * \brief Check whether an edge is aligned on a 8x8 grid.
 *
 * \param x     x-coordinate of the edge
 * \param y     y-coordinate of the edge
 * \param dir   direction of the edge
 * \return      true, if the edge is aligned on a 8x8 grid, otherwise false
 */
static bool is_on_8x8_grid(int x, int y, edge_dir dir)
{
  if (dir == EDGE_HOR) {
    return (y & 7) == 0;
  } else {
    return (x & 7) == 0;
  }
}

/**
 * \brief Apply the deblocking filter to luma pixels on a single edge.
 *
 * The caller should check that the edge is a TU boundary or a PU boundary.
 *
 \verbatim

         .-- filter this edge if dir == EDGE_HOR
         v
     +--------+
     |o <-- pixel at (x, y)
     |        |
     |<-- filter this edge if dir == EDGE_VER
     |        |
     +--------+

 \endverbatim
 *
 * \param state     encoder state
 * \param x         x-coordinate in pixels (see above)
 * \param y         y-coordinate in pixels (see above)
 * \param length    length of the edge in pixels
 * \param dir       direction of the edge to filter
 * \param tu_boundary   whether the edge is a TU boundary
 */
static void filter_deblock_edge_luma(encoder_state_t * const state,
                                     int32_t x,
                                     int32_t y,
                                     int32_t length,
                                     edge_dir dir,
                                     bool tu_boundary)
{
  videoframe_t * const frame = state->tile->frame;
  const encoder_control_t * const encoder = state->encoder_control;
  
  cu_info_t *cu_q = kvz_videoframe_get_cu(frame, x >> MIN_SIZE, y >> MIN_SIZE);

  {
    int32_t stride = frame->rec->stride;
    int32_t beta_offset_div2 = encoder->beta_offset_div2;
    int32_t tc_offset_div2   = encoder->tc_offset_div2;
    // TODO: support 10+bits
    kvz_pixel *orig_src = &frame->rec->y[x + y*stride];
    kvz_pixel *src = orig_src;
    cu_info_t *cu_p = NULL;
    int16_t x_cu = x >> MIN_SIZE;
    int16_t y_cu = y >> MIN_SIZE;

    int8_t strength = 0;
    int32_t qp              = state->global->QP;
    int32_t bitdepth_scale  = 1 << (encoder->bitdepth - 8);
    int32_t b_index         = CLIP(0, 51, qp + (beta_offset_div2 << 1));
    int32_t beta            = kvz_g_beta_table_8x8[b_index] * bitdepth_scale;
    int32_t side_threshold  = (beta + (beta >>1 )) >> 3;
    int32_t tc_index;
    int32_t tc;
    int32_t thr_cut;

    uint32_t num_4px_parts  = length / 4;

    const int32_t offset = (dir == EDGE_HOR) ? stride :      1;
    const int32_t step   = (dir == EDGE_HOR) ?      1 : stride;

    // TODO: add CU based QP calculation

    // For each 4-pixel part in the edge
    for (uint32_t block_idx = 0; block_idx < num_4px_parts; ++block_idx) {
      int32_t dp0, dq0, dp3, dq3, d0, d3, dp, dq, d;

      {
        // CU in the side we are filtering, update every 8-pixels
        cu_p = kvz_videoframe_get_cu(frame, x_cu - (dir == EDGE_VER) + (dir == EDGE_HOR ? block_idx>>1 : 0), y_cu - (dir == EDGE_HOR) + (dir == EDGE_VER ? block_idx>>1 : 0));

        bool nonzero_coeffs = cbf_is_set(cu_q->cbf.y, cu_q->tr_depth)
                           || cbf_is_set(cu_p->cbf.y, cu_p->tr_depth);

        // Filter strength
        strength = 0;
        if (cu_q->type == CU_INTRA || cu_p->type == CU_INTRA) {
          strength = 2;
        } else if (tu_boundary && nonzero_coeffs) {
          // Non-zero residual/coeffs and transform boundary
          // Neither CU is intra so tr_depth <= MAX_DEPTH.
          strength = 1;       
        } else if (cu_p->inter.mv_dir != 3 && cu_q->inter.mv_dir != 3 && ((abs(cu_q->inter.mv[cu_q->inter.mv_dir - 1][0] - cu_p->inter.mv[cu_p->inter.mv_dir - 1][0]) >= 4) || (abs(cu_q->inter.mv[cu_q->inter.mv_dir - 1][1] - cu_p->inter.mv[cu_p->inter.mv_dir - 1][1]) >= 4))) {
          // Absolute motion vector diff between blocks >= 1 (Integer pixel)
          strength = 1;
        } else if (cu_p->inter.mv_dir != 3 && cu_q->inter.mv_dir != 3 && cu_q->inter.mv_ref[cu_q->inter.mv_dir - 1] != cu_p->inter.mv_ref[cu_p->inter.mv_dir - 1]) {
          strength = 1;
        }
        
        // B-slice related checks
        if(!strength && state->global->slicetype == KVZ_SLICE_B) {

          // Zero all undefined motion vectors for easier usage
          if(!(cu_q->inter.mv_dir & 1)) {
            cu_q->inter.mv[0][0] = 0;
            cu_q->inter.mv[0][1] = 0;
          }
          if(!(cu_q->inter.mv_dir & 2)) {
            cu_q->inter.mv[1][0] = 0;
            cu_q->inter.mv[1][1] = 0;
          }

          if(!(cu_p->inter.mv_dir & 1)) {
            cu_p->inter.mv[0][0] = 0;
            cu_p->inter.mv[0][1] = 0;
          }
          if(!(cu_p->inter.mv_dir & 2)) {
            cu_p->inter.mv[1][0] = 0;
            cu_p->inter.mv[1][1] = 0;
          }
          const int refP0 = (cu_p->inter.mv_dir & 1) ? cu_p->inter.mv_ref[0] : -1;
          const int refP1 = (cu_p->inter.mv_dir & 2) ? cu_p->inter.mv_ref[1] : -1;
          const int refQ0 = (cu_q->inter.mv_dir & 1) ? cu_q->inter.mv_ref[0] : -1;
          const int refQ1 = (cu_q->inter.mv_dir & 2) ? cu_q->inter.mv_ref[1] : -1;
          const int16_t* mvQ0 = cu_q->inter.mv[0];
          const int16_t* mvQ1 = cu_q->inter.mv[1];

          const int16_t* mvP0 = cu_p->inter.mv[0];
          const int16_t* mvP1 = cu_p->inter.mv[1];

          if(( refP0 == refQ0 &&  refP1 == refQ1 ) || ( refP0 == refQ1 && refP1==refQ0 ))
          {
            // Different L0 & L1
            if ( refP0 != refP1 ) {          
              if ( refP0 == refQ0 ) {
                strength  = ((abs(mvQ0[0] - mvP0[0]) >= 4) ||
                             (abs(mvQ0[1] - mvP0[1]) >= 4) ||
                             (abs(mvQ1[0] - mvP1[0]) >= 4) ||
                             (abs(mvQ1[1] - mvP1[1]) >= 4)) ? 1 : 0;
              } else {
                strength  = ((abs(mvQ1[0] - mvP0[0]) >= 4) ||
                             (abs(mvQ1[1] - mvP0[1]) >= 4) ||
                             (abs(mvQ0[0] - mvP1[0]) >= 4) ||
                             (abs(mvQ0[1] - mvP1[1]) >= 4)) ? 1 : 0;
              }
            // Same L0 & L1
            } else {  
              strength  = ((abs(mvQ0[0] - mvP0[0]) >= 4) ||
                           (abs(mvQ0[1] - mvP0[1]) >= 4) ||
                           (abs(mvQ1[0] - mvP1[0]) >= 4) ||
                           (abs(mvQ1[1] - mvP1[1]) >= 4)) &&
                          ((abs(mvQ1[0] - mvP0[0]) >= 4) ||
                           (abs(mvQ1[1] - mvP0[1]) >= 4) ||
                           (abs(mvQ0[0] - mvP1[0]) >= 4) ||
                           (abs(mvQ0[1] - mvP1[1]) >= 4)) ? 1 : 0;
            }
          } else {
            strength = 1;
          }
        }

        tc_index        = CLIP(0, 51 + 2, (int32_t)(qp + 2*(strength - 1) + (tc_offset_div2 << 1)));
        tc              = kvz_g_tc_table_8x8[tc_index] * bitdepth_scale;
        thr_cut         = tc * 10;
      }
      if(!strength) continue;
      // Check conditions for filtering
      // TODO: Get rid of these inline defines.
      #define calc_DP(s,o) abs( (int16_t)s[-o*3] - (int16_t)2*s[-o*2] + (int16_t)s[-o] )
      #define calc_DQ(s,o) abs( (int16_t)s[0]    - (int16_t)2*s[o]    + (int16_t)s[o*2] )

      dp0 = calc_DP((src+step*(block_idx*4+0)), offset);
      dq0 = calc_DQ((src+step*(block_idx*4+0)), offset);
      dp3 = calc_DP((src+step*(block_idx*4+3)), offset);
      dq3 = calc_DQ((src+step*(block_idx*4+3)), offset);
      d0 = dp0 + dq0;
      d3 = dp3 + dq3;
      dp = dp0 + dp3;
      dq = dq0 + dq3;
      d  =  d0 + d3;

      if (d < beta) {
        int8_t filter_P = (dp < side_threshold) ? 1 : 0;
        int8_t filter_Q = (dq < side_threshold) ? 1 : 0;

        // Strong filtering flag checking
        #define useStrongFiltering(o,d,s) ( ((abs(s[-o*4]-s[-o]) + abs(s[o*3]-s[0])) < (beta>>3)) && (d<(beta>>2)) && ( abs(s[-o]-s[0]) < ((tc*5+1)>>1)) )
        int8_t sw = useStrongFiltering(offset, 2*d0, (src+step*(block_idx*4+0))) &&
                    useStrongFiltering(offset, 2*d3, (src+step*(block_idx*4+3)));

        // Filter four rows/columns
        for (int i = 0; i < 4; i++) {
          kvz_filter_deblock_luma(encoder, src + step * (4*block_idx + i), offset, tc, sw, 0, 0, thr_cut, filter_P, filter_Q);
        }
      }
    }
  }
}

/**
 * \brief Apply the deblocking filter to chroma pixels on a single edge.
 *
 * The caller should check that the edge is a TU boundary or a PU boundary.
 *
 \verbatim

         .-- filter this edge if dir == EDGE_HOR
         v
     +--------+
     |o <-- pixel at (x, y)
     |        |
     |<-- filter this edge if dir == EDGE_VER
     |        |
     +--------+

 \endverbatim
 *
 * \param state         encoder state
 * \param x             x-coordinate in chroma pixels (see above)
 * \param y             y-coordinate in chroma pixels (see above)
 * \param length        length of the edge in chroma pixels
 * \param dir           direction of the edge to filter
 * \param tu_boundary   whether the edge is a TU boundary
 */
static void filter_deblock_edge_chroma(encoder_state_t * const state,
                                       int32_t x,
                                       int32_t y,
                                       int32_t length,
                                       edge_dir dir,
                                       bool tu_boundary)
{
  const encoder_control_t * const encoder = state->encoder_control;
  const videoframe_t * const frame = state->tile->frame;
  const cu_info_t *cu_q = kvz_videoframe_get_cu_const(frame, x >> (MIN_SIZE - 1), y >> (MIN_SIZE - 1));

  // For each subpart
  {
    int32_t stride = frame->rec->stride >> 1;
    int32_t tc_offset_div2 = encoder->tc_offset_div2;
    // TODO: support 10+bits
    kvz_pixel *src[] = {
      &frame->rec->u[x + y*stride],
      &frame->rec->v[x + y*stride],
    };
    const cu_info_t *cu_p = NULL;
    int16_t x_cu = x >> (MIN_SIZE-1);
    int16_t y_cu = y >> (MIN_SIZE-1);
    int8_t strength = 2;

    int32_t QP             = kvz_g_chroma_scale[state->global->QP];
    int32_t bitdepth_scale = 1 << (encoder->bitdepth-8);
    int32_t TC_index       = CLIP(0, 51+2, (int32_t)(QP + 2*(strength-1) + (tc_offset_div2 << 1)));
    int32_t Tc             = kvz_g_tc_table_8x8[TC_index]*bitdepth_scale;

    const uint32_t num_4px_parts = length / 4;

    const int32_t offset = (dir == EDGE_HOR) ? stride :      1;
    const int32_t step   = (dir == EDGE_HOR) ?      1 : stride;

    for (uint32_t blk_idx = 0; blk_idx < num_4px_parts; ++blk_idx)
    {
      cu_p = kvz_videoframe_get_cu_const(frame, x_cu - (dir == EDGE_VER) + (dir == EDGE_HOR ? blk_idx : 0), y_cu - (dir == EDGE_HOR) + (dir == EDGE_VER ? blk_idx : 0));

      // Only filter when strenght == 2 (one of the blocks is intra coded)
      if (cu_q->type == CU_INTRA || cu_p->type == CU_INTRA) {
        for (int component = 0; component < 2; component++) {
          for (int i = 0; i < 4; i++) {
            kvz_filter_deblock_chroma(encoder, src[component] + step * (4*blk_idx + i), offset, Tc, 0, 0);
          }
        }
      }
    }
  }
}


/**
 * \brief Filter edge of a single PU or TU
 *
 * \param state     encoder state
 * \param x         block x-position in pixels
 * \param y         block y-position in pixels
 * \param width     block width in pixels
 * \param height    block height in pixels
 * \param dir       direction of the edges to filter
 * \param tu_boundary   whether the edge is a TU boundary
 */
static void filter_deblock_unit(encoder_state_t * const state,
                                int x,
                                int y,
                                int width,
                                int height,
                                edge_dir dir,
                                bool tu_boundary)
{
  // no filtering on borders (where filter would use pixels outside the picture)
  if (x == 0 && dir == EDGE_VER) return;
  if (y == 0 && dir == EDGE_HOR) return;

  // Length of luma and chroma edges.
  int32_t length;
  int32_t length_c;

  if (dir == EDGE_HOR) {
    const videoframe_t * const frame = state->tile->frame;
    const int32_t x_right             = x + width;
    const bool rightmost_4px_of_lcu   = x_right % LCU_WIDTH == 0;
    const bool rightmost_4px_of_frame = x_right == frame->width;

    if (rightmost_4px_of_lcu && !rightmost_4px_of_frame) {
      // The last 4 pixels will be deblocked when processing the next LCU.
      length   = width - 4;
      length_c = (width >> 1) - 4;

    } else {
      length   = width;
      length_c = width >> 1;
    }
  } else {
    length   = height;
    length_c = height >> 1;
  }

  filter_deblock_edge_luma(state, x, y, length, dir, tu_boundary);

  // Chroma pixel coordinates.
  const int32_t x_c = x >> 1;
  const int32_t y_c = y >> 1;
  if (is_on_8x8_grid(x_c, y_c, dir)) {
    filter_deblock_edge_chroma(state, x_c, y_c, length_c, dir, tu_boundary);
  }
}


/**
 * \brief Deblock PU and TU boundaries inside an LCU.
 *
 * \param state     encoder state
 * \param x_px      block x-position in pixels
 * \param y_px      block y-position in pixels
 * \param dir       direction of the edges to filter
 *
 * Recursively traverse the CU/TU quadtree. At the lowest level, apply the
 * deblocking filter to the left edge (when dir == EDGE_VER) or the top edge
 * (when dir == EDGE_HOR) as needed. Both luma and chroma are filtered.
 */
static void filter_deblock_lcu_inside(encoder_state_t * const state,
                                      int32_t x,
                                      int32_t y,
                                      edge_dir dir)
{
  const int end_x = MIN(x + LCU_WIDTH, state->tile->frame->width);
  const int end_y = MIN(y + LCU_WIDTH, state->tile->frame->height);

  for (int edge_y = y; edge_y < end_y; edge_y += 8) {
    for (int edge_x = x; edge_x < end_x; edge_x += 8) {
      bool tu_boundary = is_tu_boundary(state, edge_x, edge_y, dir);
      if (tu_boundary || is_pu_boundary(state, edge_x, edge_y, dir)) {
        filter_deblock_unit(state, edge_x, edge_y, 8, 8, dir, tu_boundary);
      }
    }
  }
}


/**
 * \brief Filter rightmost 4 pixels of the horizontal egdes of an LCU.
 *
 * \param state     encoder state
 * \param x_px      x-coordinate of the *right* edge of the LCU in pixels
 * \param y_px      y-coordinate of the top edge of the LCU in pixels
 */
static void filter_deblock_lcu_rightmost(encoder_state_t * const state,
                                         int32_t x_px,
                                         int32_t y_px)
{
  // Luma
  const int x = x_px - 4;
  const int end = MIN(y_px + LCU_WIDTH, state->tile->frame->height);
  for (int y = y_px; y < end; y += 8) {
    // The top edge of the whole frame is not filtered.
    bool tu_boundary = is_tu_boundary(state, x, y, EDGE_HOR);
    bool pu_boundary = is_pu_boundary(state, x, y, EDGE_HOR);
    if (y > 0 && (tu_boundary || pu_boundary)) {
      filter_deblock_edge_luma(state, x, y, 4, EDGE_HOR, tu_boundary);
    }
  }

  // Chroma
  const int x_px_c = x_px >> 1;
  const int y_px_c = y_px >> 1;
  const int x_c = x_px_c - 4;
  const int end_c = MIN(y_px_c + LCU_WIDTH_C, state->tile->frame->height >> 1);
  for (int y_c = y_px_c; y_c < end_c; y_c += 8) {
    // The top edge of the whole frame is not filtered.
    bool tu_boundary = is_tu_boundary(state, x_c << 1, y_c << 1, EDGE_HOR);
    bool pu_boundary = is_pu_boundary(state, x_c << 1, y_c << 1, EDGE_HOR);
    if (y_c > 0 && (tu_boundary || pu_boundary)) {
      filter_deblock_edge_chroma(state, x_c, y_c, 4, EDGE_HOR, tu_boundary);
    }
  }
}


/**
 * \brief Deblock a single LCU without using data from right or down.
 *
 * Filter the following vertical edges (horizontal filtering):
 *  1. The left edge of the LCU.
 *  2. All vertical edges within the LCU.
 *
 * Filter the following horizontal edges (vertical filtering):
 *  1. The rightmost 4 pixels of the top edge of the LCU to the left.
 *  2. The rightmost 4 pixels of all horizontal edges within the LCU to the
 *     left.
 *  3. The top edge and all horizontal edges within the LCU, excluding the
 *     rightmost 4 pixels. If the LCU is the rightmost LCU of the frame, the
 *     last 4 pixels are also filtered.
 *
 * What is not filtered:
 *  - The rightmost 4 pixels of the top edge and all horizontal edges within
 *    the LCU, unless the LCU is the rightmost LCU of the frame.
 *  - The bottom edge of the LCU.
 *  - The right edge of the LCU.
 *
 * \param state   encoder state
 * \param x_px    x-coordinate of the left edge of the LCU in pixels
 * \param y_px    y-coordinate of the top edge of the LCU in pixels
 */
void kvz_filter_deblock_lcu(encoder_state_t * const state, int x_px, int y_px)
{
  filter_deblock_lcu_inside(state, x_px, y_px, EDGE_VER);
  if (x_px > 0) {
    filter_deblock_lcu_rightmost(state, x_px, y_px);
  }
  filter_deblock_lcu_inside(state, x_px, y_px, EDGE_HOR);
}
