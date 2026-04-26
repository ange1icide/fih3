// Fih3 Copyright (C) 2026 [Rostik Katayev] - Licensed under GPL-3.0

/*
========================================================
|                       F  I  H ³                      |
========================================================
|      Fih³ is a third re-write of the fih engine      |
|                                                      |
|      Author : Rosik Katayev                          |
|      Date   : 4/12/2026                              |
========================================================
|             C O M P I L E  C O M M A N D             |
========================================================
| g++ -O3 -march=native -flto -DNDEBUG main.cpp -o fih |
========================================================
|                 F E A T U R E  S E T                 |
========================================================
|   1.  Bitboards for board representation             |    🟩
|   2.  32-bit move encoding                           |    🟩
|   3.  Templated move generator                       |    🟨
|   4.  Alpha-beta negamax search                      |    🟩
|   5.  Transposition tables                           |    🟨
|   6.  Quiescence search                              |
|   7.  Move ordering                                  |
|   8.  Search extensions                              |
|   9.  Pruning                                        |
|   10. Late move reductions                           |
|   11. Internal iterative reductions                  |
|   12. Razoring                                       |
|   13. Principal variation search                     |
|   14. Iterative deepening                            |
|   15. Lazy SMP threading                             |
|   16. Raw hand crafted eval                          |
|   17. NNUE (maybe sometime later)                    |
========================================================
|   🟩 = Done fully optimized as much as needed        |
|   🟨 = Not sure if it is as optimized as it can be   |
|   🟥 = Needs a lot of work                           |
========================================================
go cry about the fact that this is all in one file
im never touching c++ headers
*/


#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <ctime>


using Bitboard = uint64_t;
using Move = uint32_t;

static const int PIECE_OFFSET = 7;
const std::string THEME = "\033[38;2;255;209;220m";

static const char* pieceToChar[13] = {
    " ",
    "P","N","B","R","Q","K",
    "p","n","b","r","q","k"
};

static const int indexTable[64] = {
    56,57,58,59,60,61,62,63,
    48,49,50,51,52,53,54,55,
    40,41,42,43,44,45,46,47,
    32,33,34,35,36,37,38,39,
    24,25,26,27,28,29,30,31,
    16,17,18,19,20,21,22,23,
    8,9,10,11,12,13,14,15,
    0,1,2, 3, 4, 5, 6, 7
};

enum PieceType {
    PAWN = 1,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};

enum Color {
    WHITE,
    BLACK
};

enum GenType {
    CAPTURES,
    QUIETS,
    ALL,
    EVASIONS
};

enum Bound : uint8_t { EXACT, LOWER, UPPER };

static const int pieceValue[6] {100, 300, 330, 500, 900, 10000};

struct BoardState {
    int castlingRights;
    int enpassant_square;
    int side;
    int halfmove_clock;
    uint64_t hash;
};

struct TTEntry {
    uint64_t key;
    int score;
    int depth;
    Move move;
    uint8_t bound;
};

const int TT_SIZE = 1 << 24;
TTEntry tt[TT_SIZE];
const uint64_t TT_MASK = TT_SIZE - 1;

const int castling_rights_update[64] = {
    13, 15, 15, 15, 12, 15, 15, 14, // Rank 1 (White corners/king)
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11  // Rank 8 (Black corners/king)
};

inline Move encodeMove(int to, int from, int piece, int capture, int promotion, bool enpassant, bool castle) { Move move = (Move)(to) | (Move)(from) << 6 | (Move)(piece + PIECE_OFFSET) << 12 | (Move)(capture + PIECE_OFFSET) << 16 | (Move)(promotion + PIECE_OFFSET) << 20 | (Move)(enpassant) << 24 | (Move)(castle) << 25;
    return move;
}

inline int decodeTo(Move move) { return move & 0x3F; }
inline int decodeFrom(Move move) { return (move >> 6) & 0x3F; }
inline int decodePiece(Move move) { return ((move >> 12) & 0x0F) - PIECE_OFFSET; }
inline int decodeCapture(Move move) { return ((move >> 16) & 0x0F) - PIECE_OFFSET; }
inline int decodePromotion(Move move) { return ((move >> 20) & 0x0F) - PIECE_OFFSET; }
inline bool decodeEnPassant(Move move)  { return ((move >> 24) & 0x01); }
inline bool decodeCastle(Move move) { return (move >> 25) & 0x01; }

inline bool isCapture(Move move) { return decodeCapture(move) != 0; }
inline bool isPromotion(Move move) { return decodePromotion(move) != 0; }

static const std::string defaultBoard = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Bitboard pawnAttacksW[64];
Bitboard pawnAttacksB[64];
Bitboard knightAttacks[64];
Bitboard kingAttacks[64];

int rookOffsets[64];
int bishopOffsets[64];
Bitboard rookMasks[64];
Bitboard bishopMasks[64];
Bitboard rookAttacks[102400];
Bitboard bishopAttacks[5248];
Bitboard betweenTable[64][64];

uint64_t rookMagics[64] = {
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
uint64_t bishopMagics[64] = {
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
int rookShifts[64] = {
    52, 53, 53, 53, 53, 53, 53, 52,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    53, 54, 54, 54, 54, 54, 54, 53,
    52, 53, 53, 53, 53, 53, 53, 52
};
int bishopShifts[64] = {
    58, 59, 59, 59, 59, 59, 59, 58,
    59, 59, 59, 59, 59, 59, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 55, 55, 57, 59, 59,
    59, 59, 57, 57, 57, 57, 59, 59,
    59, 59, 59, 59, 59, 59, 59, 59,
    58, 59, 59, 59, 59, 59, 59, 58
};

uint64_t zobristPiece[13][64];   // [piece -6..6 offset][square]
uint64_t zobristSide;            // XOR in when it's black's turn
uint64_t zobristCastle[16];      // one per castling rights combination
uint64_t zobristEP[8];           // one per en passant file (0-7)

TTEntry& probe(uint64_t hash) {
    return tt[hash & TT_MASK];
}

int full_move = 0;

/* === Classes === */

class Board {
    public:
        /*================= Variables =================*/

        Color side = WHITE;
        Bitboard byTypeBB[7];
        Bitboard byColorBB[2];
        int pieceCount[2][7];

        inline Bitboard pieces(Color c) { return byColorBB[c]; }
        inline Bitboard pieces(PieceType pt) { return byTypeBB[pt]; }
        inline Bitboard pieces(Color c, PieceType pt) { return byTypeBB[pt] & byColorBB[c]; }
        inline Bitboard occupancy() { return byColorBB[WHITE] | byColorBB[BLACK]; }
        inline Bitboard occupancySide(Color side) { return byColorBB[side]; }
        inline int getPieceAt(int sq) { return mailbox[sq]; }
        inline bool getCastleR(int type) { if (type == 1) { return castling_Rights & 1; } if (type == 2) { return castling_Rights & 2; } if (type == 3) { return castling_Rights & 4; } if (type == 4) { return castling_Rights & 8; } return false;} // 1 = wkscr, 2 = wqscr, 3 = bkscr, 4 = bqscr

        int halfmove_clock = 0;
        int enpassant_square = -1;
        int castling_Rights = 0;

        Move moveHistory[4096];
        BoardState stateHistory[4096];
        int HistoryIndex = 0;

        uint64_t hash = 0;

        int mailbox[64];

        /*================= Functions =================*/

        std::string decodeMoveInt(Move best) {
            int from = decodeFrom(best);
            int to   = decodeTo(best);
            std::string moveStr;
            moveStr += (char)('a' + from % 8);
            moveStr += (char)('1' + from / 8);
            moveStr += (char)('a' + to % 8);
            moveStr += (char)('1' + to / 8);

            if (decodePromotion(best) != 0) {
                int promo = decodePromotion(best);
                if (promo < 0) promo = -promo;
                switch(promo) {
                    case 5: moveStr += 'q'; break;
                    case 4: moveStr += 'r'; break;
                    case 3: moveStr += 'b'; break;
                    case 2: moveStr += 'n'; break;
                }
            }
            return moveStr;
        }

        void newGame() {
            byTypeBB[PAWN] = byTypeBB[KNIGHT] = byTypeBB[BISHOP] = byTypeBB[ROOK] = byTypeBB[QUEEN] = byTypeBB[KING] = 0ULL;
            byColorBB[WHITE] = byColorBB[BLACK] = 0ULL;

            for (int i = 0; i < 64; i++) mailbox[i] = 0;
            castling_Rights = 0;
            enpassant_square = -1;
            halfmove_clock = 0;
            HistoryIndex = 0;
            for (int i = 0; i < 7; i++) { pieceCount[0][i] = 0; pieceCount[1][i] = 0; }
            for (int i = 0; i < 4096; i++) {
                moveHistory[i] = 0;
                BoardState emptyState;
                emptyState.castlingRights = 0;
                emptyState.side = 0;
                emptyState.halfmove_clock = 0;
                stateHistory[i] = emptyState;
            }
            loadFEN(defaultBoard);
        }

        void printBoard() {
            std::cout << "\n+---+---+---+---+---+---+---+---+\n";
            for (int r = 0; r < 8; r++) {
                for (int f = 0; f < 8; f++) {
                    int p = mailbox[indexTable[r * 8 + f]];
                    const char* symbol;
                    if (p == 0) symbol = pieceToChar[0];
                    else if (p > 0) symbol = pieceToChar[p];
                    else symbol = pieceToChar[-p + 6];
                    std::cout << "| " << symbol << " ";
                }   std::cout << "| " << (8 - r) <<"\n+---+---+---+---+---+---+---+---+\n";
            }
            std::cout << "  a   b   c   d   e   f   g   h\n\n";
            std::cout << "Fen: "<< toFen() << std::endl;
            std::cout << "Hash: " << hash << std::endl;
            std::cout << "Checkers: " << getCheckers(side) << "\n\n";
        }

        void loadFEN(const std::string& fen) {
            byTypeBB[PAWN] = byTypeBB[KNIGHT] = byTypeBB[BISHOP] = byTypeBB[ROOK] = byTypeBB[QUEEN] = byTypeBB[KING] = 0ULL;
            byColorBB[WHITE] = byColorBB[BLACK] = 0ULL;
            for (int i = 0; i < 64; i++) mailbox[i] = 0;
            castling_Rights = 0;
            enpassant_square = -1;
            halfmove_clock = 0;
            HistoryIndex = 0;

            std::string parts[6];
            int partIndex = 0;
            for (char c : fen) {
                if (c == ' ') { partIndex++; if (partIndex >= 6) break; }
                else parts[partIndex] += c;
            }

            int sq = 56;
            for (char c : parts[0]) {
                if (c == '/') {
                    sq -= 16;
                } else if (c >= '1' && c <= '8') {
                    sq += (c - '0');
                } else {
                    int piece = 0;
                    switch(c) {
                        case 'P': piece =  1; break;
                        case 'p': piece = -1; break;
                        case 'N': piece =  2; break;
                        case 'n': piece = -2; break;
                        case 'B': piece =  3; break;
                        case 'b': piece = -3; break;
                        case 'R': piece =  4; break;
                        case 'r': piece = -4; break;
                        case 'Q': piece =  5; break;
                        case 'q': piece = -5; break;
                        case 'K': piece =  6; break;
                        case 'k': piece = -6; break;
                    }
                    placePiece(sq, piece);
                    sq++;
                }
            }

            side = (parts[1] == "w") ? WHITE : BLACK;

            for (char c : parts[2]) {
                switch(c) { // these might be wrong
                    case 'K': castling_Rights |= 1; break;
                    case 'Q': castling_Rights |= 2; break;
                    case 'k': castling_Rights |= 4; break;
                    case 'q': castling_Rights |= 8; break;
                    case '-': break;
                }
            }

            if (parts[3] != "-") {
                int file = parts[3][0] - 'a'; // 'a'=0 ... 'h'=7
                int rank = parts[3][1] - '1'; // '1'=0 ... '8'=7
                enpassant_square = rank * 8 + file;
            }

            halfmove_clock = std::stoi(parts[4]);
            initHash();
        }

        std::string toFen() {
            std::string fenstr = "";
            std::stringstream sss;
            for (int r = 0; r < 8; r++) {
                int emptyCounter = 0;
                for (int f = 0; f < 8; f++) {
                    int p = mailbox[indexTable[r * 8 + f]];
                    if (p == 0) { emptyCounter++; }
                    else if (p > 0) {
                        if (emptyCounter > 0) sss << emptyCounter;
                        sss << pieceToChar[p];
                    }
                    else {
                        if (emptyCounter > 0) sss << emptyCounter;
                        sss << pieceToChar[-p + 6];
                    }
                }
                if (emptyCounter > 0) sss << emptyCounter;
                if (r < 7) sss << '/';
            }
            sss << ' ';
            sss << ((side == WHITE) ? 'w' : 'b');
            sss << ' ';
            if (castling_Rights & 1 == 1) sss << 'K';
            if (castling_Rights & 2 == 2) sss << 'Q';
            if (castling_Rights & 4 == 4) sss << 'k';
            if (castling_Rights & 8 == 8) sss << 'q';
            sss << ' ';
            if (enpassant_square != -1) {
                sss << (char)('a' + enpassant_square % 8);
                sss << (char)('1' + enpassant_square / 8);
            }
            else sss << '-';
            sss << ' ';
            sss << halfmove_clock;
            sss << ' ';
            sss << full_move;
            fenstr = sss.str();
            return fenstr;
        }

        void makeMove(Move move) {
            if (HistoryIndex >= 4096) { std::cerr << "History overflow\n"; return; }

            stateHistory[HistoryIndex].enpassant_square = enpassant_square;
            stateHistory[HistoryIndex].halfmove_clock = halfmove_clock;
            stateHistory[HistoryIndex].castlingRights = castling_Rights;
            stateHistory[HistoryIndex].hash = hash;
            moveHistory[HistoryIndex] = move;
            HistoryIndex++;

            int from      = decodeFrom(move);
            int to        = decodeTo(move);
            int piece     = decodePiece(move);
            int captured  = decodeCapture(move);
            int promotion = decodePromotion(move);
            bool enPassantMove = decodeEnPassant(move);
            bool castleMove = decodeCastle(move);

            if (piece == 1 || piece == -1 || captured != 0) { halfmove_clock = 0;}
            else {halfmove_clock++;}

            removePiece(from, piece);
            hash ^= zobristPiece[piece + 6][from];

            if (captured != 0 && !enPassantMove) { removePiece(to, captured); hash ^= zobristPiece[captured + 6][to]; }

            if (enPassantMove) {
                if (piece == 1) { removePiece(to - 8, -1); hash ^= zobristPiece[-1 + 6][to - 8]; }
                else { removePiece(to + 8, 1); hash ^= zobristPiece[1 + 6][to + 8]; }
            }

            if (promotion != 0) { placePiece(to, promotion); hash ^= zobristPiece[promotion + 6][to]; }
            else                { placePiece(to, piece);     hash ^= zobristPiece[piece + 6][to]; };

            if (castleMove) {
                if (piece == 6) {
                    if (to == 6) { removePiece(7, 4);  placePiece(5, 4);        hash ^= zobristPiece[4 + 6][7]; hash ^= zobristPiece[4 + 6][5]; } // kingside
                    else { removePiece(0, 4);  placePiece(3, 4);                hash ^= zobristPiece[4 + 6][0]; hash ^= zobristPiece[4 + 6][3]; } // queenside
                } else {
                    if (to == 62) { removePiece(63, -4); placePiece(61, -4);    hash ^= zobristPiece[-4 + 6][63]; hash ^= zobristPiece[-4 + 6][61]; } // kingside
                    else { removePiece(56, -4); placePiece(59, -4);             hash ^= zobristPiece[-4 + 6][56]; hash ^= zobristPiece[-4 + 6][59]; } // queenside
                }
            }

            hash ^= zobristCastle[castling_Rights];
            castling_Rights &= castling_rights_update[from];
            castling_Rights &= castling_rights_update[to];
            hash ^= zobristCastle[castling_Rights];

            if (enpassant_square != -1) hash ^= zobristEP[enpassant_square % 8];
            enpassant_square = -1;
            if ((piece == 1) && (to - from == 16)) enpassant_square = from + 8;
            else if ((piece == -1) && (from - to == 16)) enpassant_square = from - 8;
            if (enpassant_square != -1) hash ^= zobristEP[enpassant_square % 8];
            if (side == BLACK) full_move++;
            side = (side == WHITE) ? BLACK : WHITE;
            hash ^= zobristSide;
        }

        void unmakeMove() {
            if (HistoryIndex <= 0) { std::cerr << "History underflow\n"; return; }

            HistoryIndex--;
            uint32_t move = moveHistory[HistoryIndex];

            int from      = decodeFrom(move);
            int to        = decodeTo(move);
            int piece     = decodePiece(move);
            int captured  = decodeCapture(move);
            int promotion = decodePromotion(move);

            enpassant_square = stateHistory[HistoryIndex].enpassant_square;
            halfmove_clock   = stateHistory[HistoryIndex].halfmove_clock;
            castling_Rights = stateHistory[HistoryIndex].castlingRights;
            hash = stateHistory[HistoryIndex].hash;

            side = (side == WHITE) ? BLACK : WHITE;
            if (side == BLACK) full_move--;
            if (promotion != 0) {
                removePiece(to, promotion);
                placePiece(from, piece);
            } else {
                removePiece(to, piece);
                placePiece(from, piece);
            }

            if (captured != 0 && !decodeEnPassant(move)) placePiece(to, captured);

            if (decodeEnPassant(move)) {
                if (piece == 1) placePiece(to - 8, -1);
                else            placePiece(to + 8,  1);
            }

            if (decodeCastle(move)) {
                if (piece == 6) {
                    if (to == 6) { removePiece(5, 4);  placePiece(7, 4);  }
                    else { removePiece(3, 4);  placePiece(0, 4);  }
                } else {
                    if (to == 62) { removePiece(61, -4); placePiece(63, -4); }
                    else { removePiece(59, -4); placePiece(56, -4); }
                }
            }
        }

        bool isInCheck(Color side) {
            Bitboard king = pieces(side, KING);
            int sq = __builtin_ctzll(king);
            Color attackerSide = (side == WHITE) ? BLACK : WHITE;
            return isAttackedBy(sq, attackerSide);
        }

        bool isAttackedBy(int sq, Color side) {
            Bitboard occ = byColorBB[WHITE] | byColorBB[BLACK];
            Bitboard attackerPieces = byColorBB[side];
            Bitboard rookAtt = getRookAttacksBB(sq, occ);
            Bitboard bishopAtt = getBishopAttacksBB(sq, occ);
            Bitboard pawnAtt = (side == WHITE) ? pawnAttacksB[sq] : pawnAttacksW[sq];

            Bitboard pawn = pieces(side, PAWN);
            Bitboard knight = pieces(side, KNIGHT);
            Bitboard bishop = pieces(side, BISHOP);
            Bitboard rook = pieces(side, ROOK);
            Bitboard queen = pieces(side, QUEEN);
            Bitboard king = pieces(side, KING);

            if (((rookAtt | bishopAtt) & queen) > 0)    return true;
            else if ((rookAtt & rook) > 0)              return true;
            else if ((bishopAtt & bishop) > 0)          return true;
            else if ((knightAttacks[sq] & knight) > 0)  return true;
            else if ((pawnAtt & pawn) > 0)              return true;
            else if ((kingAttacks[sq] & king) > 0)      return true;
            return false;
        }

        void initHash() {
            hash = 0;
            for (int i = 0; i < 64; i++) {
                int p = mailbox[i];
                if (p != 0) {
                    hash ^= zobristPiece[p + 6][i];
                }
            }
            if (side == BLACK) hash ^= zobristSide;
            hash ^= zobristCastle[castling_Rights];
            if (enpassant_square != -1) hash ^= zobristEP[enpassant_square % 8];
        }

        void initBB() {
            for (int i = 0; i < 64; i++) {
                if (i >= 56) pawnAttacksW[i] = 0;
                else if (i % 8 == 0) pawnAttacksW[i] = 1ULL << (9 + i);
                else if (i % 8 == 7) pawnAttacksW[i] = 1ULL << (7 + i);
                else pawnAttacksW[i] = 1ULL << (9 + i) | 1ULL << (7 + i);
            }

            for (int i = 0; i < 64; i++) {
                if (i <= 7) pawnAttacksB[i] = 0;
                else if (i % 8 == 0) pawnAttacksB[i] = 1ULL << (i - 7);
                else if (i % 8 == 7) pawnAttacksB[i] = 1ULL << (i - 9);
                else pawnAttacksB[i] = (1ULL << (i - 7)) | (1ULL << (i - 9));
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

            /* === TT Table === */
            std::mt19937_64 rng(2147483647ULL);
            for (int p = 0; p < 13; p++) {
                for (int sq = 0; sq < 64; sq++) zobristPiece[p][sq] = rng();
            }
            zobristSide = rng();
            for (int i = 0; i < 16; i++) zobristCastle[i] = rng();
            for (int i = 0; i < 8;  i++) zobristEP[i]     = rng();
        }

        Bitboard getRookAttacksBB(int sq, Bitboard occ) {
            occ &= rookMasks[sq];
            occ *= rookMagics[sq];
            occ >>= rookShifts[sq];
            return rookAttacks[rookOffsets[sq] + occ];
        }

        Bitboard getBishopAttacksBB(int sq, Bitboard occ) {
            occ &= bishopMasks[sq];
            occ *= bishopMagics[sq];
            occ >>= bishopShifts[sq];
            return bishopAttacks[bishopOffsets[sq] + occ];
        }

        Bitboard getCheckers(Color side) {

            int sq = __builtin_ctzll(pieces(side, KING));
            Color enemy = (Color)(side ^ 1);
            Bitboard checkers = 0;

            checkers |= knightAttacks[sq] & pieces(enemy, KNIGHT);
            checkers |= (enemy == BLACK) ? (pawnAttacksW[sq] & pieces(enemy, PAWN)) : (pawnAttacksB[sq] & pieces(enemy, PAWN));
            checkers |= getRookAttacksBB(sq, byColorBB[WHITE] | byColorBB[BLACK]) & (pieces(enemy, ROOK) | pieces(enemy, QUEEN));
            checkers |= getBishopAttacksBB(sq, byColorBB[WHITE] | byColorBB[BLACK]) & (pieces(enemy, BISHOP) | pieces(enemy, QUEEN));
            return checkers;

        }

        Bitboard computeCheckMask(Color side, Bitboard checkers) {
            int sq = __builtin_ctzll(pieces(side, KING));

            if (__builtin_popcountll(checkers) > 1) {
                return 0;
            }

            int checkersSq = __builtin_ctzll(checkers);
            return (1ULL << checkersSq) | betweenTable[sq][checkersSq];
        }

    private:
        void removePiece(int sq, int piece) {
            mailbox[sq] = 0;
            Color side = (piece > 0) ? WHITE : BLACK;
            byTypeBB[abs(piece)] &= ~(1ULL << sq); byColorBB[side] &= ~(1ULL << sq); // piece count code I dont know if this work right now
            pieceCount[side][abs(piece)] -= 1;
        }
        void placePiece(int sq, int piece) {
            Bitboard bit = 1ULL <<  sq;
            Color side = (piece > 0) ? WHITE : BLACK;
            byTypeBB[abs(piece)] |= bit;
            byColorBB[side] |= bit;
            pieceCount[side][abs(piece)] += 1; // piece count code I dont know if this work right now
            mailbox[sq] = piece;

        }

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
};

class moveGen {
    public:
        Board& board;
        moveGen(Board& b) : board(b) {}

        uint64_t perft(int depth) {
            if (depth == 0) return 1;
            Move moves[256]; int count = generateMoves<ALL>(moves); uint64_t nodes = 0;
            for (int i = 0; i < count; i++) {
                board.makeMove(moves[i]);
                Color movedSide = (board.side == WHITE) ? BLACK : WHITE;
                if (!board.isInCheck(movedSide)) { nodes += perft(depth - 1); }
                board.unmakeMove();
            }
            return nodes;
        }

        template<GenType Type>
        int generateMoves(Move* moves) {
            Move* curr = moves;
            int nodes = 0;

            Color side = board.side;
            Bitboard occ = board.occupancy();
            Bitboard ownPieces = board.occupancySide(side);
            Bitboard enemyPieces = (side == WHITE) ? board.occupancySide(BLACK) : board.occupancySide(WHITE);
            Bitboard checkMask = 0ULL;

            if constexpr (Type == EVASIONS) {
                Bitboard checkers = board.getCheckers(side);
                checkMask = board.computeCheckMask(side, checkers);
            }

            curr = generate_pawn_moves<Type>(curr, side, ownPieces, enemyPieces, nodes, checkMask);     //std::cout << "Done with pawn\n";
            curr = generate_knight_moves<Type>(curr, side, ownPieces, enemyPieces, nodes, checkMask);   //std::cout << "Done with knight\n";
            curr = generate_bishop_moves<Type>(curr, side, ownPieces, enemyPieces, nodes, checkMask);   //std::cout << "Done with bishop\n";
            curr = generate_rook_moves<Type>(curr, side, ownPieces, enemyPieces, nodes, checkMask);     //std::cout << "Done with rook\n";
            curr = generate_queen_moves<Type>(curr, side, ownPieces, enemyPieces, nodes, checkMask);    //std::cout << "Done with queen\n";
            curr = generate_king_moves<Type>(curr, side, ownPieces, enemyPieces, nodes, checkMask);     //std::cout << "Done with king\n";
            curr = generate_castle_moves<Type>(curr, side, ownPieces, enemyPieces, nodes);              //std::cout << "Done with castling\n";

            return nodes;
        }

    private:
        template<GenType Type>
        Move* generate_pawn_moves(Move* moves, Color side, Bitboard own, Bitboard enemy, int& nodes, Bitboard checkMask) {
            Bitboard pawn = board.pieces(side, PAWN);
            Bitboard occ = own | enemy;
            int piece = (side == WHITE) ? 1 : -1;

            while (pawn > 0) {
                int from = __builtin_ctzll(pawn);
                pawn &= pawn - 1;
                Bitboard bit = 1ULL << from;

                if (side == WHITE) {
                    if constexpr (Type == QUIETS || Type == ALL) {
                        if (!((occ) & (bit << 8))) {
                            if (from >= 48) {
                                moves[nodes++] = encodeMove(from + 8, from, 1, 0, 5, false, false);
                                moves[nodes++] = encodeMove(from + 8, from, 1, 0, 4, false, false);
                                moves[nodes++] = encodeMove(from + 8, from, 1, 0, 3, false, false);
                                moves[nodes++] = encodeMove(from + 8, from, 1, 0, 2, false, false);
                            } else {
                                moves[nodes++] = encodeMove(from + 8, from, 1, 0, 0, false, false);
                            }
                            if (from >= 8 && from <= 15 && !(occ & (bit << 16))) { moves[nodes++] = encodeMove(from + 16, from, 1, 0, 0, false, false); }
                        }
                    }
                    if constexpr (Type == CAPTURES || Type == ALL) {
                        Bitboard captures = pawnAttacksW[from] & enemy;
                        while (captures) {
                            int to = __builtin_ctzll(captures);
                            captures &= captures - 1;
                            if (from >= 48) {
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 5, false, false);
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 4, false, false);
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 3, false, false);
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 2, false, false);
                            } else {
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 0, false, false);
                            }
                        }
                        if (board.enpassant_square != -1 && (pawnAttacksW[from] & (1ULL << board.enpassant_square))) {
                            moves[nodes++] = encodeMove(board.enpassant_square, from, 1, -1, 0, true, false);
                        }
                    }
                    if constexpr (Type == EVASIONS) {
                        if (!((occ) & (bit << 8))) {
                            int to = from + 8;
                            if (checkMask & (1ULL << to)) {
                                if (from >= 48) {
                                    moves[nodes++] = encodeMove(to, from, 1, 0, 5, false, false);
                                    moves[nodes++] = encodeMove(to, from, 1, 0, 4, false, false);
                                    moves[nodes++] = encodeMove(to, from, 1, 0, 3, false, false);
                                    moves[nodes++] = encodeMove(to, from, 1, 0, 2, false, false);
                                } else {
                                    moves[nodes++] = encodeMove(to, from, 1, 0, 0, false, false);
                                }
                            }
                            if (from >= 8 && from <= 15 && !(occ & (bit << 16))) {
                                int toDouble = from + 16;
                                if (checkMask & (1ULL << toDouble)) { moves[nodes++] = encodeMove(toDouble, from, 1, 0, 0, false, false); }
                            }
                        }
                        Bitboard captures = pawnAttacksW[from] & enemy & checkMask;
                        while (captures) {
                            int to = __builtin_ctzll(captures);
                            captures &= captures - 1;
                            if (from >= 48) {
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 5, false, false);
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 4, false, false);
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 3, false, false);
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 2, false, false);
                            } else {
                                moves[nodes++] = encodeMove(to, from, 1, board.getPieceAt(to), 0, false, false);
                            }
                        }
                        // en passant evasion - only if the captured pawn is the checker
                        if (board.enpassant_square != -1 && (pawnAttacksW[from] & (1ULL << board.enpassant_square))) {
                            int capturedPawnSq = board.enpassant_square - 8;
                            if (checkMask & (1ULL << capturedPawnSq)) {
                                moves[nodes++] = encodeMove(board.enpassant_square, from, 1, -1, 0, true, false);
                            }
                        }
                    }
                }
                else if (side == BLACK){
                    if constexpr (Type == QUIETS || Type == ALL) {
                        if (!(occ & (bit >> 8))) {
                            if (from >= 8 && from <= 15) {
                                moves[nodes++] = encodeMove(from - 8, from, -1, 0, -5, false, false);
                                moves[nodes++] = encodeMove(from - 8, from, -1, 0, -4, false, false);
                                moves[nodes++] = encodeMove(from - 8, from, -1, 0, -3, false, false);
                                moves[nodes++] = encodeMove(from - 8, from, -1, 0, -2, false, false);
                            } else {
                                moves[nodes++] = encodeMove(from - 8, from, -1, 0, 0, false, false);
                            }
                            if (from >= 48 && from <= 55 && !(occ & (bit >> 16))) { moves[nodes++] = encodeMove(from - 16, from, -1, 0, 0, false, false); }
                        }
                    }
                    if constexpr (Type == CAPTURES || Type == ALL) {
                        Bitboard captures = pawnAttacksB[from] & enemy;
                        while (captures) {
                            int to = __builtin_ctzll(captures);
                            captures &= captures - 1;
                            if (from >= 8 && from <= 15) {
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -5, false, false);
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -4, false, false);
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -3, false, false);
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -2, false, false);
                            } else {
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), 0, false, false);
                            }
                        }
                        if (board.enpassant_square != -1 && (pawnAttacksB[from] & (1ULL << board.enpassant_square))) {
                            moves[nodes++] = encodeMove(board.enpassant_square, from, -1, 1, 0, true, false);
                        }
                    }
                    if constexpr (Type == EVASIONS) {
                        if (!(occ & (bit >> 8))) {
                            int to = from - 8;
                            if (checkMask & (1ULL << to)) {
                                if (from >= 8 && from <= 15) {
                                    moves[nodes++] = encodeMove(to, from, -1, 0, -5, false, false);
                                    moves[nodes++] = encodeMove(to, from, -1, 0, -4, false, false);
                                    moves[nodes++] = encodeMove(to, from, -1, 0, -3, false, false);
                                    moves[nodes++] = encodeMove(to, from, -1, 0, -2, false, false);
                                } else {
                                    moves[nodes++] = encodeMove(to, from, -1, 0, 0, false, false);
                                }
                            }
                            if (from >= 48 && from <= 55 && !(occ & (bit >> 16))) {
                                int toDouble = from - 16;
                                if (checkMask & (1ULL << toDouble)) { moves[nodes++] = encodeMove(toDouble, from, -1, 0, 0, false, false); }
                            }
                        }
                        Bitboard captures = pawnAttacksB[from] & enemy & checkMask;
                        while (captures) {
                            int to = __builtin_ctzll(captures);
                            captures &= captures - 1;
                            if (from >= 8 && from <= 15) {
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -5, false, false);
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -4, false, false);
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -3, false, false);
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), -2, false, false);
                            } else {
                                moves[nodes++] = encodeMove(to, from, -1, board.getPieceAt(to), 0, false, false);
                            }
                        }
                        // en passant evasion - only if the captured pawn is the checker
                        if (board.enpassant_square != -1 && (pawnAttacksB[from] & (1ULL << board.enpassant_square))) {
                            int capturedPawnSq = board.enpassant_square + 8;
                            if (checkMask & (1ULL << capturedPawnSq)) {
                                moves[nodes++] = encodeMove(board.enpassant_square, from, -1, 1, 0, true, false);
                            }
                        }
                    }
                }
            }
            return moves;
        }

        template<GenType Type>
        Move* generate_knight_moves(Move* moves, Color side, Bitboard own, Bitboard enemy, int& nodes, Bitboard checkMask) {
            Bitboard knight = board.pieces(side, KNIGHT);
            int piece = (side == WHITE) ? 2 : -2;

            while (knight) {
                int from = __builtin_ctzll(knight);
                knight &= knight - 1;
                Bitboard attacks = (knightAttacks[from]) & ~own;

                if constexpr (Type == CAPTURES) attacks &= enemy;
                else if constexpr (Type == QUIETS) attacks &= ~enemy;
                else if constexpr (Type == EVASIONS) attacks &= checkMask;

                while (attacks) {
                    int to = __builtin_ctzll(attacks);
                    attacks &= attacks - 1;
                    Move move = encodeMove(to, from, piece, board.getPieceAt(to), 0, false, false);
                    moves[nodes++] = move;
                }
            }
            return moves;
        }

        template<GenType Type>
        Move* generate_bishop_moves(Move* moves, Color side, Bitboard own, Bitboard enemy, int& nodes, Bitboard checkMask) {
            Bitboard bishop = board.pieces(side, BISHOP);
            Bitboard occ = own | enemy;
            int piece = (side == WHITE) ? 3 : -3;

            while (bishop) {
                int from = __builtin_ctzll(bishop);
                bishop &= bishop - 1;
                Bitboard attacks = (board.getBishopAttacksBB(from, occ)) & ~own;

                if constexpr (Type == CAPTURES) attacks &= enemy;
                else if constexpr (Type == QUIETS) attacks &= ~enemy;
                else if constexpr (Type == EVASIONS) attacks &= checkMask;

                while (attacks) {
                    int to = __builtin_ctzll(attacks);
                    attacks &= attacks - 1;
                    Move move = encodeMove(to, from, piece, board.getPieceAt(to), 0, false, false);
                    moves[nodes++] = move;
                }
            }
            return moves;
        }

        template<GenType Type>
        Move* generate_rook_moves(Move* moves, Color side, Bitboard own, Bitboard enemy, int& nodes, Bitboard checkMask) {
            Bitboard rook = board.pieces(side, ROOK);
            Bitboard occ = own | enemy;
            int piece = (side == WHITE) ? 4 : -4;

            while (rook) {
                int from = __builtin_ctzll(rook);
                rook &= rook - 1;
                Bitboard attacks = (board.getRookAttacksBB(from, occ)) & ~own;

                if constexpr (Type == CAPTURES) attacks &= enemy;
                else if constexpr (Type == QUIETS) attacks &= ~enemy;
                else if constexpr (Type == EVASIONS) attacks &= checkMask;

                while (attacks) {
                    int to = __builtin_ctzll(attacks);
                    attacks &= attacks - 1;
                    Move move = encodeMove(to, from, piece, board.getPieceAt(to), 0, false, false);
                    moves[nodes++] = move;
                }
            }
            return moves;
        }

        template<GenType Type>
        Move* generate_queen_moves(Move* moves, Color side, Bitboard own, Bitboard enemy, int& nodes, Bitboard checkMask) {
            Bitboard queen = board.pieces(side, QUEEN);
            Bitboard occ = own | enemy;
            int piece = (side == WHITE) ? 5 : -5;

            while (queen) {
                int from = __builtin_ctzll(queen);
                queen &= queen - 1;
                Bitboard attacks = (board.getRookAttacksBB(from, occ) | board.getBishopAttacksBB(from, occ)) & ~own;

                if constexpr (Type == CAPTURES) attacks &= enemy;
                else if constexpr (Type == QUIETS) attacks &= ~enemy;
                else if constexpr (Type == EVASIONS) attacks &= checkMask;

                while (attacks) {
                    int to = __builtin_ctzll(attacks);
                    attacks &= attacks - 1;
                    Move move = encodeMove(to, from, piece, board.getPieceAt(to), 0, false, false);
                    moves[nodes++] = move;
                }
            }
            return moves;
        }

        template<GenType Type>
        Move* generate_king_moves(Move* moves, Color side, Bitboard own, Bitboard enemy, int& nodes, Bitboard checkMask) {
            Bitboard king = board.pieces(side, KING);
            int piece = (side == WHITE) ? 6 : -6;

            while (king) {
                int from = __builtin_ctzll(king);
                king &= king - 1;
                Bitboard attacks = (kingAttacks[from]) & ~own;

                if constexpr (Type == CAPTURES) attacks &= enemy;
                else if constexpr (Type == QUIETS) attacks &= ~enemy;

                while (attacks) {
                    int to = __builtin_ctzll(attacks);
                    attacks &= attacks - 1;
                    Move move = encodeMove(to, from, piece, board.getPieceAt(to), 0, false, false);
                    moves[nodes++] = move;
                }
            }
            return moves;
        }

        template<GenType Type>
        Move* generate_castle_moves(Move* moves, Color side, Bitboard own, Bitboard enemy, int& nodes) {
            Bitboard occ = own | enemy;
            if (side == WHITE) {
                if (board.getCastleR(1) && !(occ & (1ULL << 5)) && !(occ & (1ULL << 6)) && !board.isAttackedBy(4, BLACK) && !board.isAttackedBy(5, BLACK) && !board.isAttackedBy(6, BLACK))
                    if constexpr (Type == QUIETS || Type == ALL) { moves[nodes++] = encodeMove(6, 4, 6, 0, 0, false, true); }

                if (board.getCastleR(2) && !(occ & (1ULL << 3)) && !(occ & (1ULL << 2)) && !(occ & (1ULL << 1)) && !board.isAttackedBy(4, BLACK) && !board.isAttackedBy(3, BLACK) && !board.isAttackedBy(2, BLACK))
                    if constexpr (Type == QUIETS || Type == ALL) { moves[nodes++] = encodeMove(2, 4, 6, 0, 0, false, true); }
            }
            else {
                if (board.getCastleR(3) && !(occ & (1ULL << 61)) && !(occ & (1ULL << 62)) && !board.isAttackedBy(60, WHITE) && !board.isAttackedBy(61, WHITE) && !board.isAttackedBy(62, WHITE))
                    if constexpr (Type == QUIETS || Type == ALL) { moves[nodes++] = encodeMove(62, 60, -6, 0, 0, false, true); }

                if (board.getCastleR(4) && !(occ & (1ULL << 59)) && !(occ & (1ULL << 58)) && !(occ & (1ULL << 57)) && !board.isAttackedBy(60, WHITE) && !board.isAttackedBy(59, WHITE) && !board.isAttackedBy(58, WHITE))
                    if constexpr (Type == QUIETS || Type == ALL) { moves[nodes++] = encodeMove(58, 60, -6, 0, 0, false, true); }
            }
            return moves;

        }
};


class Eval {
    public:
        Board& board;
        Eval(Board& b) : board(b) {}

        int evaluate() {
            int score = 0;

            score += board.pieceCount[WHITE][PAWN] * 100;
            score += board.pieceCount[WHITE][KNIGHT] * 320;
            score += board.pieceCount[WHITE][BISHOP] * 330;
            score += board.pieceCount[WHITE][ROOK] * 500;
            score += board.pieceCount[WHITE][QUEEN] * 900;

            score -= board.pieceCount[BLACK][PAWN] * 100;
            score -= board.pieceCount[BLACK][KNIGHT] * 320;
            score -= board.pieceCount[BLACK][BISHOP] * 330;
            score -= board.pieceCount[BLACK][ROOK] * 500;
            score -= board.pieceCount[BLACK][QUEEN] * 900;

            return (board.side == WHITE) ? score : -score;
        }
};

class Search {
    public:
        Board& board;
        moveGen& mg;
        Eval& eval;
        Search(Board& b, moveGen& m, Eval& e) : board(b), mg(m), eval(e) {}

        const static int INF = 3000000;

        Move bestMove = 0;
        int rootDepth = 0;
        uint64_t nodes = 0;

        int search(int depth, int alpha, int beta) {
            uint64_t hash = board.hash;
            TTEntry& entry = probe(hash);
            Move ttMove = 0;

            if (entry.key == hash) {
                ttMove = entry.move;
                if (entry.depth >= depth) {
                    if (entry.bound == EXACT)                         return entry.score;
                    if (entry.bound == LOWER && entry.score >= beta)  return entry.score;
                    if (entry.bound == UPPER && entry.score <= alpha) return entry.score;
                }
            }

            if (depth == 0) return eval.evaluate();
            int bestValue = -INF;
            Bound bound = UPPER;
            Move localBestMove = 0;

            Move moves[256];
            int count = mg.generateMoves<ALL>(moves);

            for (int i = 0; i < count; i++) {
                if (moves[i] == ttMove) {
                    std::swap(moves[0], moves[i]);
                    break;
                }
            }

            bool hasLegal = false;
            for (int i = 0; i < count; i++) {
                nodes++;

                board.makeMove(moves[i]);
                if (board.isInCheck(board.side == WHITE ? BLACK : WHITE)) { board.unmakeMove(); continue; }

                hasLegal = true;
                int score = -search((depth - 1), -beta, -alpha);
                board.unmakeMove();
                if (score > bestValue) {
                    bestValue = score;
                    localBestMove = moves[i];
                    if (depth == rootDepth) bestMove = moves[i];
                }
                if (score > alpha) { alpha = score; bound  = EXACT; }
                if (score >= beta) { bound = LOWER; break; }
            }

            if (!hasLegal) {return board.isInCheck(board.side) ? -(10000 - depth) : 0;}

            entry.key   = hash;
            entry.score = bestValue;
            entry.depth = depth;
            entry.move  = localBestMove;
            entry.bound = bound;

            return bestValue;
        }

        Move getBestMove(int depth) {
            bestMove = 0;
            Move lastBest = 0;
            nodes = 0;
            for (int d = 1; d <= depth; d++) {
                rootDepth = d;
                int score = search(d, -INF, INF);
                lastBest = bestMove;
                std::cout << "Nodes: " << nodes << " depth: " << d << " best: " << board.decodeMoveInt(lastBest) << std::endl;
            }

            return bestMove;
        }
};


/* ==== Main ====*/
int main() {
    std::cout << THEME << "Fih³ chess engine \n";
    Board board;
    board.initBB();
    moveGen mg(board);
    board.loadFEN(defaultBoard);
    Eval eval(board);
    Search search(board, mg, eval);

    board.printBoard();

    int depth = 9;
    auto start = std::chrono::high_resolution_clock::now();
    Move move = search.getBestMove(depth);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double seconds = elapsed.count();
    double nps = (seconds > 0) ? (search.nodes / seconds) : 0;

    std::cout << "---------------------------------" << std::endl;
    std::cout << "Depth:    " << depth << std::endl;
    std::cout << "Nodes:    " << search.nodes << std::endl;
    std::cout << "Time:     " << seconds << " s" << std::endl;
    std::cout << "NPS:      " << static_cast<uint64_t>(nps) << std::endl;
    std::cout << "Move:     " << board.decodeMoveInt(move) << std::endl;
    std::cout << "---------------------------------" << std::endl;

    return 0;
}