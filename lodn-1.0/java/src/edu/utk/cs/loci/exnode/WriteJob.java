/* $Id: WriteJob.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.io.*;
import edu.utk.cs.loci.ibp.*;

public class WriteJob
{
    public static Log DEBUG = new Log( true );

    private Exnode exnode;
    private long offset;
    private int length;
    private int duration;
    private SyncFile file;
    private DepotList depots;

    private String functionName;

    final static int TIMEOUT_ALLOCATE = 20;
    final static int TIMEOUT_WRITE = 30;

    public WriteJob( Exnode exnode, long offset, int length, int duration,
        SyncFile file, DepotList depots, String functionName )
    {
//        if ( length > Integer.MAX_VALUE )
//        {
//            throw new IllegalArgumentException( "WriteJob length cannot be greater than 2G" );
//        }
        this.exnode = exnode;
        this.offset = offset;
        this.length = length;
        this.duration = duration;
        this.file = file;
        this.depots = depots;
        this.functionName = functionName;
    }

    public void execute() throws WriteException
    {

        byte[] buf = new byte[length];
        Function function = null;
        String key = null;

        try
        {
            file.read( buf, offset );
            //DEBUG.println("WriteJob09: (before)buf.length: " + buf.length);
            if ( functionName != null )
            {
                // Apply function (Encryption, Compression, Checksum,...)
                function = FunctionFactory.getFunction( functionName );
                key = function.genkey(); // return key in String format and set key
                buf = function.execute( buf ); // can change length of buf (e.g compression)
                //DEBUG.println("WriteJob10: (after)buf.length: " + buf.length);
            }
        }
        catch ( IOException e )
        {
            throw (new WriteException( e.getMessage() ));
        }
        catch ( UnknownFunctionException e )
        {
            throw (new WriteException( e.getMessage() ));
        }
        catch ( Exception e )
        {
            throw (new WriteException( e.getMessage() ));
        }

        boolean done = false;
        Depot d = null;
        String capaHostInfo = "";
        String capaPortInfo = "";

        String statusMsg = "Writing ...";

        while ( !done && !exnode.cancel )
        {

            try
            {

                d = depots.next();

                Allocation a = d.allocateHardByteArray(
                    TIMEOUT_ALLOCATE, duration, buf.length );

                Mapping m = new Mapping();
                m.setAllocation( a );
                m.addMetadata( new IntegerMetadata(
                    "exnode_offset", (long) offset ) );
                m.addMetadata( new IntegerMetadata(
                    "logical_length", (long) length ) );
                m.addMetadata( new IntegerMetadata( "alloc_offset", (long) 0 ) );
                m.addMetadata( new IntegerMetadata(
                    "alloc_length", (long) buf.length ) );
                //m.addMetadata(new IntegerMetadata("e2e_blocksize", (int)buf.length ));
                m.addMetadata( new IntegerMetadata( "e2e_blocksize", (long) 0 ) );
                capaHostInfo = m.getAllocation().getReadCapability().getHost();
                capaPortInfo = m.getAllocation().getReadCapability().getPort();

                statusMsg = "Store in " + capaHostInfo + ":" + capaPortInfo
                    + " " + offset + " " + buf.length;

                DEBUG.println( "Store in\t" + capaHostInfo + ":" + capaPortInfo
                    + "\t" + offset + " " + buf.length );

                m.write( buf, TIMEOUT_WRITE, buf.length ); // store (2nd arg. is timeout)

                //============================== Jp
                if ( functionName != null )
                {
                    function.addArgument( new StringArgument( "KEY", key ) );

                    /* 
                     blocksize is used by : 
                     - Compress function: blocksize is the original size (=logical_length)
                     - Checksum function: add 33 characters length (checksum:data)
                     */
                    if ( functionName.equals( "checksum" ) )
                    {
                        function.addArgument( new IntegerArgument(
                            "blocksize",
                            (ChecksumFunction.CHECKSUM_HEADER_SIZE + length) ) );
                    }
                    else
                    {
                        function.addArgument( new IntegerArgument(
                            "blocksize", length ) );
                    }

                    //DEBUG.println("WriteJob20: buf.length: " + buf.length);
                    m.setFunction( function );
                }
                //============================== Jp
                exnode.addMapping( m );

                exnode.updateProgress( length, statusMsg );

                done = true;

            }
            catch ( IBPException e )
            {
                DEBUG.println( "Depot " + d.toString()
                    + " unresponsive: removing" );
                depots.remove( d );
                if ( depots.length() == 0 )
                {
                    exnode.setError( new WriteException(
                        "Depots list exhausted.\n"
                            + "Please try another location." ) );
                    exnode.setCancel( true );
                    done = true;
                }
            }
            catch ( WriteException e )
            {
                DEBUG.println( "Can not write data on: " + capaHostInfo + ":"
                    + capaPortInfo + " (" + e + ")" );
            }
            catch ( IndexOutOfBoundsException e2 )
            {
                DEBUG.error( "Error in WriteJob.execute: " + e2 );
            }
        }
    }
}