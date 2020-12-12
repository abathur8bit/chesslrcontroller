import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import ssobjects.telnet.TelnetMessage;
import ssobjects.telnet.TelnetServer;
import ssobjects.telnet.TelnetServerSocket;

import java.io.IOException;

public class EchoServer extends TelnetServer {
    private static final Logger log = LoggerFactory.getLogger(EchoServer.class);
    public static final long FREQUENCY = 100;       //how often to idle in milliseconds
    public static final int DEFAULT_PORT = 9998;
    public static void main(String[] args) throws Exception {
        EchoServer server = new EchoServer(DEFAULT_PORT);
        log.info("Server running on port {}",DEFAULT_PORT);
        server.run();
    }

    public EchoServer(int port) throws IOException {
        super(null,port,FREQUENCY);
    }

    protected boolean hasIllegalChars(String s) {
        String allowed="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz 1234567890{}:,[]/-+=\"'";
        for(int i=0; i<s.length(); i++) {
            if(-1 == allowed.indexOf(s.charAt(i)))
                return true;
        }
        return false;
    }
    @Override
    public void processSingleMessage(TelnetMessage msg)
    {
        try {
            if (msg.text != null && msg.text.length() > 0) {
                if(hasIllegalChars(msg.text)) {
                    log.warn("Trying to send illegal characters from {}",msg.sock.getSockAddr());
                    close(msg.sock);
                } else {
                    log.debug("Read from ["+msg.sock.getSockAddr()+"] ["+msg.toString()+"]");
                    if(msg.toString() == null)
                        close(msg.sock);
                    else {
                        String[] input = msg.toString().split(" ");

                        try {
                            if(input[0].equalsIgnoreCase("ping"))
                                msg.sock.println("pong");
                            else
                                printlnAllExcept(msg.toString(),msg.sock);
                        } catch(Exception e) {
                            msg.sock.println("ERROR: "+e.getLocalizedMessage());
                        }
                    }
                }
            }
        } catch(Exception e) {
            log.debug("Closing connection "+msg.sock);
            close(msg.sock);
        }
    }

    @Override
    public void connectionAccepted(TelnetServerSocket sock) {
        log.debug("Connection from ["+sock.getSockAddr()+"]");
        try {
            sock.println("Hello");
        } catch(IOException e) {
            log.debug("socket already closed");
            close(sock);
        }
    }

    @Override
    public void connectionClosed(TelnetServerSocket sock,IOException e) {
        log.debug("Connection closed from ["+sock.getSockAddr()+"]");
    }

    @Override
    public void idle(long deltaTime) {}

}
