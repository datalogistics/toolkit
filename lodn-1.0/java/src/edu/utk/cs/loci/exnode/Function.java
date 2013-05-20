/* $Id: Function.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.util.*;
import org.w3c.dom.*;

import edu.utk.cs.loci.ibp.Log;

import java.io.IOException;

// undocumented package
import sun.misc.BASE64Encoder;
import sun.misc.BASE64Decoder;

public abstract class Function extends MetadataContainer
{
    public static Log DEBUG = new Log( true );

    Map arguments;
    String name;

    public abstract void setkey( String key ) throws Exception;

    public abstract byte[] execute( byte[] rawBuf ) throws Exception;

    public Function( String name )
    {
        this.name = name;
        this.arguments = new HashMap();
    }

    public String getName()
    {
        return (name);
    }

    public void addArgument( Argument arg )
    {
        arguments.put( arg.getName(), arg );
    }

    public Iterator getArguments()
    {
        Set keySet = arguments.keySet();
        ArrayList list = new ArrayList();
        Iterator i = keySet.iterator();
        String name;
        while ( i.hasNext() )
        {
            name = (String) i.next();
            list.add( arguments.get( name ) );
        }
        return (list.iterator());
    }

    public Argument getArgument( String name )
    {
        return ((Argument) arguments.get( name ));
    }

    // generate a random key in a String
    // New functions can overload this method to generate a more adapted key
    public String genkey() throws Exception
    {
        return "ThIsIsAdEfauLtKeY31415"; // TODO
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:function name=\"" + name + "\">\n" );

        Iterator i = getMetadata();
        Metadata md;
        while ( i.hasNext() )
        {
            md = (Metadata) i.next();
            sb.append( md.toXML() );
        }

        Argument arg;
        i = getArguments();
        while ( i.hasNext() )
        {
            arg = (Argument) i.next();
            sb.append( arg.toXML() );
        }

        sb.append( "</exnode:function>\n" );

        return (sb.toString());
    }

    public static Function fromXML( Element e ) throws DeserializeException
    {
        Function f;
        try
        {
            f = FunctionFactory.getFunction( e.getAttribute( "name" ) );
        }
        catch ( UnknownFunctionException ufe )
        {
            throw (new DeserializeException( ufe.getMessage() ));
        }

        NodeList metadata = e.getElementsByTagNameNS(
            Exnode.namespace, "metadata" );
        Metadata md;
        Element element;
        for ( int i = 0; i < metadata.getLength(); i++ )
        {
            element = (Element) metadata.item( i );
            if ( element.getParentNode().equals( e ) )
            {
                md = Metadata.fromXML( element );
                f.addMetadata( md );
            }
        }

        NodeList subfunctions = e.getElementsByTagNameNS(
            Exnode.namespace, "function" );
        Function func;
        for ( int i = 0; i < subfunctions.getLength(); i++ )
        {
            element = (Element) subfunctions.item( i );
            if ( element.getParentNode().equals( e ) )
            {
                func = Function.fromXML( element );
                f.addArgument( new FunctionArgument( func.getName(), func ) );
            }
        }

        NodeList arguments = e.getElementsByTagNameNS(
            Exnode.namespace, "argument" );
        Argument arg;
        for ( int i = 0; i < arguments.getLength(); i++ )
        {
            element = (Element) arguments.item( i );
            if ( element.getParentNode().equals( e ) )
            {
                arg = Argument.fromXML( element );
                f.addArgument( arg );
            }
        }

        return (f);
    }

    /* ================== USEFUL METHODS ================== */

    /*
     The methods below are common methods for a lot of function
     */

    // http://www.nsftools.com/tips/JavaTips.htm#sunbase64
    public String encode( byte[] raw )
    {
        BASE64Encoder encoder = new BASE64Encoder();
        String ch = encoder.encodeBuffer( raw );
        return ch.substring( 0, ch.length() - 1 );
    }

    public byte[] decode( String encodedStr )
    {
        BASE64Decoder decoder = new BASE64Decoder();
        try
        {
            return decoder.decodeBuffer( encodedStr );
        }
        catch ( IOException e )
        {
            DEBUG.error( "decoder: " + e );
        }
        return null;
    }

    static public String byteToHex( byte b )
    {
        // Returns hex String representation of byte b
        char hexDigit[] =
            { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c',
                'd', 'e', 'f' };
        char[] array =
            { hexDigit[(b >> 4) & 0x0f], hexDigit[b & 0x0f] };
        return new String( array );
    }

    public String bytesToHex( byte[] data )
    {
        StringBuffer buf = new StringBuffer();
        for ( int i = 0; i < data.length; i++ )
        {
            buf.append( byteToHex( data[i] ) );
        }
        return (buf.toString());
    }
}