# chesslrcontroller
Board controller for ChessLR e-board. This is the new low level controller that handles the i2c interface. Moving the controller logic from ChessLR project to here also allows me to be able to connect to the controller remotely, from a desktop for instance. 

I broke out the controller from ChessLR for two reasons. The main one was that the Java GPIO lib pi4j has issues when using so many MCP chips. It just randomly messes up the handlers that get called when the input state changes. 

The other reason was to allow for being able to connect from a desktop computer, as it would be easier to annotate the moves using a desktop/laptop then it would be using the touch screen.

 


