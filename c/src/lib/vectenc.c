
/*
 * Authors: Robert Ransom
 *
 * This software is released to the public domain.
 *
 * To the extent permitted by law, this software is provided WITHOUT ANY
 * WARRANTY WHATSOEVER.
 */

#include "vectenc.h"
#include "modulo.h"

#include <assert.h>
#include <stdlib.h>

#define VECTOR_ENCODE_LIMIT 16384
/* #define VECTOR_ENCODE_OUTMOD 256 */

typedef struct modulus modulus;

struct vclayer_bytes {
  size_t total;
  const uint8_t *outcounts;
};

struct vclayer_merge {
  const modulus *moduli;
  int has_odd_element;
};

struct vcstep {
  size_t nelts_lower;
  const struct vclayer_bytes *bytes;
  const struct vclayer_merge *merge;
};

struct vectcoder {
  size_t nsteps;
  const struct vcstep *steps;
  vectelt root_bound;
  uint8_t root_bytes;
};

#define OUTMOD 256
static vectelt ceildiv_by_outmod(vectelt m) {
  return (m + (OUTMOD - 1)) / OUTMOD;
};
#undef OUTMOD

static int compute_layer_bytes(const struct vclayer_bytes **ppbytes, vectelt *M, size_t *pnelts) {
  size_t i;
  struct vclayer_bytes *bytes;
  size_t total;
  uint8_t *outcounts;

  *ppbytes = bytes = calloc(1, sizeof(struct vclayer_bytes));
  if (bytes == NULL) return -1;

  bytes->outcounts = outcounts = calloc(*pnelts, sizeof(uint8_t));
  if (outcounts == NULL) return -1;

  total = 0;
  for (i = 0; i < *pnelts; ++i) {
    uint8_t outcount = 0;
    vectelt m = M[i];
    while (m >= VECTOR_ENCODE_LIMIT) {
      m = ceildiv_by_outmod(m);
      ++outcount; ++total;
    };
    outcounts[i] = outcount;
    M[i] = m;
  };
  bytes->total = total;

  if (total == 0) {
    *ppbytes = NULL;
    free(outcounts);
    free(bytes);
  };

  return 0;
};

static int compute_layer_merge(const struct vclayer_merge **ppmerge, vectelt *M, size_t *pnelts) {
  size_t i, j;
  struct vclayer_merge *merge;
  int has_odd_element;
  modulus *moduli;

  *ppmerge = merge = calloc(1, sizeof(struct vclayer_merge));
  if (merge == NULL) return -1;

  merge->moduli = moduli = calloc(*pnelts, sizeof(modulus));
  if (moduli == NULL) return -1;

  merge->has_odd_element = has_odd_element = (*pnelts) & 1;

  for (i = 0, j = 0; i < (*pnelts)-1; i += 2, j += 1) {
    pqcr_modulus_init(&(moduli[i]), M[i]);
    pqcr_modulus_init(&(moduli[i+1]), M[i+1]);
    M[j] = M[i] * M[i+1];
  };

  if (has_odd_element) {
    pqcr_modulus_init(&(moduli[i]), M[i]);
    M[j] = M[i];
    ++j;
  };

  (*pnelts) = j;

  return 0;
};

struct vectcoder *pqcr_vectcoder_new(vectelt *Mbuf, size_t nelts) {
  struct vectcoder *rv = calloc(1, sizeof(struct vectcoder));
  struct vcstep *steps;
  size_t nsteps;
  vectelt root_bound;
  uint8_t root_bytes;
  size_t i;
  int status;

  if (nelts < 1) goto err;

  if (rv == NULL) goto err;

  nsteps = 0;
  i = nelts;
  while (i > 1) {
    i = (i >> 1) + (i & 1);
    ++nsteps;
  };
  rv->steps = steps = calloc(nsteps, sizeof(struct vcstep));
  rv->nsteps = nsteps;

  status = 0;
  i = 0;
  while (i < nsteps) {
    steps[i].nelts_lower = nelts;
    status |= compute_layer_bytes(&(steps[i].bytes), Mbuf, &nelts);
    status |= compute_layer_merge(&(steps[i].merge), Mbuf, &nelts);
    ++i;
  };

  assert(nelts == 1);

  rv->root_bound = root_bound = Mbuf[0];

  root_bytes = 0;
  while (root_bound > 1) {
    root_bound = ceildiv_by_outmod(root_bound);
    ++root_bytes;
  };
  rv->root_bytes = root_bytes;

  if (status != 0) goto err;

  return rv;

 err:
  if (rv != NULL) {
    pqcr_vectcoder_free(rv);
  };

  return NULL;
};

struct vectcoder *pqcr_vectcoder_new_uniform_bound(vectelt M, size_t nelts) {
  vectelt *Mbuf = calloc(nelts, sizeof(vectelt));
  struct vectcoder *rv;
  size_t i;

  if (Mbuf == NULL) return NULL;

  for (i = 0; i < nelts; ++i) {
    Mbuf[i] = M;
  };

  rv = pqcr_vectcoder_new(Mbuf, nelts);

  free(Mbuf);

  return rv;
};

void pqcr_vectcoder_free(struct vectcoder *vc) {
  size_t i;
  if (vc != NULL) {
    if (vc->steps != NULL) {
      for (i = 0; i < vc->nsteps; ++i) {
	if (vc->steps[i].merge != NULL) {
	  if (vc->steps[i].merge->moduli != NULL) {
	    free((modulus *)(vc->steps[i].merge->moduli));
	  };
	  free((void *)(vc->steps[i].merge));
	};

	if (vc->steps[i].bytes != NULL) {
	  if (vc->steps[i].bytes->outcounts != NULL) {
	    free((void *)(vc->steps[i].bytes->outcounts));
	  };
	  free((void *)(vc->steps[i].bytes));
	};
      };

      free((void *)(vc->steps));
    };

    free(vc);
  };
};

size_t pqcr_vectcoder_get_nelts(const struct vectcoder *vc) {
  if ((vc->nsteps != 0) && (vc->steps != NULL)) {
    return vc->steps[0].nelts_lower;
  };
  return 1;
};

size_t pqcr_vectcoder_get_nbytes_separate_root(const struct vectcoder *vc) {
  size_t nbytes = 0;
  size_t i;

  for (i = 0; i < vc->nsteps; ++i) {
    if (vc->steps[i].bytes != NULL) {
      nbytes += vc->steps[i].bytes->total;
    };
  };

  return nbytes;
};

vectelt pqcr_vectcoder_get_root_bound(const struct vectcoder *vc) {
  return vc->root_bound;
};

size_t pqcr_vectcoder_get_nbytes(const struct vectcoder *vc) {
  return pqcr_vectcoder_get_nbytes_separate_root(vc) + vc->root_bytes;
};


/* ENCODER */

/* Encoding moves the S pointer through the buffer from beginning to
   end.  The R input buffer is used as scratch space to avoid memory
   allocation and all the error-handling gunk that would require. */

static uint8_t *encode_apply_bytes(const struct vcstep *step, uint8_t *S_, vectelt *R) {
  const struct vclayer_bytes *bytes = step->bytes;
  uint8_t *S = S_;
  size_t i;

  if (bytes == NULL) return S;

  for (i = 0; i < step->nelts_lower; ++i) {
    size_t oc = bytes->outcounts[i];
    vectelt r = R[i];

    while (oc > 0) {
      *S = r & 255;
      r = r >> 8;
      ++S;

      --oc;
    };

    R[i] = r;
  };

  return S;
};

static uint8_t *encode_apply_merge(const struct vcstep *step, uint8_t *S, vectelt *R) {
  const struct vclayer_merge *merge = step->merge;
  size_t i, j;

  if (merge == NULL) return S;

  for (i = 0, j = 0; i < step->nelts_lower - 1; i += 2, j += 1) {
    vectelt r = R[i] + pqcr_modulus_mult(&(merge->moduli[i]), R[i+1]);
    R[j] = r;
  };

  if (merge->has_odd_element) {
    R[j] = R[i];
  };

  return S;
};

static uint8_t *encode_separate_root_internal(const struct vectcoder *vc, uint8_t *S_, vectelt *root, vectelt *R) {
  uint8_t *S = S_;
  size_t i;

  for (i = 0; i < vc->nsteps; ++i) {
    S = encode_apply_bytes(&(vc->steps[i]), S, R);
    S = encode_apply_merge(&(vc->steps[i]), S, R);
  };

  *root = R[0];

  return S;
};

void pqcr_vectcoder_encode_separate_root(const struct vectcoder *vc, uint8_t *S, vectelt *root, vectelt *Rbuf) {
  return (void)encode_separate_root_internal(vc, S, root, Rbuf);
};

void pqcr_vectcoder_encode(const struct vectcoder *vc, uint8_t *S_, vectelt *Rbuf) {
  vectelt root = 0;
  uint8_t *S = encode_separate_root_internal(vc, S_, &root, Rbuf);
  size_t i, root_bytes;

  root_bytes = vc->root_bytes;
  for (i = 0; i < root_bytes; ++i) {
    *S = root & 255;
    ++S;
    root = root >> 8;
  };
};


/* DECODER */

/* Decoding starts from the end of the buffer, and operates
   right-to-left within each layer to fit in a single buffer. */

static const uint8_t *decode_apply_bytes(const struct vcstep *step, const uint8_t *S_, vectelt *R) {
  const struct vclayer_bytes *bytes = step->bytes;
  const uint8_t *S = S_;
  size_t i;

  if (bytes == NULL) return S;

  i = step->nelts_lower;
  do {
    size_t oc;
    vectelt r;

    --i;

    oc = bytes->outcounts[i];
    r = R[i];

    while (oc > 0) {
      --S;
      r = (r << 8) | (((vectelt)*S) & 255);

      --oc;
    };

    R[i] = r;
  } while (i != 0);

  return S;
};

static const uint8_t *decode_apply_merge(const struct vcstep *step, const uint8_t *S, vectelt *R) {
  const struct vclayer_merge *merge = step->merge;
  size_t i, j;

  if (merge == NULL) return S;

  if (merge->has_odd_element) {
    R[step->nelts_lower - 1] = R[(step->nelts_lower - 1) >> 1];
  };

  i = (step->nelts_lower) & (~1);
  j = i >> 1;
  do {
    vectelt r2;
    vectelt r2quot, r2rem=0;

    i -= 2; j -= 1;

    r2 = R[j];
    r2quot = pqcr_modulus_divmod(&(merge->moduli[i]), &r2rem, r2);
    R[i] = r2rem;
    R[i+1] = pqcr_modulus_modulo(&(merge->moduli[i+1]), r2quot);
  } while (i != 0);

  return S;
};

#define IVDUMP(layertype, R, Rlen)
/*
#include <stdio.h>
static void dumpvec(const char *name, uint32_t *v, size_t len) {
  int at_eol = 0;
  size_t i = 0;
  printf("%s (%zu elts):\n", name, len);
  while (i < len) {
    at_eol = 0;
    printf("\t%d", (int)(v[i]));
    ++i;
    if (i % 8 == 0) {
      printf("\n");
      at_eol = 1;
    };
  };
  if (!at_eol) printf("\n");
};
#define IVDUMP(layertype, R, Rlen) dumpvec(#layertype, R, Rlen)
*/

static void decode_separate_root_internal(const struct vectcoder *vc, const uint8_t *S_, vectelt *R) {
  const uint8_t *S = S_;
  size_t i;

  i = vc->nsteps;
  while (i != 0) {
    --i;

    S = decode_apply_merge(&(vc->steps[i]), S, R);
    IVDUMP(merge, R, vc->steps[i].nelts_lower);
    S = decode_apply_bytes(&(vc->steps[i]), S, R);
    IVDUMP(bytes, R, vc->steps[i].nelts_lower);
  };
};

void pqcr_vectcoder_decode_separate_root(const struct vectcoder *vc, vectelt *R, const uint8_t *S, vectelt root) {
  size_t nbytes_separate_root = pqcr_vectcoder_get_nbytes_separate_root(vc);

  R[0] = root;

  return decode_separate_root_internal(vc, S+nbytes_separate_root, R);
};

void pqcr_vectcoder_decode(const struct vectcoder *vc, vectelt *R, const uint8_t *S) {
  size_t nbytes = pqcr_vectcoder_get_nbytes(vc);
  const uint8_t *S_ = S + nbytes;
  size_t i;
  vectelt root;

  root = 0;

  i = vc->root_bytes;
  while (i != 0) {
    --i;

    --S_;
    root = (root << 8) | (((vectelt)*S_) & 255);
  };

  R[0] = root;

  return decode_separate_root_internal(vc, S_, R);
};

