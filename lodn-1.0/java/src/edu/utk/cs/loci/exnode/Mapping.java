/* $Id: Mapping.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import edu.utk.cs.loci.ibp.*;
import edu.utk.cs.loci.transfer.TransferThread;

import java.net.Socket;
import java.util.*;
import org.w3c.dom.*;

public class Mapping extends MetadataContainer
{
    public static Log DEBUG = new Log( true );

    Allocation allocation;
    Function function;

    public Mapping()
    {
        allocation = null;
        function = null;
        this.addMetadata( new IntegerMetadata( "alloc_offset", -1 ) );
        this.addMetadata( new IntegerMetadata( "exnode_offset", -1 ) );
        this.addMetadata( new IntegerMetadata( "logical_length", -1 ) );
    }

    public Allocation getAllocation()
    {
        return (allocation);
    }

    public void setAllocation( Allocation a )
    {
        this.allocation = a;
    }

    public Function getFunction()
    {
        return (function);
    }

    public void setFunction( Function f )
    {
        this.function = f;
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:mapping>\n" );

        Iterator i = getMetadata();
        Metadata md;
        while ( i.hasNext() )
        {
            md = (Metadata) i.next();
            sb.append( md.toXML() );
        }

        try
        {
            sb.append( function.toXML() );
        }
        catch ( NullPointerException e )
        {
            //sb.append("<exnode:function></exnode:function>\n");
        }

        sb.append( "<exnode:read>" );
        try
        {
            sb.append( allocation.getReadCapability().toString() );
        }
        catch ( NullPointerException e )
        {
        }
        sb.append( "</exnode:read>\n" );

        sb.append( "<exnode:write>" );
        try
        {
            sb.append( allocation.getWriteCapability().toString() );
        }
        catch ( NullPointerException e )
        {
        }
        sb.append( "</exnode:write>\n" );

        sb.append( "<exnode:manage>" );
        try
        {
            sb.append( allocation.getManageCapability().toString() );
        }
        catch ( NullPointerException e )
        {
        }
        sb.append( "</exnode:manage>\n" );

        sb.append( "</exnode:mapping>\n" );

        return (sb.toString());
    }

    public static Mapping fromXML( Element e ) throws DeserializeException
    {
        Mapping mapping = new Mapping();

        NodeList metadata = e.getElementsByTagNameNS(
            Exnode.namespace, "metadata" );
        Metadata md;
        for ( int i = 0; i < metadata.getLength(); i++ )
        {
            Element element = (Element) metadata.item( i );
            if ( element.getParentNode().equals( e ) )
            {
                md = Metadata.fromXML( element );
                mapping.addMetadata( md );
            }
        }

        NodeList function = e.getElementsByTagNameNS(
            Exnode.namespace, "function" );
        Function f;
        for ( int i = 0; i < function.getLength(); i++ )
        {
            f = Function.fromXML( (Element) function.item( i ) );
            mapping.setFunction( f );
        }
        if ( mapping.getFunction() == null )
        {
            mapping.setFunction( new IdentityFunction() );
        }

        try
        {
            NodeList nl = e.getElementsByTagNameNS( Exnode.namespace, "read" );
            Element readElement = (Element) nl.item( 0 );
            Capability readCapability;
            if ( readElement.hasChildNodes() )
            {
                readCapability = new Capability(
                    readElement.getFirstChild().getNodeValue() );
            }
            else
            {
                readCapability = null;
            }

            nl = e.getElementsByTagNameNS( Exnode.namespace, "write" );
            Element writeElement = (Element) nl.item( 0 );
            Capability writeCapability;
            if ( writeElement.hasChildNodes() )
            {
                writeCapability = new Capability(
                    writeElement.getFirstChild().getNodeValue() );
            }
            else
            {
                writeCapability = null;
            }

            nl = e.getElementsByTagNameNS( Exnode.namespace, "manage" );
            Element manageElement = (Element) nl.item( 0 );
            Capability manageCapability;
            if ( manageElement.hasChildNodes() )
            {
                manageCapability = new Capability(
                    manageElement.getFirstChild().getNodeValue() );
            }
            else
            {
                manageCapability = null;
            }

            Allocation a = new Allocation(
                readCapability, writeCapability, manageCapability );
            mapping.setAllocation( a );
        }
        catch ( Exception ex )
        {
            ex.printStackTrace();
            DEBUG.error( "Error in Deserializing mapping: " + ex );

            throw (new DeserializeException( ex.getMessage() ));
        }

        return (mapping);
    }

    public byte[] read( int timeout, long size, long readOffset,
        int writeOffset ) throws ReadException
    {
        try
        {
//            long allocationOffset = getAllocationOffset(); // Always 0 !
//            long allocationLength = getAllocationLength(); // == logical_length if no function applied

            boolean isIdentityFunction = this.getFunction().getName().equals( "identity" );
            long loadOffset = getAllocationOffset() + readOffset;
            long loadLength = size;
            
            if ( !isIdentityFunction )
            { // If it is NOT Identity function

                DEBUG.println( "Mapping : It is NOT Identity Function!" );

                //Some function does not required a key (e.g checksum, compress,...)
                Argument keyarg = this.getFunction().getArgument( "KEY" );
                String key = null;
                if ( keyarg != null )
                {
                    key = keyarg.getString();
                }

                function.setkey( key );

                //length = this.getFunction().getArgument("blocksize").getInteger().longValue();
                
                // loadOffset = // compute the proper offset for the corresponding readOffset if there is a function  
                loadLength = this.getAllocationLength();
            }

            // NOTE: Note, we cannot create an array with > 2GB items,
            // Thus, make sure loadLength is never > 2GB
            if ( loadLength > Integer.MAX_VALUE )
            {
                throw new ReadException( "Mapping.read(): size " + loadLength 
                    + " is greater than max int" );
            }
            
            byte[] rawBuf = new byte[(int) loadLength];

            DEBUG.println( "Mapping: Request Load at offset: " + readOffset
                + ", " + loadLength + " Bytes." );

            int bytesRead = allocation.load(
                rawBuf, timeout, loadLength, loadOffset, writeOffset );

            String executeStatus = "Mapping: Executing function on rawBuf. length: "
                + rawBuf.length + ", bytesRead=" + bytesRead; 
            DEBUG.println( executeStatus );

            Thread curThread = Thread.currentThread();
            boolean monitorOn = false;
            TransferThread transferThread = null;
            
            if ( curThread instanceof TransferThread )
            {
                transferThread = (TransferThread) curThread;
                if ( transferThread.isMonitorOn() )
                {
                    monitorOn = true;
                }
            }
            
            if ( monitorOn )
            {
                transferThread.setStatus( executeStatus );
            }

            byte[] outBuf = function.execute( rawBuf );

            DEBUG.println( "Mapping: outBuf.length: " + outBuf.length );

            if ( monitorOn )
            {
                transferThread.setStatus( "Done reading and executing function." );
            }

            return (outBuf);
        }
        catch ( Exception e )
        {
            //e.printStackTrace();
            //DEBUG.error("Mapping60: e = " + e.getMessage());
            throw (new ReadException( e.getMessage() ));
        }
    }

    public int write( byte[] buf, int timeout, long size )
        throws WriteException
    {
        try
        {
            return (allocation.store( buf, timeout, size ));
        }
        catch ( IBPException e )
        {
            throw (new WriteException( e.getMessage() ));
        }
    }

    public long getAllocationOffset()
    {
        return (getMetadata( "alloc_offset" ).getInteger().longValue());
    }

    public long getAllocationLength()
    {
        return (getMetadata( "alloc_length" ).getInteger().longValue());
    }

    public long getExnodeOffset()
    {
        return (getMetadata( "exnode_offset" ).getInteger().longValue());
    }

    public long getLogicalLength()
    {
        return (getMetadata( "logical_length" ).getInteger().longValue());
    }

    public long getE2EBlockSize()
    {
        return (getMetadata( "e2e_blocksize" ).getInteger().longValue());
    }

    public boolean hasE2EBlocks()
    {
        if ( getE2EBlockSize() > 0 )
        {
            return (true);
        }
        else
        {
            return (false);
        }
    }
}