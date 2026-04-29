#pragma once

#include <cstdint>
#include <array>
#include <string>

using Bitboard = uint64_t;
using Square = int;
using Move = uint32_t;
using Rank = int;

enum Color : int { WHITE, BLACK };
enum Piece : int { PAWN = 1, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum File  : int { A, B, C, D, E, F, G, H };
enum GenType { ALL, CAPTURES, QUIETS, EVASIONS };
enum Bound : uint8_t { UPPER, LOWER, EXACT };

constexpr int INF        =  30000;
constexpr int MATE_SCORE =  29000;
constexpr int DRAW_SCORE =  0;

constexpr std::array<int, 6> PIECE_VALUES = { 100, 320, 330, 500, 900, 0 };

constexpr File fileOf(Square sq) { return File(sq % 8); }
constexpr Rank rankOf(Square sq) { return Rank(sq / 8); }
constexpr Square makeSquare(File f, Rank r) { return r * 8 + f; }
constexpr Color  flipColor(Color c) { return Color(c ^ 1); }
constexpr int PIECE_OFFSET = 7;

enum Squares : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
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
