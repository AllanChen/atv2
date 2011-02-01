#ifndef BITREADER_H_INCLUDED
#define BITREADER_H_INCLUDED

/*
 * Copyright (C) 2000, 2001, 2002 H�kan Hjort <d95hjort@dtek.chalmers.se>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t *start;
  uint32_t byte_position;
  uint32_t bit_position;
  uint8_t byte;
} getbits_state_t;

int dvdread_getbits_init(getbits_state_t *state, uint8_t *start);
uint32_t dvdread_getbits(getbits_state_t *state, uint32_t number_of_bits);

#ifdef __cplusplus
};
#endif
#if defined(__APPLE__) && defined(__arm__)
#include "../../../../../DllLoader/exports/ios_wrap.h"
#endif

#endif /* BITREADER_H_INCLUDED */
