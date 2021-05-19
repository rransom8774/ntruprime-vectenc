
import sys
import struct
import hashlib
import argparse
import collections

import vectenc_ref

TestVector = collections.namedtuple('TestVector', ['R', 'M', 'S', 'seed'])

u32le = struct.Struct('<i')

def generate_R(M, seed):
    # non-uniform sampling for simplicity
    hobj = hashlib.shake_256()
    hobj.update(seed)
    Rbytes = hobj.digest(len(M)*4)
    R = list()
    for i in range(len(M)):
        m = M[i]
        rbytes = Rbytes[4*i:4*(i+1)]
        r = u32le.unpack(rbytes)[0] % m
        R.append(r)
        pass
    return R

def generate_testvec(M, seed):
    R = generate_R(M, seed)
    S = bytes(vectenc_ref.Encode(R, M))
    return TestVector(R, M, S, seed)

testvecset_map = dict()

def tvs_const(m, n):
    M = [m] * n
    def const_tvs_func(seed):
        return generate_testvec(M, seed)
    testvecset_map['const_%d_%d' % (m, n)] = const_tvs_func
    pass

# Streamlined NTRU Prime vector formats, unrounded

tvs_const(4591, 761)
tvs_const(4621, 653)
tvs_const(5167, 857)

# Streamlined NTRU Prime vector formats, rounded

def ntruprime_rounded_m(q):
    qm3 = q % 3
    if qm3 == 1:
        return (q - 1)//2
    elif qm3 == 2:
        return (q + 1)//2
    else: # qm3 == 0
        raise Exception("%d is not odd and cannot be a valid value of q" % q)
    pass

tvs_const(ntruprime_rounded_m(4591), 761)
tvs_const(ntruprime_rounded_m(4621), 653)
tvs_const(ntruprime_rounded_m(5167), 857)

# pkpsig z vector formats

tvs_const(797, 55)
tvs_const(977, 61)
tvs_const(1409, 87)
tvs_const(1789, 111)

# pkpsig rho vector formats, unsquished

tvs_const(55, 55)
tvs_const(61, 61)
tvs_const(87, 87)
tvs_const(111, 111)

def tvs_squished_perm(n):
    M = [n - i for i in range(n-1)]
    def squished_perm_tvs_func(seed):
        return generate_testvec(M, seed)
    testvecset_map['squished_perm_%d' % n] = squished_perm_tvs_func
    pass

# pkpsig rho vector formats, squished

tvs_squished_perm(55)
tvs_squished_perm(61)
tvs_squished_perm(87)
tvs_squished_perm(111)

def tvs_random(n, ilb, iub, Mseed):
    Max = [iub-(ilb-1) for i in range(n)]
    M = [rand+ilb for rand in generate_R(Max, Mseed)]
    def random_tvs_func(seed):
        return generate_testvec(M, seed)
    testvecset_map['random_%d_%d_%d_%s' %
                   (n, ilb, iub, str(Mseed, encoding='us-ascii'))] = random_tvs_func
    pass

# miscellaneous random vector formats with no particular application in mind

tvs_random(128, 2, 15, b'foobar')
tvs_random(768, 2048, 2048+256, b'foo')

# text and test-vector file generation

def generate_testvecset_text(tv):
    rv = list()
    rv.append('Seed = ' + tv.seed.hex())
    rv.append('R = ' + ', '.join(map(str, tv.R)))
    rv.append('M = ' + ', '.join(map(str, tv.M)))
    rv.append('S = ' + tv.S.hex())
    rv.append('')
    return ''.join(line + '\n' for line in rv)

def generate_testvecset_file(tvsetname, tvsetfunc, count):
    with open('TVSet_%d_%s.txt' % (count, tvsetname), 'w', encoding='utf8') as f:
        for i in range(count):
            seed = u32le.pack(i)
            tv = tvsetfunc(seed)
            f.write(generate_testvecset_text(tv))
            pass
        pass
    pass

# main function for use as a standalone script

testvecsets_all = testvecset_map.keys()

ap = argparse.ArgumentParser(
    description="Generate test vectors for the NTRU Prime round 2 vector encoder.")
ap.add_argument('-v', '--verbose', action='store_true')
ap.add_argument('-q', '--quiet', action='store_false', dest='verbose')
ap.add_argument('-c', '--count', type=int, default=10)
ap.add_argument('testvecsets', nargs='*', default=testvecsets_all)

def main(argv):
    args = ap.parse_args(argv[1:])
    for tvsetname in args.testvecsets:
        tvsetfunc = testvecset_map[tvsetname]
        print(tvsetname)
        generate_testvecset_file(tvsetname, tvsetfunc, args.count)
        pass
    return 0

if __name__== '__main__':
    sys.exit(main(sys.argv))
    pass

