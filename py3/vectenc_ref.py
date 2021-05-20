
# Copied from the NTRU Prime round 2 submission document.

LIMIT = 16384

def Encode(R, M):
    if len(M) == 0: return []
    S = []
    if len(M) == 1:
        r, m = R[0], M[0]
        while m > 1:
            S += [r%256]
            r, m = r//256, (m+255)//256
        return S
    R2, M2 = [], []
    for i in range(0, len(M) - 1, 2):
        m, r = M[i]*M[i+1], R[i] + M[i]*R[i+1]
        while m >= LIMIT:
            S += [r%256]
            r, m = r//256, (m+255)//256
        R2 += [r]
        M2 += [m]
    if len(M) & 1:
        R2 += [R[-1]]
        M2 += [M[-1]]
    return S + Encode(R2, M2)

def Decode(S, M):
    if len(M) == 0: return []
    if len(M) == 1: return [sum(S[i] * 256**i for i in range(len(S))) % M[0]]
    k = 0
    bottom, M2 = [], []
    for i in range(0, len(M)-1, 2):
        m, r, t = M[i]*M[i+1], 0, 1
        while m >= LIMIT:
            r, t, k, m = r + S[k]*t, t*256, k+1, (m+255)//256
        bottom += [(r, t)]
        M2 += [m]
    if len(M) & 1:
        M2 += [M[-1]]
    R2 = Decode(S[k:], M2)
    R = []
    for i in range(0, len(M)-1, 2):
        r, t = bottom[i//2]
        r += t*R2[i//2]
        R += [r % M[i]]
        R += [(r // M[i]) % M[i+1]]
    if len(M) & 1:
        R += [R2[-1]]
    return R

