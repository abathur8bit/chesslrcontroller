//compile with: gcc -Wall -o mcptest mcptest.c -lwiringPi
//g++ -o controller -std=c++11 -I../ssobjects -I src/main/cpp -lwiringPi -lssobjects src/main/cpp/controller.cpp
//pin numbers are BCM

/** LEDs are on bank B, reed switches are on bank A.
 *
 * Pin layout:
 *
 * ```
 *                   +----- -----+
 * LED 7 - GPB0 <--> |1    -   28| <--> GPA7 - Reed switch 7
 * LED 6 - GPB1 <--> |2        27| <--> GPA6 - Reed switch 6
 * LED 5 - GPB2 <--> |3        26| <--> GPA5 - Reed switch 5
 * LED 4 - GPB3 <--> |4    M   25| <--> GPA4 - Reed switch 4
 * LED 3 - GPB4 <--> |5    C   24| <--> GPA3 - Reed switch 3
 * LED 2 - GPB5 <--> |6    P   23| <--> GPA2 - Reed switch 2
 * LED 1 - GPB6 <--> |7    2   22| <--> GPA1 - Reed switch 1
 * LED 0 - GPB7 <--> |8    3   21| <--> GPA0 - Reed switch 0
 *          VDD ---> |9    0   20| ---> INTA
 *          VSS ---> |10   1   19| ---> INTB
 *           NC ---- |11   7   18| ---> RESET
 *          SCL ---> |12       17| <--- A2
 *          SDA <--> |13       16| <--- A1
 *           NC ---- |14       15| <--- A0
 *                   +-----------+
 * ```
 *
 * $ sudo i2cdetect -y 1
 *
 *       0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
 *  00:          -- -- -- -- -- -- -- -- -- -- -- -- --
 *  10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 *  20: 20 21 22 23 24 25 26 27 -- -- -- -- -- -- -- --
 *deci> 32 33 34 35 36 37 38 39 <deci
 *  30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 *  40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 *  50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 *  60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
 *  70: -- -- -- -- -- -- -- --
 */

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <mcp23017.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <error.h>
#include <ssobjects/ssobjects.h>
#include <ssobjects/simpleserver.h>
#include <ssobjects/tsleep.h>
#include <ssobjects/telnetserver.h>
#include <ssobjects/telnetserversocket.h>

#include "json.hpp"
#include "thc.h"
#include "chessmove.hpp"
#include "chessaction.hpp"

#define VERSION "1.0"

#define WAIT_SLOW 500
#define WAIT_FAST 100
#define BASE_I2C 200
#define BASE_I2C_INPUT  BASE_I2C     //first 8 pins (bank A)
#define BASE_I2C_OUTPUT BASE_I2C+8   //last 8 pins (bank B)

using namespace std;
using namespace nlohmann;   //trying this


#define MOVE_UP 'U'
#define MOVE_DOWN 'D'
#define MOVE_NONE '_'

class BoardRules : public thc::ChessRules {
public:
    char pieceAt(int i) {
        return squares[i];
    }
};
class ControllerServer : public TelnetServer {
public:
    enum {MODE_SETUP,MODE_INSPECT,MODE_PLAY,MODE_MOVE};

    BoardRules cr;
    int gameMode;
    ChessMove waitMove;     ///< The move the board is waiting for the player to complete.
    char moveType[4]= {'_','_','_','_'};
    int moveSquareIndex[4]={-1,-1,-1,-1};
    int moveIndex=-1;
    int movesNeeded=0;
    int moveFrom;
    int moveTo;
    bool moveCaptures;
    enum {FREQ=10};
    int devId;
    int index;
    int baseInput;
    int baseOutput;
    int input[64];          ///< Pin map for input. You use in your digitalRead calls like **digitalRead(input[squareIndex])**.
    int output[64];         ///< Pin map for output. You use in your digitalWrite calls digitalWrite(output[squareIndex],state);. 1=on, 0=off.
    int squareState[64];    ///< What the board is currently seeing. When a piece is lifted or dropped, this gets updated. 0=empty, 1=occupied
    int ledState[64];       ///< What the LEDs are displaying. If you change this, it will immediately change what is displayed.
    int board[64];          ///< What the board should look like. If this and square state are different, then we are in the process of moving/capturing a piece.
    const char* rowNames="87654321";
    const char* colNames="abcdefgh";
    int mcp[8];

    ControllerServer(SockAddr& saBind,bool swap) : TelnetServer(saBind,FREQ){
        devId=0x20;
        index=0;
        baseInput=250-64*2;
        baseOutput=baseInput+8;
        memset(squareState,0,sizeof(squareState));
        gameMode = MODE_PLAY;

        wiringPiSetup();

        for(int row = 0; row<8; ++row) {
            mcp[row]=mcp23017Setup(baseInput,devId+row);
            if(mcp[row]<0)
                perror("wiringPiI2CSetup");

            for(int col=0; col<8; col++) {
                int inputCol = col;
                int outputCol = 7-col;
                pinMode(baseInput+inputCol,INPUT);
                pullUpDnControl(baseInput+inputCol,PUD_UP);
                pinMode(baseOutput+outputCol,OUTPUT);

                input[index]=baseInput+inputCol;
                output[index]=baseOutput+outputCol;

                ++index;
            }
            baseInput+=16;
            baseOutput+=16;
        }

        printf("Turning off led's...\n");
        clearLeds();
        if(swap) {
            // I messed up my wiring, so this fixes it
            int i=8*4+3;
            int t=input[i];
            input[i]=input[i+1];
            input[i+1]=t;
        }
        for(int i=0; i<64; i++) {
            squareState[i] = readState(i); //0=empty 1=occupied
        }

        const char* fen = "8/8/8/8/8/8/8/K6k w - - 0 1";
        cr.Forsyth(fen);
        display_position(cr);
        for(int i=0; i<64; i++) {
            if(cr.pieceAt(i) == ' ')
                led(i,0);
            else
                led(i,1);
        }
        printf("setup board");
        getchar();
        clearLeds();


//        int index=0;
//        printf("index %d is %c%c\n",index,toCol(index),toRow(index));
//        index=63;
//        printf("index %d is %c%c\n",index,toCol(index),toRow(index));
//
//        char square[3]="A1";
//        printf("index=%d %s\n",toIndex(square),square);
//        strcpy(square,"A8");
//        printf("index=%d %s\n",toIndex(square),square);
//        strcpy(square,"H1");
//        printf("index=%d %s\n",toIndex(square),square);
//        strcpy(square,"H8");
//        printf("index=%d %s\n",toIndex(square),square);
    }

    void processSingleMsg(PacketMessage* pmsg)
    {
        TelnetServerSocket* psocket = (TelnetServerSocket*)pmsg->socket();
        PacketBuffer* ppacket = pmsg->packet();
        switch(ppacket->getCmd())
        {
            //One way to handle the message. Process and reply within the switch.
            case PacketBuffer::pcNewConnection:   onConnection(pmsg);               break;
            case PacketBuffer::pcClosed:          printf("Connection closed.\n");   break;
            case TelnetServerSocket::pcFullLine:  onFullLine(pmsg);                 break;
        }
        DELETE_NULL(ppacket);   //IMPORTANT! The packet is no longer needed. You must delete it.
    }

    ChessAction* parseJson(const char* s) {
        json j = json::parse(s);
        printf("parseJson creating chess action\n");
        ChessAction* c = new ChessAction(j);
        return c;
    }

    void onFullLine(PacketMessage* pmsg)
    {
        TelnetServerSocket* psocket = (TelnetServerSocket*)pmsg->socket();
        char* pszString = (char*)pmsg->packet()->getBuffer();
        printf("Got from client: [%s]\n",pszString);

        try {
            json j = json::parse(pszString);
            json jresult;
            jresult["success"]=false;
            jresult["code"]=nullptr;
            jresult["message"]=nullptr;
            string action = j["action"];
            printf("parsed and have action = %s\n",action.c_str());
            if(!action.compare("move")) {
                doMove(psocket,pszString);
            } else if(!action.compare("ping")) {
                psocket->println("pong");
            } else if(!action.compare("setmode")) {
                setMode(j, jresult);
            } else if(!action.compare("led")) {
                setLed(j,jresult);
            }
            psocket->println(jresult.dump().c_str());

        } catch(json::parse_error& e) {
            psocket->println("json parse error: %s",e.what());
        }
    }

    /** Turn on the specified LED. Example:
     * echo '{"action":"led","square":"a2"}' | nc -C -N localhost 9999 */
    void setLed(json& j,json& jresult) {
        jresult["success"] = true;
        string square = j["square"];
        int index = toIndex(square.c_str());
        printf("square=%s index=%d\n",square.c_str(),index);
        led(index,!ledState[index]);
    }

    void setMode(json& j,json& jresult) {
        jresult["success"] = true;     //assume okay
        if(j.count("mode")==1) {
            string mode = j["mode"];
            if(!mode.compare("play")) {
                gameMode = MODE_PLAY;
                jresult["message"] = "game mode set to MODE_PLAY";
                clearLeds();
            } else if(!mode.compare("setup")) {
                gameMode = MODE_SETUP;
                jresult["message"] = "game mode set to MODE_SETUP";
                clearLeds();
            } else if(!mode.compare("inspect")) {
                gameMode = MODE_INSPECT;
                jresult["message"] = "game mode set to MODE_INSPECT";
                clearLeds();
            } else {
                jresult["message"] = "invalid mode";
                jresult["success"] = false;
            }
        } else {
            jresult["message"] = "mode must be specified";
            jresult["success"] = false;
        }
    }

    void clearLeds() {
        for(int i=0; i<64; i++) {
            ledState[i] = 0;
            led(i,0);
        }
    }

    void doMove(TelnetServerSocket* psocket,const char* pszString) {
        ChessAction *ca = parseJson(pszString);
        int index=0;
        waitMove.setFrom(ca->move(index).fromIndex());
        waitMove.setTo(ca->move(index).toIndex());
        waitMove.setType(ca->move(index).type());
        ledState[ca->move(index).fromIndex()] = 1;
        ledState[ca->move(index).toIndex()] = 1;
        gameMode = MODE_MOVE;
        moveIndex = 0;
        if(!strcmp(ca->move(index).type(),"capture")) {
            moveType[0] = MOVE_UP;
            moveType[1] = MOVE_UP;
            moveType[2] = MOVE_DOWN;
            moveSquareIndex[0] = ca->move(index).fromIndex();
            moveSquareIndex[1] = ca->move(index).toIndex();
            moveSquareIndex[2] = ca->move(index).toIndex();
            movesNeeded = 3;
        } else {
            moveType[0] = MOVE_UP;
            moveType[1] = MOVE_DOWN;
            moveSquareIndex[0] = ca->move(index).fromIndex();
            moveSquareIndex[1] = ca->move(index).toIndex();
            movesNeeded = 2;
        }

        for(int i=0; i<4; i++) {
            printf("type[%d]=%c square index=%d\n",i,moveType[i],moveSquareIndex[i]);
        }

        delete ca;
    }

    void onConnection(PacketMessage* pmsg)
    {
        TelnetServerSocket* psocket = (TelnetServerSocket*)pmsg->socket();
        psocket->println("Hello");
    }

    char* toMove(char* buffer, size_t n, char col, char row) {
        snprintf(buffer, n, "%c%c", col, row);
        return buffer;
    }
    char* toMove(char* buffer,size_t n,int index) {
        snprintf(buffer,n,"%c%c",toCol(index),toRow(index));
        return buffer;
    }

    /** Return the letter character of the column the index points to. The column should display before the row. */
    char toCol(int index) {
        int y = index/8;
        int x = index-y*8;
        return colNames[x];
    }

    /** Returns the number character of the row the index points to. You should use the column, then row. */
    char toRow(int index) {
        int y = index/8;
        int x = index-y*8;
        return rowNames[y];
    }

    /** Convert a string like "A1" into an index like 56, or "A8" to 0. Top left corner is 0, bottom right is 63. */
    int toIndex(const char* square) {
        int col=tolower(square[0])-'a';
        int row=8-(square[1]-'0');
//        printf("%s col=%d row=%d\n",square,col,row);
        return row*8+col;
    }

    int toIndex(char col,char row) {
        char buffer[5];
        toMove(buffer,sizeof(buffer),col,row);
        return toIndex(buffer);
    }

    //called every FREQ milliseconds
    void idle(unsigned32 now) {
        char buffer[1024];
        switch(gameMode) {
            case MODE_INSPECT: idleShowPieces(); break;
            case MODE_PLAY: idlePlay(); break;
            case MODE_MOVE: idleMove(); break;
        }
    }

    void idleShowPieces() {
        char buffer[1024];
        for (int i = 0; i < 64; i++) {
            int state = readState(i);
            if (state != squareState[i]) {
                snprintf(buffer, sizeof(buffer), "%c%c %s\r\n", toCol(i), toRow(i), (state ? "pieceDown" : "pieceUp"));
                printf(buffer);
                send2All(buffer);
            }
            squareState[i] = state;
            led(i, state);
        }
    }

    void showValidSquares(int fromIndex) {
        char bufFrom[5],bufTo[5];
        snprintf(bufFrom, sizeof(bufFrom), "%c%c", toCol(fromIndex), toRow(fromIndex));
//        printf("Move from %s\n",bufFrom);

        std::vector<thc::Move> moves;
        std::vector<bool> check;
        std::vector<bool> mate;
        std::vector<bool> stalemate;
        cr.GenLegalMoveList(moves, check, mate, stalemate);
        unsigned int len = moves.size();
        clearLeds();
        led(fromIndex,1);
        for(int i=0; i<len; i++) {
            thc::Move mv = moves[i];
            std::string mv_txt = mv.TerseOut();
//            printf("checking move %s > %s\n",mv_txt.c_str(),bufFrom);
            if(mv_txt[0] == bufFrom[0] && mv_txt[1] == bufFrom[1]) {
                int to=toIndex(mv_txt[2],mv_txt[3]);
                led(to,1);
            }
        }
    }

    /** To long algebraic notation like "a2a3". */
    char* toLAN(char* dest,size_t n,int from,int to) {
        char bfrom[5];
        char bto[5];
        char bmove[5];
        toMove(bfrom,sizeof(bfrom),from);
        toMove(bto,sizeof(bto),to);
        snprintf(dest,n,"%s%s",bfrom,bto);
        return dest;
    }

    void display_position(thc::ChessRules& cr)
    {
        std::string fen = cr.ForsythPublish();
        std::string s = cr.ToDebugStr();
        printf("FEN = %s\n", fen.c_str());
        printf("Position = %s\n", s.c_str());
    }

    void finishMove(int toIndex) {
        char buffer[80];
        toLAN(buffer,sizeof(buffer),moveSquareIndex[0],toIndex);
        printf("full move is %s\n",buffer);
        thc::Move mv;
        mv.TerseIn(&cr,buffer);
        cr.PlayMove(mv);
        display_position(cr);
        moveIndex=-1;
        clearLeds();
    }

    void idlePlay() {
        char buffer[1024];
        for (int i = 0; i < 64; i++) {
            int state = readState(i);
            if (state != squareState[i]) {
                snprintf(buffer, sizeof(buffer), "%c%c", toCol(i), toRow(i));
                json j;
                j["action"] = state ? "pieceDown" : "pieceUp";
                j["square"] = buffer;
                printf("%s\n",j.dump().c_str());
                send2All(j.dump().c_str());
                send2All("\r\n");

                if(!state && -1==moveIndex) {
                    moveType[0] = MOVE_UP;
                    moveSquareIndex[0] = i;
                    moveIndex=1;
                    showValidSquares(i);
                } else {
                    //piece down
                    finishMove(i);
                }
            }
            squareState[i] = state;
        }
    }

    void idleMove() {
        for (int i = 0; i < 64 && gameMode==MODE_MOVE; i++) {
            int state = readState(i);
            if (state != squareState[i]) {
                char type = state ? MOVE_DOWN:MOVE_UP;
                if(moveType[moveIndex] == type && moveSquareIndex[moveIndex] == i) {
                    printf("got move %d %c\n",moveIndex,moveType[moveIndex]);
                    bool resetLed = (moveIndex==1 && type==MOVE_UP) ? false:true;
                    if(resetLed)
                        ledState[i] = 0;
                    moveIndex++;
                    if(moveIndex == movesNeeded) {
                        gameMode = MODE_PLAY;
                        printf("Move finished %s\n",waitMove.tojson().dump().c_str());
                        send2All(waitMove.tojson().dump().c_str());
                        send2All("\r\n");
                    }
                } else {
                    char buffer[1024];
                    snprintf(buffer, sizeof(buffer), "%c%c", toCol(i), toRow(i));
                    json j;
                    j["action"] = state ? "pieceDown" : "pieceUp";
                    j["square"] = buffer;
                    printf("%s\n",j.dump().c_str());
                    send2All(j.dump().c_str());
                    send2All("\r\n");
                }
            }
            squareState[i] = state;
            led(i,ledState[i]);
        }
    }

    /**
     * Checks if a piece is detected on the given square.
     * @param index Square to check.
     * @return 0 if empty, 1 if a piece is detected.
     */
    int readState(int index) {
        return digitalRead(input[index])==0 ? 1:0;
    }

    // Return true if the square is occupied, false otherwise.
    bool isSquareOccupied(int index) {
        return squareState[index]==1;   //0=occupied, 1=not
    }

    // Return true if the state given means the square is occupied, false otherwise
    bool isStateOccupied(int state) {
        return state==0;
    }

    void led(int index,int on) {
        digitalWrite(output[index],on?1:0);
        ledState[index] = on;
    }
};

char* getString(const char* prompt,char* value,int size) {
    char* buffer = (char*)calloc(1,size);
    printf("%s [%s]: ",prompt,value);
    fgets(buffer,size,stdin);
    if(strlen(buffer)) {
        sscanf(buffer,"%s",value);
    }
    free(buffer);

    return value;
}

int getNumber(const char* prompt,int value) {
    char buffer[80];
    printf("%s [0x%02X (%d)]: ",prompt,value,value);
    fgets(buffer,sizeof(buffer),stdin);
    if(strlen(buffer)) {
        sscanf(buffer,"%d",&value);
    }

    return value;
}

int getBool(const char* prompt,int value) {
    char buffer[3] = "N";
    if(value) {
        strcpy(buffer,"Y");
    }
    getString(prompt,buffer,sizeof(buffer));
    if('y' == buffer[0] || 'Y' == buffer[0])
        value=1;
    else
        value=0;

    return value;
}

//int writeReg8(int fd,int reg,int data) {
//    int retval = wiringPiI2CWriteReg8(fd,reg,data);
//    if(-1 == retval) {
//        perror("wiringPiI2CWriteReg8");
//        exit(-1);
//    }
//    return retval;
//}
//
//int readReg8(int fd,int reg) {
//    int data = wiringPiI2CReadReg8(fd,reg);
//    if(-1 == data) {
//        perror("wiringPiI2CReadReg8");
//        exit(-1);
//    }
//    return data;
//}

void readRow(int base,int* rowState) {
    int i;
    for(i=0; i<8; i++) {
        rowState[i] = digitalRead(base+i);
    }
}

void showRow(int base,int* rowState) {
    int i;
    for(i=0; i<8; i++) {
        digitalWrite(base+i,rowState[7-i]?0:1);
    }
}

void testRow(int argc,char* argv[]) {
    int i;
    int devId = 0x27;
    int rowState[8]={1,1,1,1,1,1,1,1};

    printf("MCP Tester %s\n",VERSION);
    if(1 == argc) {
        devId = getNumber("i2c device id",devId);

        printf("\n");
    }

    printf("Device ID: %02XH (%03d)\n",devId,devId);

    wiringPiSetup();
//    wiringPiSetupGpio();	//use BCM pin numbers
    int mcp = mcp23017Setup(BASE_I2C,devId);
    if(mcp<0)
        perror("wiringPiI2CSetup");

    for(i=0; i<8; i++) {
        pinMode(BASE_I2C_OUTPUT+i,OUTPUT);
        pinMode(BASE_I2C_INPUT+i,INPUT);
        pullUpDnControl (BASE_I2C_INPUT+i,PUD_UP);
    }

    printf("square lights up when piece down\n");
    int counter=0;
    while(1) {
        readRow(BASE_I2C_INPUT,rowState);
        showRow(BASE_I2C_OUTPUT,rowState);
        delay(10);
    }

    printf("done\n");

}

void allrows(bool swap) {
    int devId=0x20;
    int index=0;
    int baseInput=250-64*2;
    int baseOutput=baseInput+8;
    int input[64];
    int output[64];
    int mcp[8];

    wiringPiSetup();

    for(int row = 0; row<8; ++row) {
        mcp[row]=mcp23017Setup(baseInput,devId+row);
        if(mcp[row]<0)
            perror("wiringPiI2CSetup");

        for(int col=0; col<8; col++) {
            int inputCol = col;
            int outputCol = 7-col;
            pinMode(baseInput+inputCol,INPUT);
            pullUpDnControl(baseInput+inputCol,PUD_UP);
            pinMode(baseOutput+outputCol,OUTPUT);

            input[index]=baseInput+inputCol;
            output[index]=baseOutput+outputCol;

            ++index;
        }
        baseInput+=16;
        baseOutput+=16;
    }

//    while(true) {
//        for(int i=0; i<64; i++) {
//                digitalWrite(output[i],0);
//                delay(50);
//        }
//        for(int i=0; i<64; i++) {
//                digitalWrite(output[i],1);
//                delay(50);
//        }
//    }

    printf("Turning off led's...\n");
    for(int i=0; i<64; i++) {
        digitalWrite(output[i],0);
    }
    if(swap) {
        int i=8*4+3;
        int t=input[i];
        input[i]=input[i+1];
        input[i+1]=t;
    }
    printf("Press any key\n");
    getchar();
    printf("Place pieces, square lights up when piece is down\n");
    while(true) {
        for(int i=0; i<64; i++) {
            int state = digitalRead(input[i]);
            digitalWrite(output[i],state?0:1);
        }
        delay(10);
    }


}

int main(int argc,char* argv[]) {
    bool swap=false;
    unsigned16 wPort = 9999;
    for(int i=0; i<argc; i++) {
        if(!strcmp(argv[i],"-s")) {
            swap=true;
        } else if(!strcmp(argv[i],"-p")) {
            wPort = atoi(argv[++i]);
        }
    }
    printf("Binding to port %d\n",wPort);
    SockAddr saBind((ULONG)INADDR_ANY,wPort);
    printf("construction\n");
    ControllerServer server(saBind,swap);
    printf("server running on port %d\n",wPort);
    server.startServer();
    printf("Server finished\n");
    return 0;
}
