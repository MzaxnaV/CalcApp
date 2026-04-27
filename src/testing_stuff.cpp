#include <bits/stdc++.h>
using namespace std;

using ll = long long;

static const int MAXN = 1 << 20;          // 2^20

// fast input
inline int fastReadInt() {
    int x = 0, c = getchar_unlocked();
    while (c <= ' ') c = getchar_unlocked();
    bool neg = false;
    if (c == '-') { neg = true; c = getchar_unlocked(); }
    for (; c >= '0'; c = getchar_unlocked())
        x = x * 10 + (c - '0');
    return neg ? -x : x;
}

// fast output of long long
char outbuf[1 << 20];
int outp = 0;
inline void flushOut() {
    fwrite(outbuf, 1, outp, stdout);
    outp = 0;
}
inline void fastWriteLL(ll x) {
    if (outp > (1 << 20) - 30) flushOut();
    if (x == 0) { outbuf[outp++] = '0'; outbuf[outp++] = '\n'; return; }
    char s[30];
    int n = 0;
    while (x) {
        s[n++] = char('0' + x % 10);
        x /= 10;
    }
    for (int i = n - 1; i >= 0; --i) outbuf[outp++] = s[i];
    outbuf[outp++] = '\n';
}

int main() {
    // read n
    int n = fastReadInt();
    int N = 1 << n;

    vector<int> a(N);
    for (int i = 0; i < N; ++i) a[i] = fastReadInt();

    // tables for each level 1..n
    vector<ll> inv0(n + 1, 0), inv1(n + 1, 0);
    vector<int> tmp(N);

    // bottom‑up merge sort, level by level
    for (int k = 1; k <= n; ++k) {
        int sz = 1 << k;
        int half = sz >> 1;
        ll cntInv0 = 0;
        ll cntEq = 0;

        for (int start = 0; start < N; start += sz) {
            int l = start, m = start + half, r = start + sz;
            int i = l, j = m, p = l;

            while (i < m && j < r) {
                if (a[i] < a[j]) {
                    tmp[p++] = a[i++];
                } else if (a[i] > a[j]) {
                    cntInv0 += (ll)(m - i);
                    tmp[p++] = a[j++];
                } else { // equal
                    int val = a[i];
                    ll cntL = 0;
                    while (i < m && a[i] == val) { ++cntL; ++i; }
                    ll cntR = 0;
                    while (j < r && a[j] == val) { ++cntR; ++j; }
                    cntEq += cntL * cntR;
                    for (ll t = 0; t < cntL; ++t) tmp[p++] = val;
                    for (ll t = 0; t < cntR; ++t) tmp[p++] = val;
                }
            }
            while (i < m) tmp[p++] = a[i++];
            while (j < r) tmp[p++] = a[j++];
            // copy back
            for (int idx = start; idx < r; ++idx) a[idx] = tmp[idx];
        }

        ll totalPairs = (ll)(N / sz) * half * half;
        inv0[k] = cntInv0;
        inv1[k] = totalPairs - cntInv0 - cntEq;
    }

    // initial answer = sum inv0
    ll curAns = 0;
    for (int k = 1; k <= n; ++k) curAns += inv0[k];

    // read queries
    int m = fastReadInt();
    vector<int> flipped(n + 1, 0); // 0 = normal, 1 = swapped

    for (int qi = 0; qi < m; ++qi) {
        int q = fastReadInt();
        for (int k = 1; k <= q; ++k) {
            flipped[k] ^= 1;
            if (flipped[k]) {
                curAns = curAns - inv0[k] + inv1[k];
            } else {
                curAns = curAns - inv1[k] + inv0[k];
            }
        }
        fastWriteLL(curAns);
    }
    flushOut();
    return 0;
}