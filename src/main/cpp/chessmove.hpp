#include "json.hpp"
#include <string>

#ifndef __CHESS_MOVE_H__
#define __CHESS_MOVE_H__
//using json = nlohmann::json;
using namespace nlohmann;

using namespace std;


/**
 *    a  b  c  d  e  f  g  h
 * 8  00 01 02 03 04 05 06 07  8
 * 7  08 09 10 11 12 13 14 15  7
 * 6  16 17 18 19 20 21 22 23  6
 * 5  24 25 26 27 28 29 30 31  5
 * 4  32 33 34 35 36 37 38 39  4
 * 3  40 41 42 43 44 45 46 47  3
 * 2  48 49 50 51 52 53 54 55  2
 * 1  56 57 58 59 60 61 62 63  1
 *    a  b  c  d  e  f  g  h
 */
class ChessMove {
public:
    string m_type; //move, capture, takeback
    string m_from;
    string m_to;
    string m_description;

    char buff[80];
    const char* rowNames="87654321";
    const char* colNames="ABCDEFGH";

    int fromIndex() {
        return squaresIndex(m_from);
    }
    int toIndex() {
        return squaresIndex(m_to);
    }

    int squaresIndex(string square) {
        return (8*('8'-square[1]))+(square[0]-'A');
    }

    char toRow(int index) {
        int y = index/8;
        int x = index-y*8;
        return rowNames[y];
    }
    char toCol(int index) {
        int y = index/8;
        int x = index-y*8;
        return colNames[x];
    }

    string str_toupper(string s) {
        transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c){ return std::toupper(c); } // correct
        );
        return s;
    }

    ChessMove() {}
    ChessMove(json j) {
        m_from = str_toupper(j["m_from"]);
        m_to = str_toupper(j["to"]);
        m_type = j["m_type"];
    }

    char* c_str() {
        snprintf(buff, sizeof(buff), "%s%s m_type=%s", m_from.c_str(), m_to.c_str(), m_type.c_str());
        return buff;
    }
};

#endif 