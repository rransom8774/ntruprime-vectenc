#ifndef Xq56zfmlsnhbp2cqxe7vj9mg93gxtoriemzrzox04ytgr3x8oywqk2nn7kt6itivt
#define Xq56zfmlsnhbp2cqxe7vj9mg93gxtoriemzrzox04ytgr3x8oywqk2nn7kt6itivt

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

typedef uint32_t vectelt;

struct vectcoder;

struct vectcoder *pqcr_vectcoder_new(vectelt *Mbuf, size_t nelts); /* clobbers Mbuf */
struct vectcoder *pqcr_vectcoder_new_uniform_bound(vectelt M, size_t nelts);
void pqcr_vectcoder_free(struct vectcoder *vc);

size_t pqcr_vectcoder_get_nelts(const struct vectcoder *vc);
size_t pqcr_vectcoder_get_nbytes_separate_root(const struct vectcoder *vc);
vectelt pqcr_vectcoder_get_root_bound(const struct vectcoder *vc);
size_t pqcr_vectcoder_get_nbytes(const struct vectcoder *vc);

void pqcr_vectcoder_encode_separate_root(const struct vectcoder *vc, uint8_t *S, vectelt *root, vectelt *Rbuf);
void pqcr_vectcoder_encode(const struct vectcoder *vc, uint8_t *S, vectelt *Rbuf);

void pqcr_vectcoder_decode_separate_root(const struct vectcoder *vc, vectelt *R, const uint8_t *S, vectelt root);
void pqcr_vectcoder_decode(const struct vectcoder *vc, vectelt *R, const uint8_t *S);

#endif
