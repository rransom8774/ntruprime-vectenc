
import sys
import struct
import hashlib
import argparse
import contextlib
import collections

import vectenc_ref

TestVector = collections.namedtuple('TestVector',
                                    ['seed', 'R', 'M', 'S', 'decR'])

u32le = struct.Struct('<I')

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
    decR = vectenc_ref.Decode(S, M)
    if decR != R:
        print("decR != R: %s" % seed.hex())
        pass
    return TestVector(seed, R, M, S, decR)

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

def generate_testvec_text(tv):
    rv = list()
    rv.append('Seed = ' + tv.seed.hex())
    rv.append('   R = ' + ', '.join(map(str, tv.R)))
    rv.append('   M = ' + ', '.join(map(str, tv.M)))
    rv.append('   S = ' + tv.S.hex())
    rv.append('decR = ' + ', '.join(map(str, tv.decR)))
    rv.append('')
    return ''.join(line + '\n' for line in rv)

u16le = struct.Struct('<H')

def R_to_bytes(V):
    rv = bytearray(len(V) * 2)
    for i in range(len(V)):
        v = V[i]
        if v > 32767:
            v |= 32768
            pass
        v &= 65535
        u16le.pack_into(rv, 2*i, v)
        pass
    return rv

class DataSink(object):
    def write(*args, **kwargs):
        pass
    pass
datasink = DataSink()

def maybe_open(files, file_id, fname, is_text):
    if file_id in files:
        if is_text:
            return open(fname, 'w', encoding='utf8')
        else:
            return open(fname, 'wb')
        pass
    else:
        return contextlib.nullcontext(datasink)
    pass

def generate_testvecset_files(tvsetname, tvsetfunc, count, files):
    with maybe_open(files, 'text', 'TVSet_%d_%s.txt' % (count, tvsetname), True) as f_text:
        with maybe_open(files, 'R', 'TVSet_%d_%s_R.bin' % (count, tvsetname), False) as f_R_bin:
            with maybe_open(files, 'S', 'TVSet_%d_%s_S.bin' % (count, tvsetname), False) as f_S_bin:
                with maybe_open(files, 'decR', 'TVSet_%d_%s_decR.bin' % (count, tvsetname), False) as f_decR_bin:
                    for i in range(count):
                        seed = u32le.pack(i)
                        tv = tvsetfunc(seed)
                        f_text.write(generate_testvec_text(tv))
                        f_R_bin.write(R_to_bytes(tv.R))
                        f_S_bin.write(tv.S)
                        f_decR_bin.write(R_to_bytes(tv.decR))
                        pass
                    pass
                pass
            pass
        pass
    pass

# main function for use as a standalone script

testvecsets_all = testvecset_map.keys()

ap = argparse.ArgumentParser(
    description="Generate test vectors for the NTRU Prime round 2 vector encoder.")
ap.add_argument('-v', '--verbose', action='store_true', default=True)
ap.add_argument('-q', '--quiet', action='store_false', dest='verbose')
ap.add_argument('-c', '--count', type=int, default=10)
ap.add_argument('-R', '--R-bin', action='append_const', const='R', dest='files')
ap.add_argument('-S', '--S-bin', action='append_const', const='S', dest='files')
ap.add_argument('-d', '--decR-bin', action='append_const', const='decR', dest='files')
ap.add_argument('-t', '--text', action='append_const', const='text', dest='files')
ap.add_argument('testvecsets', nargs='*', default=testvecsets_all)

def main(argv):
    args = ap.parse_args(argv[1:])
    if args.files == None or len(args.files) == 0:
        args.files = ['R', 'S', 'decR', 'text']
        pass
    for tvsetname in args.testvecsets:
        tvsetfunc = testvecset_map[tvsetname]
        if args.verbose:
            print(tvsetname)
            pass
        generate_testvecset_files(tvsetname, tvsetfunc, args.count, args.files)
        pass
    return 0

if __name__== '__main__':
    sys.exit(main(sys.argv))
    pass

