import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

public class ChessConnector {
    List<String> readList = new ArrayList<String>();
    List<String> writeList = new ArrayList<String>();
    AtomicBoolean running = new AtomicBoolean(false);
    Socket chessSocket;
    Socket echoSocket;
    BufferedReader chessIn;
    PrintWriter chessOut;
    BufferedReader echoIn;
    PrintWriter echoOut;


    public static void main(String[] args) throws Exception {
        ChessConnector service = new ChessConnector(args[0],args[1]);
        service.startService();
    }

    public ChessConnector(String chesslr,String echo) throws Exception {
        System.out.println("connecting to chesslr "+chesslr);
        chessSocket = new Socket(chesslr,9999);
        chessIn = new BufferedReader(new InputStreamReader(chessSocket.getInputStream()));
        chessOut = new PrintWriter(chessSocket.getOutputStream());
        System.out.println("connecting to echo "+echo);
        echoSocket = new Socket(echo,9998);
        echoIn = new BufferedReader(new InputStreamReader(echoSocket.getInputStream()));
        echoOut = new PrintWriter(echoSocket.getOutputStream());
    }

    public void startService() {
        running.set(true);
        Thread chessThread = new Thread((Runnable) () -> {
            //reads from chess and writes to echo
            try {
                String line = chessIn.readLine();   //ignore first line
                System.out.println("First line from chesslr "+line);
                while(running.get()) {
                    line = chessIn.readLine();
                    System.out.println("From chesslr "+line);
                    if(line == null) {
                        running.set(false);
                    } else {
                        echoOut.print(line+"\r\n");
                        echoOut.flush();
                    }
                }
            } catch(IOException e) {
                e.printStackTrace();
                running.set(false);
            }
        });
        chessThread.start();

        try {
            String line = echoIn.readLine();    //ignore first line
            System.out.println("First line from echo "+line);
            while(running.get()) {
                line = echoIn.readLine();
                System.out.println("From echo "+line);
                if(line==null) {
                    running.set(false);
                } else {
                    chessOut.print(line+"\r\n");
                    chessOut.flush();
                }
            }
        } catch(Exception e) {
            e.printStackTrace();
            running.set(false);
        }

        System.out.println("Done");
    }

    public void snooze(long ms) {
        try { Thread.sleep(ms); } catch(InterruptedException e) {}
    }

}
