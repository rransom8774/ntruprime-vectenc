
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NO_GETOPT
#include <unistd.h>
#endif

#include "modulo.h"
#include "vectenc.h"

#include "XKCP/SimpleFIPS202.h"

struct test_vector {
  const uint8_t *seed;
  size_t seedlen;
  vectelt *R, *decR;
  uint8_t *S;
  const struct test_vector_set *tvs;
};

struct test_vector_set {
  char name[64];
  vectelt *M;
  size_t Mlen;
  struct vectcoder *vc;
  size_t Slen;
};

static struct test_vector_set *test_vector_sets = NULL;
static size_t test_vector_sets_count = 0;
static size_t test_vector_sets_capacity = 0;

static vectelt *Mbuf = NULL;
static size_t Mbuf_capacity = 0;

static struct test_vector_set *tvs_alloc() {
  size_t i = test_vector_sets_count++;
  struct test_vector_set *rv;

  if (i >= test_vector_sets_capacity) {
    size_t new_capacity = test_vector_sets_capacity + 8;
    struct test_vector_set *new_tvs_array =
      realloc(test_vector_sets,
              sizeof(struct test_vector_set) * new_capacity);
    if (new_tvs_array == NULL) {
      return NULL;
    };
    test_vector_sets = new_tvs_array;
    test_vector_sets_capacity = new_capacity;
  };

  rv = &(test_vector_sets[i]);
  memset(rv, 0, sizeof(*rv));
  return rv;
};

static struct test_vector_set *tvs_lookup(const char *name) {
  size_t i;

  for (i = 0; i < test_vector_sets_count; ++i) {
    if (strcmp(test_vector_sets[i].name, name) == 0) {
      return &(test_vector_sets[i]);
    };
  };

  return NULL;
};

static int tvs_init_vc_Slen(struct test_vector_set *tvs) {
  /* NUL-terminate tvs->name in case snprintf didn't */
  tvs->name[sizeof(tvs->name) - 1] = '\0';

  if (tvs->Mlen > Mbuf_capacity) {
    size_t new_capacity = tvs->Mlen;
    vectelt *new_Mbuf =
      realloc(Mbuf, sizeof(vectelt) * new_capacity);
    if (new_Mbuf == NULL) {
      return -1;
    };
    Mbuf = new_Mbuf;
    Mbuf_capacity = new_capacity;
  };

  memcpy(Mbuf, tvs->M, sizeof(vectelt) * tvs->Mlen);
  tvs->vc = pqcr_vectcoder_new(Mbuf, tvs->Mlen);
  if (tvs->vc == NULL) return -1;

  tvs->Slen = pqcr_vectcoder_get_nbytes(tvs->vc);

  return 0;
};

static void tv_init(struct test_vector *tv) {
  tv->seed = NULL;
  tv->seedlen = 0;
  tv->R = NULL;
  tv->decR = NULL;
  tv->S = NULL;
  tv->tvs = NULL;
};

static uint32_t u32le_get(const uint8_t *p) {
  return (( ((uint32_t)p[0])       ) +
	        ((((uint32_t)p[1]) <<  8)) +
	        ((((uint32_t)p[2]) << 16)) +
	        ((((uint32_t)p[3]) << 24)));
};

static int tv_generate_R(struct test_vector *tv) {
  size_t Rlen = tv->tvs->Mlen;
  uint8_t *buf = malloc(4 * Rlen);
  int rv = -1;
  size_t i;

  if (buf == NULL) goto end;

  if (tv->R == NULL) {
    tv->R = malloc(sizeof(vectelt) * Rlen);
    if (tv->R == NULL) goto end;
  };

  SHAKE256(buf, 4*Rlen, tv->seed, tv->seedlen);

  for (i = 0; i < Rlen; ++i) {
    tv->R[i] = u32le_get(buf + (4*i)) % tv->tvs->M[i];
  };

  rv = 0;

 end:
  if (buf != NULL) free(buf);
  return rv;
};

static int tv_generate_testvec(struct test_vector *tv) {
  if (tv_generate_R(tv) < 0) return -1;

  if (tv->S == NULL) {
    tv->S = malloc(tv->tvs->Slen);
    if (tv->S == NULL) return -1;
  };

  /* clobbers R */
  pqcr_vectcoder_encode(tv->tvs->vc, tv->S, tv->R);

  /* now regenerate R, from the (unchanged) seed */
  if (tv_generate_R(tv) < 0) return -1;

  /* now test the decoder */

  if (tv->decR == NULL) {
    tv->decR = malloc(sizeof(vectelt) * tv->tvs->Mlen);
    if (tv->decR == NULL) return -1;
  };

  pqcr_vectcoder_decode(tv->tvs->vc, tv->decR, tv->S);

  if (memcmp(tv->R, tv->decR, sizeof(vectelt) * tv->tvs->Mlen) != 0) {
    fprintf(stderr, "decR != R: %s, %08x\n",
	    tv->tvs->name, u32le_get(tv->seed));
    return 1;
  };

  return 0;
};

static int gen_tvs_const(vectelt m, size_t n) {
  struct test_vector_set *tvs = tvs_alloc();
  size_t i;

  snprintf(tvs->name, sizeof(tvs->name), "const_%d_%d", (int)m, (int)n);

  tvs->M = malloc(sizeof(vectelt) * n);
  if (tvs->M == NULL) return -1;
  for (i = 0; i < n; ++i) {
    tvs->M[i] = m;
  };
  tvs->Mlen = n;

  return tvs_init_vc_Slen(tvs);
};

static unsigned int ntruprime_rounded_m(unsigned int q) {
  unsigned int qm3 = q % 3;
  if (qm3 == 1) {
    return (q - 1) / 2;
  } else if (qm3 == 2) {
    return (q + 1) / 2;
  } else {
    /* q is invalid */
    abort();
  };
};

static int gen_tvs_squished_perm(size_t n) {
  struct test_vector_set *tvs = tvs_alloc();
  size_t i;

  snprintf(tvs->name, sizeof(tvs->name), "squished_perm_%d", (int)n);

  tvs->M = malloc(sizeof(vectelt) * (n-1));
  if (tvs->M == NULL) return -1;
  for (i = 0; i < n-1; ++i) {
    tvs->M[i] = n - i;
  };
  tvs->Mlen = n-1;

  return tvs_init_vc_Slen(tvs);
};

static int gen_tvs_random(size_t n, vectelt ilb, vectelt iub, const char *Mseed) {
  struct test_vector_set *tvs = tvs_alloc();
  size_t Mseedlen = strlen(Mseed);
  size_t i;
  vectelt max = iub - (ilb-1);
  struct test_vector tv_tmp;

  snprintf(tvs->name, sizeof(tvs->name), "random_%d_%d_%d_%s",
           (int)n, (int)ilb, (int)iub, Mseed);

  tvs->M = malloc(sizeof(vectelt) * n);
  if (tvs->M == NULL) return -1;
  for (i = 0; i < n; ++i) {
    /* temporary value; will be replaced with output of generate_R */
    tvs->M[i] = max;
  };
  tvs->Mlen = n;

  tv_init(&tv_tmp);
  tv_tmp.tvs = tvs;
  tv_tmp.seed = Mseed;
  tv_tmp.seedlen = Mseedlen;
  if (tv_generate_R(&tv_tmp) < 0) return -1;
  for (i = 0; i < n; ++i) {
    tvs->M[i] = tv_tmp.R[i] + ilb;
  };
  free(tv_tmp.R);

  return tvs_init_vc_Slen(tvs);
};

static int setup_testvecsets() {
  int rv = 0;

  /* Streamlined NTRU Prime vector formats, unrounded */

  rv |= gen_tvs_const(4591, 761);
  rv |= gen_tvs_const(4621, 653);
  rv |= gen_tvs_const(5167, 857);

  /* Streamlined NTRU Prime vector formats, rounded */

  rv |= gen_tvs_const(ntruprime_rounded_m(4591), 761);
  rv |= gen_tvs_const(ntruprime_rounded_m(4621), 653);
  rv |= gen_tvs_const(ntruprime_rounded_m(5167), 857);

  /* pkpsig z vector formats */

  rv |= gen_tvs_const(797, 55);
  rv |= gen_tvs_const(977, 61);
  rv |= gen_tvs_const(1409, 87);
  rv |= gen_tvs_const(1789, 111);

  /* pkpsig rho vector formats, unsquished */

  rv |= gen_tvs_const(55, 55);
  rv |= gen_tvs_const(61, 61);
  rv |= gen_tvs_const(87, 87);
  rv |= gen_tvs_const(111, 111);

  /* pkpsig rho vector formats, squished */

  rv |= gen_tvs_squished_perm(55);
  rv |= gen_tvs_squished_perm(61);
  rv |= gen_tvs_squished_perm(87);
  rv |= gen_tvs_squished_perm(111);

  /* miscellaneous random vector formats with no particular application in mind */

  rv |= gen_tvs_random(128, 2, 15, "foobar");
  rv |= gen_tvs_random(768, 2048, 2048+256, "foo");

  if (rv != 0) rv = -1;
  return rv;
};

static void output_vectelts(FILE *f, const vectelt *V, size_t Vlen) {
  size_t i;

  if (Vlen == 0) return;

  i = 0;
  while (1) {
    fprintf(f, "%d", (int)V[i]);
    ++i;
    if (i < Vlen) {
      fprintf(f, ", ");
    } else {
      fprintf(f, "\n");
      return;
    };
  };
};

static void output_hexdump(FILE *f, const uint8_t *S, size_t Slen) {
  size_t i;

  for (i = 0; i < Slen; ++i) {
    fprintf(f, "%02x", (int)S[i]);
  };
  fprintf(f, "\n");
};

static void output_testvec_text(FILE *f, struct test_vector *tv) {
  fprintf(f, "Seed = ");
  output_hexdump(f, tv->seed, tv->seedlen);

  fprintf(f, "   R = ");
  output_vectelts(f, tv->R, tv->tvs->Mlen);

  fprintf(f, "   M = ");
  output_vectelts(f, tv->tvs->M, tv->tvs->Mlen);

  fprintf(f, "   S = ");
  output_hexdump(f, tv->S, tv->tvs->Slen);

  fprintf(f, "decR = ");
  output_vectelts(f, tv->decR, tv->tvs->Mlen);

  fprintf(f, "\n");
};

static void output_testvec_vectelt_bin(FILE *f, struct test_vector *tv, vectelt *V) {
  size_t Vlen = tv->tvs->Mlen;
  size_t i;
  uint8_t *buf = malloc(2 * Vlen);

  if (buf == NULL) {
    fprintf(stderr, "generate-test-vectors: malloc failed in output_testvec_vectelt_bin\n");
    exit(1);
  };

  for (i = 0; i < Vlen; ++i) {
    buf[2*i + 0] =  V[i]       & 255;
    buf[2*i + 1] = (V[i] >> 8) & 255;
    if (V[i] & (~(vectelt)32767)) {
      buf[2*i + 1] |= 128;
    };
  };

  (void)fwrite(buf, 2*Vlen, 1, f);

  free(buf);
};

static void output_testvec_S_bin(FILE *f, struct test_vector *tv) {
  (void)fwrite(tv->S, tv->tvs->Slen, 1, f);
};

static inline void pack_ui32(uint8_t *buf, uint32_t x) {
  buf[0] =  x        & 255;
  buf[1] = (x >>  8) & 255;
  buf[2] = (x >> 16) & 255;
  buf[3] = (x >> 24) & 255;
};

#define FILES_TEXT 1
#define FILES_R_BIN 2
#define FILES_S_BIN 4
#define FILES_DECR_BIN 8
#define FILES_ALL 15

static int generate_testvecset_files(struct test_vector_set *tvs, uint32_t count, unsigned int files) {
  char fname_buf_text[128];
  char fname_buf_R_bin[128];
  char fname_buf_S_bin[128];
  char fname_buf_decR_bin[128];
  FILE *f_text = NULL;
  FILE *f_R_bin = NULL;
  FILE *f_S_bin = NULL;
  FILE *f_decR_bin = NULL;
  uint32_t i;
  uint8_t seedbuf[4];
  struct test_vector tv;
  int rv = -1;
  int gtv_rv = 0;

  tv_init(&tv);
  tv.seed = seedbuf;
  tv.seedlen = 4;
  tv.tvs = tvs;

  if (files & FILES_TEXT) {
    snprintf(fname_buf_text, 128, "TVSet_%d_%s.txt", count, tvs->name);
    fname_buf_text[127] = '\0';
    f_text = fopen(fname_buf_text, "w");
    if (f_text == NULL) goto err;
  };

  if (files & FILES_R_BIN) {
    snprintf(fname_buf_R_bin, 128, "TVSet_%d_%s_R.bin", count, tvs->name);
    fname_buf_R_bin[127] = '\0';
    f_R_bin = fopen(fname_buf_R_bin, "wb");
    if (f_R_bin == NULL) goto err;
  };

  if (files & FILES_S_BIN) {
    snprintf(fname_buf_S_bin, 128, "TVSet_%d_%s_S.bin", count, tvs->name);
    fname_buf_S_bin[127] = '\0';
    f_S_bin = fopen(fname_buf_S_bin, "wb");
    if (f_S_bin == NULL) goto err;
  };

  if (files & FILES_DECR_BIN) {
    snprintf(fname_buf_decR_bin, 128, "TVSet_%d_%s_decR.bin", count, tvs->name);
    fname_buf_decR_bin[127] = '\0';
    f_decR_bin = fopen(fname_buf_decR_bin, "wb");
    if (f_decR_bin == NULL) goto err;
  };

  for (i = 0; i < count; ++i) {
    pack_ui32(seedbuf, i);
    if ((gtv_rv |= tv_generate_testvec(&tv)) < 0) goto err;
    if (files & FILES_TEXT) output_testvec_text(f_text, &tv);
    if (files & FILES_R_BIN) output_testvec_vectelt_bin(f_R_bin, &tv, tv.R);
    if (files & FILES_S_BIN) output_testvec_S_bin(f_S_bin, &tv);
    if (files & FILES_DECR_BIN) output_testvec_vectelt_bin(f_decR_bin, &tv, tv.decR);
  };

  rv = gtv_rv ? 3 : 0;

 err:
  if (f_text != NULL) {
    if (ferror(f_text) != 0) rv = -1;
    if (fclose(f_text) != 0) rv = -1;
  };

  if (f_R_bin != NULL) {
    if (ferror(f_R_bin) != 0) rv = -1;
    if (fclose(f_R_bin) != 0) rv = -1;
  };

  if (f_S_bin != NULL) {
    if (ferror(f_S_bin) != 0) rv = -1;
    if (fclose(f_S_bin) != 0) rv = -1;
  };

  if (f_decR_bin != NULL) {
    if (ferror(f_decR_bin) != 0) rv = -1;
    if (fclose(f_decR_bin) != 0) rv = -1;
  };

  if (tv.R != NULL) free(tv.R);
  if (tv.S != NULL) free(tv.S);
  if (tv.decR != NULL) free(tv.decR);

  return rv;
};

#ifndef NO_GETOPT
static void usage(FILE *f) {
  fprintf(f, "usage: generate-test-vectors [-qv] [-RSdt] [-c COUNT] "
	  "[TEST-VEC-SETS]\n");
};
#endif

static int process_testvecset(struct test_vector_set *tvs, int verbose, uint32_t count, unsigned int files) {
  int rv;

  if (verbose) printf("%s\n", tvs->name);

  rv = generate_testvecset_files(tvs, count, files);
  if (rv < 0) {
    fprintf(stderr, "generate-test-vectors: error generating %s\n", tvs->name);
    exit(1);
  };
  return rv;
};

int main(int argc, char *argv[]) {
  uint32_t count = 10;
  int verbose = 1;
  int files = FILES_ALL;
  size_t i;
  int rv = 0;

#ifndef NO_GETOPT
  int opt;
  int files_changed = 0;

  while ((opt = getopt(argc, argv, "c:RSdtqv")) >= 0) {
    switch (opt) {
    case 'c':
      {
        char *endptr = NULL;
        unsigned long int ul = strtoul(optarg, &endptr, 0);
        count = ul;
        if ((*optarg == '\0') || (*endptr != '\0')) {
          fprintf(stderr, "generate-test-vectors: count is not a valid number\n");
          return 2;
        };
        if (ul != (unsigned long int) count) {
          fprintf(stderr, "generate-test-vectors: count too large\n");
          return 2;
        };
      };
      break;
    case 'R':
      if (files_changed == 0) {
	files = 0;
	files_changed = 1;
      };
      files |= FILES_R_BIN;
      break;
    case 'S':
      if (files_changed == 0) {
	files = 0;
	files_changed = 1;
      };
      files |= FILES_S_BIN;
      break;
    case 'd':
      if (files_changed == 0) {
	files = 0;
	files_changed = 1;
      };
      files |= FILES_DECR_BIN;
      break;
    case 't':
      if (files_changed == 0) {
	files = 0;
	files_changed = 1;
      };
      files |= FILES_TEXT;
      break;
    case 'q':
      verbose = 0;
      break;
    case 'v':
      verbose = 1;
      break;
    default:
      usage(stderr);
      return 2;
    };
  };
#else /* NO_GETOPT, i.e. the Windows version */
  int optind = argc;

  /* usage: generate-test-vectors [COUNT] [FILES] [TEST-VEC-SETS] */

  switch (argc) {
  default:
    optind = 3;
    /* fall through */
  case 3:
    if (strcmp(argv[2], "all") == 0) {
      files = FILES_ALL;
    } else if (strcmp(argv[2], "s_bin") == 0) {
      files = FILES_S_BIN;
    } else if (strcmp(argv[2], "text") == 0) {
      /* not particularly useful on Windows; hash does not include CRs at EOL */
      files = FILES_TEXT;
    } else {
      fprintf(stderr, "generate-test-vectors: invalid file selector\n");
    };
    /* fall through */
  case 2:
    {
      char *optarg = argv[1];
      char *endptr = NULL;
      unsigned long int ul = strtoul(optarg, &endptr, 0);
      count = ul;
      if ((*optarg == '\0') || (*endptr != '\0')) {
	fprintf(stderr, "generate-test-vectors: count is not a valid number\n");
	return 2;
      };
      if (ul != (unsigned long int) count) {
	fprintf(stderr, "generate-test-vectors: count too large\n");
	return 2;
      };
    };
    /* fall through */
  case 1:
  case 0:
    break;
  };
#endif

  if (setup_testvecsets() < 0) {
    fprintf(stderr, "generate-test-vectors: error initializing test vector sets\n");
    return 1;
  };

  if (optind == argc) {
    /* no test vector set names on command line; generate them all */
    for (i = 0; i < test_vector_sets_count; ++i) {
      rv |= process_testvecset(&(test_vector_sets[i]), verbose, count, files);
    };
  } else {
    /* test vector set names provided on command line; generate only those */
    for (i = optind; i < argc; ++i) {
      rv |= process_testvecset(tvs_lookup(argv[i]), verbose, count, files);
    };
  };

  return rv;
};

