/*
 * Created on Feb 2, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.transfer;

import java.io.IOException;
import java.net.Socket;

import com.bayanihancomputing.util.time.DoubleSpeedMeter;

import edu.utk.cs.loci.ibp.Log;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class TransferThread extends Thread
{
    // set priority to be slightly LESS than normal processes so
    // it doesn't bog down other normal processes.
    public static int TRANSFER_THREAD_PRIORITY = Thread.NORM_PRIORITY - 1;
    
    public static boolean MONITOR_ON = true;
    
    public static Log DEBUG = new Log( true );

    protected int id;
    protected Socket socket;
    protected String host;
    protected int port;
    protected String status;
    protected Object curJob;

    protected boolean monitorOn = MONITOR_ON;
    protected DoubleSpeedMeter speedMeter = new DoubleSpeedMeter();

    /**
     * 
     */
    public TransferThread( int id )
    {
        super();
        this.id = id;
        // TODO Auto-generated constructor stub
        
        this.setPriority( TransferThread.TRANSFER_THREAD_PRIORITY );
    }

    

    /**
     * @return Returns the id.
     */
    public long getId()
    {
        return this.id;
    }
    
    /**
     * @return Returns the socket.
     */
    public Socket getSocket()
    {
        return this.socket;
    }

    /**
     * @param socket The socket to set.
     */
    //    public void setSocket( Socket socket )
    //    {
    //        this.socket = socket;
    //    }
    /**
     * @return Returns the host.
     */
    public String getHost()
    {
        return this.host;
    }

    /**
     * @return Returns the port.
     */
    public int getPort()
    {
        return this.port;
    }

    /**
     * @return Returns the status.
     */
    public String getStatus()
    {
        return this.status;
    }

    /**
     * @param status The status to set.
     */
    public void setStatus( String status )
    {
        this.status = status;
    }

    /**
     * @return Returns the monitorOn.
     */
    public boolean isMonitorOn()
    {
        return this.monitorOn;
    }

    /**
     * @param monitorOn The monitorOn to set.
     */
    public void setMonitorOn( boolean monitorOn )
    {
        this.monitorOn = monitorOn;
    }
    
    

    /**
     * @return Returns the speedMeter.
     */
    public DoubleSpeedMeter getSpeedMeter()
    {
        return this.speedMeter;
    }
    
    
    
    /**
     * @return Returns the job.
     */
    public Object getCurJob()
    {
        return this.curJob;
    }
    /**
     * @param job The job to set.
     */
    public void setCurJob( Object job )
    {
        this.curJob = job;
    }
    
    /**
     * Use this if you want the thread to be interruptible
     * even as the socket is being created.
     * Otherwise, you don't even get a chance to call setSocket()
     * because just creating the Socket instance from the
     * calling code already tries to connect and can
     * hang even before you can call setSocket() on currentThread.
     * 
     * Actually ... this doesn't work either.  Apparently,
     * the constructor of Socket really blocks until it is connected,
     * so the assignment to this.socket doesn't even happen.
     * 
     * @param host
     * @param port
     * @return
     * @throws IOException
     */
    public Socket createSocket( String host, int port ) throws IOException
    {
        this.host = host;
        this.port = port;
        DEBUG.println( "Thread " + this + ": Calling createSocket(" + host
            + "," + port + ") ..." );

        this.status = "Creating socket";
        this.speedMeter.reset();
        this.speedMeter.start();

        this.socket = new Socket( host, port );

        if ( this.socket != null )
        {
            this.status = "Socket created";
        }
        else
        {
            this.status = "Socket creation failed";
        }

        DEBUG.println( "Thread " + Thread.currentThread()
            + ": Socket created: " + this.socket );

        return this.socket;
    }
    
    public void closeSocket()
    {
        try
        {
            if ( socket != null )
            {
                DEBUG.println( "Thead " + this + ": closing socket ... "
                    + socket );
                socket.close();
            }
            else
            {
                DEBUG.println( "Thead " + this + ": socket is null" );
            }
        }
        catch ( IOException ioe )
        {
            DEBUG.error( "Thread " + this + ": exception closing socket ... "
                + ioe );
        }

        this.status = "Socket closed.";
        
        // maybe keep the old host and port so we still have the old info
//        this.host = "";
//        this.port = 0;
    }
    

    public void interrupt()
    {
        DEBUG.println( "Thread " + this + " interrupt called." );

        this.closeSocket();
        DEBUG.println( "Thread " + this + ": Calling super.interrupt() ..." );
        super.interrupt();

    }

}
