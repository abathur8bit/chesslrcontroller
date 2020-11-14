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



class ControllerServer : public TelnetServer {
public:
    enum {FREQ=10};
    int devId;
    int index;
    int baseInput;
    int baseOutput;
    int input[64];
    int output[64];
    int squareState[64];
    int ledState[64];
    const char* rowNames="87654321";
    const char* colNames="ABCDEFGH";
    int mcp[8];

    ControllerServer(SockAddr& saBind,bool swap) : TelnetServer(saBind,FREQ){
        devId=0x20;
        index=0;
        baseInput=250-64*2;
        baseOutput=baseInput+8;
        memset(squareState,0,sizeof(squareState));

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
        for(int i=0; i<64; i++) {
            digitalWrite(output[i],0);
        }
        if(swap) {
            int i=8*4+3;
            int t=input[i];
            input[i]=input[i+1];
            input[i+1]=t;
        }
        for(int i=0; i<64; i++) {
            squareState[i] = digitalRead(input[i]); //1=empty 0=occupied
            ledState[i] = 0;
        }
//        printf("Press any key\n");
//        getchar();
    }
    void processSingleMsg(PacketMessage* pmsg)
    {
        TelnetServerSocket* psocket = (TelnetServerSocket*)pmsg->socket();
        PacketBuffer* ppacket = pmsg->packet();
        switch(ppacket->getCmd())
        {
            //One way to handle the message. Process and reply within the switch.
            case PacketBuffer::pcNewConnection:   onConnection(pmsg);   break;
            case PacketBuffer::pcClosed:          printf("Connection closed.\n");          break;
            case TelnetServerSocket::pcFullLine:  onFullLine(pmsg);        break;
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
            string action = j["action"];
            printf("parsed and have action = %s\n",action.c_str());
            if(!action.compare("move")) {
                ChessAction *ca = parseJson(pszString);
                printf("action %s num moves=%d\n", ca->action(), ca->numMoves());
                for (int i = 0; i < ca->numMoves(); i++) {
                    psocket->println("move %d = %s index %d %d", i, ca->move(i).c_str(), ca->move(i).fromIndex(), ca->move(i).toIndex());
                    psocket->println("row %c col %c", ca->move(i).toRow(0), ca->move(i).toCol(0));
                    psocket->println("row %c col %c", ca->move(i).toRow(63), ca->move(i).toCol(63));

                    ledState[ca->move(i).fromIndex()] = 1;
                    ledState[ca->move(i).toIndex()] = 1;
                }

                psocket->println("Action=%s", ca->action());
                delete ca;
            } else if(!action.compare("ping")) {
                psocket->println("pong");
            } else {
                psocket->println("invalid");
            }
        } catch(json::parse_error& e) {
            psocket->println("json parse error: %s",e.what());
        }
    }
    void onConnection(PacketMessage* pmsg)
    {
        TelnetServerSocket* psocket = (TelnetServerSocket*)pmsg->socket();
        psocket->println("Hello");
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

    void idle(unsigned32 now) {
        char buffer[80];
        for(int i=0; i<64; i++) {
            int state = digitalRead(input[i]);
            if(state != squareState[i]) {
                snprintf(buffer,sizeof(buffer),"piece at %c%c is now %s\n",toCol(i),toRow(i),(occupied(state)?"occupied":"empty"));
                printf(buffer);
                send2All(buffer);
            }
            squareState[i] = state;
            digitalWrite(output[i],ledState[i]);
        }
        printf("");
    }

    // Return true if the square is occupied, false otherwise.
    bool isSquareOccupied(int index) {
        return squareState[index]==1;   //0=occupied, 1=not
    }
    // Return true if the state given means the square is occupied, false otherwise
    bool occupied(int state) {
        return state==0;
    }
    void led(int index,bool on) {

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
    printf("binding to port %d\n",wPort);
    SockAddr saBind((ULONG)INADDR_ANY,wPort);
    printf("construction\n");
    ControllerServer server(saBind,swap);
    printf("server running on port %d\n",wPort);
    server.startServer();
    printf("Server finished\n");
    return 0;
}