/*
 * Created on Nov 24, 2003
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

import org.w3c.dom.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public abstract class Argument
{
    String name;
    Object value;

    public String getName()
    {
        return (name);
    }

    public Function getFunction() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Integer getInteger() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Integer getInteger( int i ) throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Integer getInteger( int i, int j )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Double getDouble() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Double getDouble( int i ) throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Double getDouble( int i, int j )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());

    }

    public String getString() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public String getString( int i ) throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public String getString( int i, int j )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void insertInteger( int i, IntegerArgument arg )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void insertInteger( int i, int j, IntegerArgument arg )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void insertDouble( int i, DoubleArgument arg )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void insertDouble( int i, int j, DoubleArgument arg )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void insertString( int i, StringArgument arg )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void insertString( int i, int j, StringArgument arg )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public abstract String toXML();

    public static Argument fromXML( Element e ) throws DeserializeException
    {
        Argument arg = null;

        String type = e.getAttribute( "type" );

        if ( type.compareToIgnoreCase( "integer" ) == 0 )
        {
            arg = IntegerArgument.fromXML( e );
        }
        else if ( type.compareToIgnoreCase( "double" ) == 0 )
        {
            arg = DoubleArgument.fromXML( e );
        }
        else if ( type.compareToIgnoreCase( "string" ) == 0 )
        {
            arg = StringArgument.fromXML( e );
        }
        else
        {
            throw (new DeserializeException( "Unknown metadata type" ));
        }

        return (arg);
    }
}