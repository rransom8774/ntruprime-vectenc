
/*
 * Authors: Robert Ransom
 *
 * This software is released to the public domain.
 *
 * To the extent permitted by law, this software is provided WITHOUT ANY
 * WARRANTY WHATSOEVER.
 */

#include "modulo.h"

#include <assert.h>

#define MODULUS_MAX 16384

static inline unsigned int bit_length(uint32_t x) {
  unsigned int rv = 0;
  while (x != 0) {
    ++rv;
    x = (x >> 1);
  };
  return rv;
};

static inline uint64_t ceildiv(uint64_t x, uint32_t div) {
  uint64_t y = x + (uint64_t)(div - 1);
  return y / (uint64_t)div;
};

int pqcr_modulus_init(struct modulus *mod, uint32_t value) {
  if ((value == 0) || (value > MODULUS_MAX)) {
    return -1;
  };

  uint64_t recip;
  uint8_t shift;

  mod->orig = value;
  mod->shift = shift = bit_length(value);
  mod->recip = recip = ceildiv((((uint64_t)1) << (31 + shift)), value);

  if ((recip & 1) == 0) {
    mod->recip = recip = (recip >> 1);
    mod->shift = shift = (shift - 1);
  };

  assert(recip == mod->recip);

  return 0;
};

