/* $Id: ProtocolService.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.ibp;

import java.net.*;
import java.io.*;

import edu.utk.cs.loci.transfer.TransferThread;

final class ProtocolService
{
    public static Log DEBUG = new Log( true );

    // Protocol version tag.
    static final int PROTOCOL_VERSION = 0;

    // Command tags.
    static final int ALLOCATE = 1;
    static final int STORE = 2;
    static final int DELIVER = 3;
    static final int STATUS = 4;
    static final int SEND = 5;
    static final int MCOPY = 6;
    static final int REMOTE_STORE = 7;
    static final int LOAD = 8;
    static final int MANAGE = 9;
    static final int NOP = -1;

    // MANAGE sub-command tags.
    static final int PROBE = 40;
    static final int INCREMENT = 41;
    static final int DECREMENT = 42;
    static final int CHANGE = 43;
    static final int CONFIG = 44;

    static final int STATUS_INQUIRE = 1;
    static final int STATUS_CHANGE = 2;

    // Reliability codes.
    static final int SOFT = 1;
    static final int HARD = 2;

    // Allocation type codes.
    static final int BYTEARRAY = 1;
    static final int BUFFER = 2;
    static final int FIFO = 3;
    static final int CIRQ = 4;

    // Success return code
    static final int SUCCESS = 1;

    // Generic allocate routine.
    private static final Allocation allocate( Depot depot, int timeout,
        int duration, long size, int reliability, int type )
        throws IBPException
    {
        try
        {
            //DEBUG.println("Connecting to "+depot.toString());
            //Socket s=new Socket(depot.getHost(),depot.getPort());
            Socket s = new Socket();
            s.connect(
                new InetSocketAddress( depot.getHost(), depot.getPort() ),
                10 * 1000 );
            s.setSoTimeout( timeout * 1000 );

            //DEBUG.println("Acquiring I/O streams");
            OutputStream os = s.getOutputStream();
            //InputStream  is = s.getInputStream();
            BufferedReader is = new BufferedReader( new InputStreamReader(
                s.getInputStream() ) );

            String cmd = new String( PROTOCOL_VERSION + " " + ALLOCATE + " "
                + reliability + " " + type + " " + duration + " " + size + " "
                + timeout + "\n" );

            //DEBUG.println("ProtocoleService:allocate: Sending request...");
            os.write( cmd.getBytes() );

            //DEBUG.println("Receiving response");
            StringBuffer buf = new StringBuffer();
            //int data=is.read();
            //while(data!=-1) {
            //    buf.append((char)data);
            //    data=is.read();
            //}
            buf.append( is.readLine() );
            //DEBUG.println("ProtocoleService:allocate: response received.");
            //DEBUG.println("buf= " + buf);

            //DEBUG.println("Parsing response");
            String[] responseComponents = buf.toString().split( "[ \n]" );

            if ( responseComponents.length < 3 )
            {
                throw (new IBPException(
                    Integer.parseInt( responseComponents[1] ) ));
            }

            //DEBUG.println("Creating capabilities");
            Capability readCapability = new Capability( responseComponents[1] );
            Capability writeCapability = new Capability( responseComponents[2] );
            Capability manageCapability = new Capability( responseComponents[3] );

            //DEBUG.println("Closing connection");
            s.close();

            //DEBUG.println("Returning allocation");
            return (new Allocation(
                readCapability, writeCapability, manageCapability ));
        }
        catch ( Exception e )
        {
            throw (new IBPException( -38 ));
        }
    }

    static final Allocation allocateSoftByteArray( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, SOFT, BYTEARRAY ));
    }

    static final Allocation allocateHardByteArray( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, HARD, BYTEARRAY ));
    }

    static final Allocation allocateSoftBuffer( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, SOFT, BUFFER ));
    }

    static final Allocation allocateHardBuffer( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, HARD, BUFFER ));
    }

    static final Allocation allocateSoftFifo( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, SOFT, FIFO ));
    }

    static final Allocation allocateHardFifo( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, HARD, FIFO ));
    }

    static final Allocation allocateSoftCirq( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, SOFT, CIRQ ));
    }

    static final Allocation allocateHardCirq( Depot depot, int timeout,
        int duration, long size ) throws IBPException
    {
        return (allocate( depot, timeout, duration, size, HARD, CIRQ ));
    }

    static final int store( Allocation alloc, byte[] buf, int timeout, long size )
        throws IBPException
    {
        try
        {
            Depot depot = alloc.getDepot();

            Socket s = new Socket( depot.getHost(), depot.getPort() );
            s.setSoTimeout( timeout * 1000 );

            OutputStream os = s.getOutputStream();
            // * InputStream is=s.getInputStream();
            BufferedReader is = new BufferedReader( new InputStreamReader(
                s.getInputStream() ) );

            Capability writeCapability = alloc.getWriteCapability();

            String cmd = new String( PROTOCOL_VERSION + " " + STORE + " "
                + writeCapability.getKey() + " " + writeCapability.getWRMKey()
                + " " + size + " " + timeout + "\n" );

            os.write( cmd.getBytes() );

            StringBuffer result = new StringBuffer();
            //int data=is.read();
            //while((char)data!='\n') {
            //	result.append((char)data);
            //	data=is.read();
            //}
            result.append( is.readLine() );

            int returnCode = Integer.parseInt( result.toString().trim() );

            if ( returnCode != SUCCESS )
            {
                throw (new IBPException( returnCode ));
            }

            os.write( buf );

            result = new StringBuffer();
            //data=is.read();
            //while(data!=-1) {
            //	result.append((char)data);
            //	data=is.read();
            //}
            result.append( is.readLine() );

            String[] fields = result.toString().split( " " );
            returnCode = Integer.parseInt( fields[0].trim() );
            int bytesWritten = Integer.parseInt( fields[1].trim() );

            if ( returnCode != SUCCESS )
            {
                throw (new IBPException( returnCode ));
            }

            return (bytesWritten);
        }
        catch ( IOException e )
        {
            throw (new IBPException( -1 ));
        }
    }

    static final int load( Allocation alloc, byte[] buf, int timeout,
        long size, long readOffset, int writeOffset ) throws IBPException
    {
        Thread curThread = Thread.currentThread();
        TransferThread transferThread = null;
        try
        {
            Depot depot = alloc.getDepot();
            Socket s = null;

            if ( curThread instanceof TransferThread )
            {
                transferThread = (TransferThread) curThread;
                s = transferThread.createSocket(
                    depot.getHost(), depot.getPort() );
            }
            else
            {
                s = new Socket( depot.getHost(), depot.getPort() );
            }

            boolean monitorOn = (transferThread != null)
                && (transferThread.isMonitorOn());

            s.setSoTimeout( timeout * 1000 );

            OutputStream os = s.getOutputStream();
            InputStream is = s.getInputStream();
            // BufferedReader is = new BufferedReader(new InputStreamReader(s.getInputStream()));

            Capability readCapability = alloc.getReadCapability();

            String cmd = new String( PROTOCOL_VERSION + " " + LOAD + " "
                + readCapability.getKey() + " " + readCapability.getWRMKey()
                + " " + readOffset + " " + size + " " + timeout + "\n" );

            DEBUG.println( "ProtocolService: sending command " + cmd );
            if ( monitorOn )
            {
                transferThread.setStatus( "sending request for " + readOffset
                    + " " + size );
            }

            os.write( cmd.getBytes() );

            StringBuffer result = new StringBuffer();
            int data = is.read();
            while ( (char) data != '\n' )
            {
                result.append( (char) data );
                data = is.read();
            }
            // result.append(is.readLine());

            DEBUG.println( "ProtocolService: got result " + result );

            int returnCode;
            int toRead;
            if ( result.toString().indexOf( '-' ) != -1 )
            {
                returnCode = Integer.parseInt( result.toString().trim() );
                throw (new IBPException( returnCode ));
            }
            else
            {
                String[] fields = result.toString().split( " " );

                returnCode = Integer.parseInt( fields[0] );
                toRead = Integer.parseInt( fields[1] );
            }

            String readDetails = "Reading offset=" + readOffset + ", toRead="
                + toRead;
            DEBUG.println( "ProtocolService: " + readDetails + " from "
                + depot.getHost() + ":" + depot.getPort() );

            if ( monitorOn )
            {
                transferThread.setStatus( readDetails );
            }

            int bytesRead = is.read( buf, writeOffset, toRead );
            while ( bytesRead < toRead )
            {
                //                DEBUG.println( "ProtocolService: Reading more from " + depot.getHost() + ":" + depot.getPort()
                //                    + "bytesRead= " + bytesRead + ", left toRead=" + (toRead - bytesRead) );

                bytesRead += is.read(
                    buf, writeOffset + bytesRead, toRead - bytesRead );
                
                if ( monitorOn )
                {
                    transferThread.getSpeedMeter().setProgress( bytesRead );
                }
            }

            readDetails = "Done reading " + bytesRead + " bytes";
            DEBUG.println( "ProtocolService: " + readDetails + " from " 
                + depot.getHost() + ":"
                + depot.getPort()  );
            if ( monitorOn )
            {
                transferThread.setStatus( readDetails );
            }

            if ( transferThread != null )
            {
                transferThread.closeSocket();
            }
            else
            {
                s.close();
            }

            return (bytesRead);
        }
        catch ( IOException e )
        {
            DEBUG.error( "IBP ProtocolService.load: IOException: " + e );
            transferThread.setStatus( "Error encountered during IBP load" );
            // e.printStackTrace();
            throw (new IBPException( -1 ));
        }
    }

    static final int copy( Allocation src, Allocation dest, int timeout,
        long size, long offset ) throws IBPException
    {
        try
        {
            Depot depot = src.getDepot();

            Socket s = new Socket( depot.getHost(), depot.getPort() );
            s.setSoTimeout( timeout * 1000 );

            OutputStream os = s.getOutputStream();
            // * InputStream is=s.getInputStream();
            BufferedReader is = new BufferedReader( new InputStreamReader(
                s.getInputStream() ) );
            Capability readCapability = src.getReadCapability();

            String cmd = new String( PROTOCOL_VERSION + " " + SEND + " "
                + readCapability.getKey() + " " + dest.getWriteCapability()
                + " " + readCapability.getWRMKey() + " " + offset + " " + size
                + " " + timeout + " " + timeout + " " + timeout + "\n" );

            os.write( cmd.getBytes() );

            StringBuffer tmp = new StringBuffer();
            //int data=is.read();
            //while((char)data!='\n') {
            //	tmp.append((char)data);
            //	data=is.read();
            //}
            tmp.append( is.readLine() );
            String result = tmp.toString();

            int returnCode;
            int bytesCopied = 0;
            if ( result.toString().indexOf( '-' ) != -1 )
            {
                returnCode = Integer.parseInt( result.toString().trim() );
                throw (new IBPException( returnCode ));
            }
            else
            {
                String[] fields = result.toString().split( " " );

                returnCode = Integer.parseInt( fields[0] );
                bytesCopied = Integer.parseInt( fields[1] );
            }

            return (bytesCopied);
        }
        catch ( IOException e )
        {
            DEBUG.error( "IBP ProtocolService.copy: IOException: " + e );
            // e.printStackTrace();
            throw (new IBPException( -1 ));
        }
    }
}