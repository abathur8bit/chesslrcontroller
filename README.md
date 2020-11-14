# chesslrcontroller
Board controller for ChessLR e-board. This is the new low level controller that handles the i2c interface. Moving the controller logic from ChessLR project to here also allows me to be able to connect to the controller remotely, from a desktop for instance. 

I broke out the controller from ChessLR for two reasons. The main one was that the Java GPIO lib pi4j has issues when using so many MCP chips. It just randomly messes up the handlers that get called when the input state changes. 

The other reason was to allow for being able to connect from a desktop computer, as it would be easier to annotate the moves using a desktop/laptop then it would be using the touch screen.

# Status

Prototype. I can send the controller one or more moves via socket in json format.

# Compiling and running
Note that you may need to use sudo since the wiringPI lib needs access to the i2c hardware.

    cmake -f CMakeLists.txt
    make 
    sudo ./chesslrcontroller

# Reference board

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

# JSON

Sample JSON

Sample for a normal move:

    {"action":"move","description":null,"moves":[{"from":"e1","to":"g1","type":"move"}]}
    {"action":"move","description":null,"moves":[{"from":"a1","to":"a2","type":"move"}]}

And another for castling short:

One line:

    {"action":"move","description":"Castle short","moves":[{"from":"e1","to":"g1","type":"move"},{"from":"h1","to":"f1","type":"move"}]}
    
Multiline:    

```
{
  "action": "move",
  "description": "Castle short",
  "moves": [
    {
      "from": "e1",
      "to": "g1",
      "type": "move"
    },
    {
      "from": "h1",
      "to": "f1",
      "type": "move"
    }
  ]
}
```

