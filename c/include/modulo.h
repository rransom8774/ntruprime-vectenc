#ifndef X9pwka31j5bv0zhrfx0cts2vgp4ttqgbotn6to4jux405p4g2modpftix6zusnbyp
#define X9pwka31j5bv0zhrfx0cts2vgp4ttqgbotn6to4jux405p4g2modpftix6zusnbyp

/*
 * Authors: Robert Ransom
 *
 * This software is released to the public domain.
 *
 * To the extent permitted by law, this software is provided WITHOUT ANY
 * WARRANTY WHATSOEVER.
 */

#include <stddef.h>
#include <stdint.h>

/* Division and modulo.  Uses the same general approach as current
   libdivide, but with a simpler algorithm.  Maximum divisor is
   2**14 = 16384; maximum dividend is 2**30 - 1. */

/* The approach of https://arxiv.org/abs/1902.01961 would require a
   precision of 30+14 = 44 bits.  Multiplying by a 32-bit reciprocal
   is likely to be faster, and many of the modulo operations in this
   software will be in the vector coder where half of the modulo calls
   also need the quotient. */

struct modulus {
  uint32_t recip;
  uint16_t orig;
  uint8_t shift;
};

/* maximum value 16384 */
/* returns 0 on success, <0 on failure (invalid input only) */
int pqcr_modulus_init(struct modulus *mod, uint32_t value);

/* maximum value 2**30 - 1; no error checking */
static inline uint32_t pqcr_modulus_divide(const struct modulus *mod, uint32_t value) {
  return (uint32_t)( (((uint64_t)value) * (uint64_t)(mod->recip)) >> (31 + mod->shift) );
};

/* maximum value 2**30 - 1; no error checking */
static inline uint32_t pqcr_modulus_divmod(const struct modulus *mod, uint32_t *rem_out, uint32_t value) {
  uint32_t q = pqcr_modulus_divide(mod, value);
  *rem_out = value - (q * (uint32_t)mod->orig);
  return q;
};

/* maximum value 2**30 - 1; no error checking */
static inline uint32_t pqcr_modulus_modulo(const struct modulus *mod, uint32_t value) {
  uint32_t r;
  (void)pqcr_modulus_divmod(mod, &r, value);
  return r;
};

static inline uint32_t pqcr_modulus_mult(const struct modulus *mod, uint32_t value) {
  return value * (uint32_t)mod->orig;
};

#endif
