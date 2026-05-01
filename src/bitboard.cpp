#include "bitboard.h"


/* === Public Variables === */
Bitboard pawnAttacks[2][64];
Bitboard knightAttacks[64];
Bitboard kingAttacks[64];
Bitboard betweenTable[64][64];
Bitboard rayTable[64][64];

/* === Private Variables === */
Bitboard rookMasks[64];
Bitboard bishopMasks[64];
Bitboard rookAttacks[102400];
Bitboard bishopAttacks[5248];
int rookOffsets[64];
int bishopOffsets[64];

constexpr uint64_t rookMagics[64] = {
    0x8a80104000800020ULL, 0x140002000100040ULL,  0x2801880a0017001ULL,  0x100081001000420ULL,
    0x200020010080420ULL,  0x3001c0002010008ULL,  0x8480008002000100ULL, 0x2080088004402900ULL,
    0x800098204000ULL,     0x2024401000200040ULL, 0x100802000801000ULL,  0x120800800801000ULL,
    0x208808088000400ULL,  0x2802200800400ULL,    0x2200800100020080ULL, 0x801000060821100ULL,
    0x80044006422000ULL,   0x100808020004000ULL,  0x12108a0010204200ULL, 0x140848010000802ULL,
    0x481828014002800ULL,  0x8094004002004100ULL, 0x4010040010010802ULL, 0x20008806104ULL,
    0x100400080208000ULL,  0x2040002120081000ULL, 0x21200680100081ULL,   0x20100080080080ULL,
    0x2000a00200410ULL,    0x20080800400ULL,      0x80088400100102ULL,   0x80004600042881ULL,
    0x4040008040800020ULL, 0x440003000200801ULL,  0x4200011004500ULL,    0x188020010100100ULL,
    0x14800401802800ULL,   0x2080040080800200ULL, 0x124080204001001ULL,  0x200046502000484ULL,
    0x480400080088020ULL,  0x1000422010034000ULL, 0x30200100110040ULL,   0x100021010009ULL,
    0x2002080100110004ULL, 0x202008004008002ULL,  0x20020004010100ULL,   0x2048440040820001ULL,
    0x101002200408200ULL,  0x40802000401080ULL,   0x4008142004410100ULL, 0x2060820c0120200ULL,
    0x1001004080100ULL,    0x20c020080040080ULL,  0x2935610830022400ULL, 0x44440041009200ULL,
    0x280001040802101ULL,  0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL,
    0x20030a0244872ULL,    0x12001008414402ULL,   0x2006104900a0804ULL,  0x1004081002402ULL
};

constexpr uint64_t bishopMagics[64] = {
    0x40040844404084ULL,   0x2004208a004208ULL,   0x10190041080202ULL,   0x108060845042010ULL,
    0x581104180800210ULL,  0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
    0x4050404440404ULL,    0x21001420088ULL,       0x24d0080801082102ULL, 0x1020a0a020400ULL,
    0x40308200402ULL,      0x4011002100800ULL,     0x401484104104005ULL,  0x801010402020200ULL,
    0x400210c3880100ULL,   0x404022024108200ULL,   0x810018200204102ULL,  0x4002801a02003ULL,
    0x85040820080400ULL,   0x810102c808880400ULL,  0xe900410884800ULL,    0x8002020480840102ULL,
    0x220200865090201ULL,  0x2010100a02021202ULL,  0x152048408022401ULL,  0x20080002081110ULL,
    0x4001001021004000ULL, 0x800040400a011002ULL,  0xe4004081011002ULL,   0x1c004001012080ULL,
    0x8004200962a00220ULL, 0x8422100208500202ULL,  0x2000402200300c08ULL, 0x8646020080080080ULL,
    0x80020a0200100808ULL, 0x2010004880111000ULL,  0x623000a080011400ULL, 0x42008c0340209202ULL,
    0x209188240001000ULL,  0x400408a884001800ULL,  0x110400a6080400ULL,   0x1840060a44020800ULL,
    0x90080104000041ULL,   0x201011000808101ULL,   0x1a2208080504f080ULL, 0x8012020600211212ULL,
    0x500861011240000ULL,  0x180806108200800ULL,   0x4000020e01040044ULL, 0x300000261044000aULL,
    0x802241102020002ULL,  0x20906061210001ULL,    0x5a84841004010310ULL, 0x4010801011c04ULL,
    0xa010109502200ULL,    0x4a02012000ULL,         0x500201010098b028ULL, 0x8040002811040900ULL,
    0x28000010020204ULL,   0x6000020202d0240ULL,   0x8918844842082200ULL, 0x4010011029020020ULL
};

constexpr int rookShifts[64] = {
    52, 53, 53, 53, 53, 53, 53, 52,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    52, 53, 53, 53, 53, 53, 53, 52
};

constexpr int bishopShifts[64] = {
    58, 59, 59, 59, 59, 59, 59, 58,
    59, 59, 59, 59, 59, 59, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 59, 59, 59, 59, 59, 59,
    58, 59, 59, 59, 59, 59, 59, 58
};

/* === Functions === */
Bitboard indexToOccupancy(int index, int numBits, Bitboard mask) {
    Bitboard occupancy = 0;
    for (int i = 0; i < numBits; i++) {
        int bitPos = __builtin_ctzll(mask);
        mask &= mask - 1;
        if (index & (1 << i)) { occupancy |= (1ULL << bitPos); }
    }
    return occupancy;
}

Bitboard computeRookAttacks(int sq, Bitboard occ) {
    Bitboard attacks = 0;
    int r = sq / 8;
    int f = sq % 8;

    for (int i = r + 1; i <= 7; i++) { attacks |= (1ULL << (i * 8 + f)); if (occ & (1ULL << (i * 8 + f))) break; }
    for (int i = r - 1; i >= 0; i--) { attacks |= (1ULL << (i * 8 + f)); if (occ & (1ULL << (i * 8 + f))) break; }
    for (int j = f + 1; j <= 7; j++) { attacks |= (1ULL << (r * 8 + j)); if (occ & (1ULL << (r * 8 + j))) break; }
    for (int j = f - 1; j >= 0; j--) { attacks |= (1ULL << (r * 8 + j)); if (occ & (1ULL << (r * 8 + j))) break; }

    return attacks;
}

Bitboard computeBishopAttacks(int sq, Bitboard occ) {
    Bitboard attacks = 0;
    int r = sq / 8;
    int f = sq % 8;

    for (int tr = r + 1, tf = f + 1; tr <= 7 && tf <= 7; tr++, tf++) { attacks |= 1ULL << (tr * 8 + tf); if (occ & (1ULL << (tr * 8 + tf))) break; }
    for (int tr = r + 1, tf = f - 1; tr <= 7 && tf >= 0; tr++, tf--) { attacks |= 1ULL << (tr * 8 + tf); if (occ & (1ULL << (tr * 8 + tf))) break; }
    for (int tr = r - 1, tf = f + 1; tr >= 0 && tf <= 7; tr--, tf++) { attacks |= 1ULL << (tr * 8 + tf); if (occ & (1ULL << (tr * 8 + tf))) break; }
    for (int tr = r - 1, tf = f - 1; tr >= 0 && tf >= 0; tr--, tf--) { attacks |= 1ULL << (tr * 8 + tf); if (occ & (1ULL << (tr * 8 + tf))) break; }

    return attacks;
}

void fillRookAttacks() {
    for (int sq = 0; sq < 64; sq++) {
        int numBits = 64 - rookShifts[sq];
        int numConfigs = 1 << numBits;

        for (int index = 0; index < numConfigs; index++) {
            Bitboard occ = indexToOccupancy(index, numBits, rookMasks[sq]);
            Bitboard magicIndex = (occ * rookMagics[sq]) >> rookShifts[sq];
            Bitboard attacks = computeRookAttacks(sq, occ);
            rookAttacks[rookOffsets[sq] + magicIndex] = attacks;
        }
    }
}
void fillBishopAttacks() {
    for (int sq = 0; sq < 64; sq++) {
        int numBits = 64 - bishopShifts[sq];
        int numConfigs = 1 << numBits;

        for (int index = 0; index < numConfigs; index++) {
            Bitboard occ = indexToOccupancy(index, numBits, bishopMasks[sq]);
            Bitboard magicIndex = (occ * bishopMagics[sq]) >> bishopShifts[sq];
            Bitboard attacks = computeBishopAttacks(sq, occ);
            bishopAttacks[bishopOffsets[sq] + magicIndex] = attacks;
        }
    }
}

void init() {
    for (int i = 0; i < 64; i++) {
        if (i >= 56) pawnAttacks[WHITE][i] = 0;
        else if (i % 8 == 0) pawnAttacks[WHITE][i] = 1ULL << (9 + i);
        else if (i % 8 == 7) pawnAttacks[WHITE][i] = 1ULL << (7 + i);
        else pawnAttacks[WHITE][i] = 1ULL << (9 + i) | 1ULL << (7 + i);
    }

    for (int i = 0; i < 64; i++) {
        if (i <= 7) pawnAttacks[BLACK][i] = 0;
        else if (i % 8 == 0) pawnAttacks[BLACK][i] = 1ULL << (i - 7);
        else if (i % 8 == 7) pawnAttacks[BLACK][i] = 1ULL << (i - 9);
        else pawnAttacks[BLACK][i] = (1ULL << (i - 7)) | (1ULL << (i - 9));
    }

    //Kings - horrible code I made but it works \_(-_-)_/
    for (int i = 0; i < 64; i++) {
        Bitboard temp_val = 1ULL << i;
        if (i % 8 == 0 || i % 8 == 7 || i <= 7 || i >= 56) {
            if (i % 8 == 0 ) {
                if (i == 7) { kingAttacks[i] = temp_val << 1 | (temp_val << 8 | temp_val << 9); }
                else if (i == 56) { kingAttacks[i] = temp_val << 1 | (temp_val >> 7 | temp_val >> 8); }
                else { kingAttacks[i] = temp_val << 1 | ((temp_val << 8 | temp_val << 9) | (temp_val >> 7 | temp_val >> 8)); }
            }
            else if (i % 8 == 7) {
                if (i == 56) { kingAttacks[i] = temp_val >> 1 | (temp_val >> 8 | temp_val >> 9); }
                else if (i == 7) { kingAttacks[i] = temp_val >> 1 | (temp_val << 7 | temp_val << 8); }
                else { kingAttacks[i] = temp_val >> 1 | ((temp_val << 7 | temp_val << 8) | (temp_val >> 8 | temp_val >> 9)); }
            }
            else if (i >= 1 && i <= 7 || i >= 56 && i <= 63) {
                if (i >= 1 && i <= 7 ) { kingAttacks[i] = (temp_val << 1 | temp_val >> 1) | (temp_val << 7 | temp_val << 8 | temp_val << 9); }
                else { kingAttacks[i] = (temp_val << 1 | temp_val >> 1) | (temp_val >> 7 | temp_val >> 8 | temp_val >> 9); }
            }
        }
        else{ kingAttacks[i] = (temp_val >> 1 | temp_val << 1) | ((temp_val << 7 | temp_val << 8 | temp_val << 9) | (temp_val >> 7 | temp_val >> 8 | temp_val >> 9)); }
    }

    for (int sq = 0; sq < 64; sq++) {
        knightAttacks[sq] = 0;
        int r = sq / 8;
        int f = sq % 8;

        int moves[8][2] = {
            {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
            {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
        };

        for (int i = 0; i < 8; i++) {
            int tr = r + moves[i][0];
            int tf = f + moves[i][1];
            if (tr >= 0 && tr <= 7 && tf >= 0 && tf <= 7) {
                knightAttacks[sq] |= (1ULL << (tr * 8 + tf));
            }
        }
    }

    //Masks for sliding pieces
    for (int i = 0; i < 64; i++) { //Rooks
        int r = i / 8;
        int f = i % 8;
        uint64_t temp_mask = 0;
        for (int up    = r + 1; up <= 6;   up++)   temp_mask |= 1ULL << (up * 8 + f);
        for (int down  = r - 1; down >= 1; down--) temp_mask |= 1ULL << (down * 8 + f);
        for (int right = f + 1; right <= 6; right++) temp_mask |= 1ULL << (r * 8 + right);
        for (int left  = f - 1; left >= 1;  left--) temp_mask |= 1ULL << (r * 8 + left);
        rookMasks[i] = temp_mask;
    }
    for (int i = 0; i < 64; i++) { //Bishops
        int r = i / 8;
        int f = i % 8;
        uint64_t temp_mask = 0;
        for (int tr = r + 1, tf = f + 1; tr <= 6 && tf <= 6; tr++, tf++) temp_mask |= 1ULL << (tr * 8 + tf);
        for (int tr = r + 1, tf = f - 1; tr <= 6 && tf >= 1; tr++, tf--) temp_mask |= 1ULL << (tr * 8 + tf);
        for (int tr = r - 1, tf = f + 1; tr >= 1 && tf <= 6; tr--, tf++) temp_mask |= 1ULL << (tr * 8 + tf);
        for (int tr = r - 1, tf = f - 1; tr >= 1 && tf >= 1; tr--, tf--) temp_mask |= 1ULL << (tr * 8 + tf);
        bishopMasks[i] = temp_mask;
    }

    rookOffsets[0] = 0;
    bishopOffsets[0] = 0;
    for (int i = 1; i < 64; i++) { rookOffsets[i] = rookOffsets[i - 1] + (1ULL << (64 - rookShifts[i - 1])); }
    for (int i = 1; i < 64; i++) { bishopOffsets[i] = bishopOffsets[i - 1] + (1ULL << (64 - bishopShifts[i - 1])); }

    fillBishopAttacks();
    fillRookAttacks();

    for (int sq1 = 0; sq1 < 64; sq1++) {
        for (int sq2 = 0; sq2 < 64; sq2++) {
            betweenTable[sq1][sq2] = 0;
            if (sq1 == sq2) continue;
            int r1 = sq1 / 8;
            int r2 = sq2 / 8;
            int f1 = sq1 % 8;
            int f2 = sq2 % 8;

            if (r1 == r2 && f1 != f2){
                int minF = std::min(f1, f2);
                int maxF = std::max(f1, f2);
                for (int f = minF + 1; f < maxF; f++) {
                    betweenTable[sq1][sq2] |= 1ULL << (r1 * 8 + f);
                }
            }

            else if (r1 != r2 && f1 == f2) {
                int minR = std::min(r1, r2);
                int maxR = std::max(r1, r2);
                for (int r = minR + 1; r < maxR; r++) {
                    betweenTable[sq1][sq2] |= 1ULL << (r * 8 + f1);
                }
            }

            else if (abs(r1 - r2) == abs(f1 - f2)) {
                int dr = (r2 > r1) ? 1 : -1;
                int df = (f2 > f1) ? 1 : -1;
                int r = r1 + dr, f = f1 + df;
                while (r != r2 && f != f2) {
                    betweenTable[sq1][sq2] |= 1ULL << (r * 8 + f);
                    r += dr; f += df;
                }
            }
        }
    }
}
