/*
 * Created on Nov 21, 2003
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

import java.util.*;
import org.w3c.dom.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public abstract class Metadata
{
    Object value;
    String name;

    public String getName()
    {
        return (name);
    }

    public Long getInteger() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Double getDouble() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public String getString() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Iterator getChildren() throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public Metadata getChild( String name )
        throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void add( Metadata child ) throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public void remove( String name ) throws UnsupportedOperationException
    {
        throw (new UnsupportedOperationException());
    }

    public String toString()
    {
        return (new String( "Metadata(" + name + ")" ));
    }

    public abstract String toXML();

    public static Metadata fromXML( Element e ) throws DeserializeException
    {
        Metadata md = null;

        String type = e.getAttribute( "type" );

        if ( type.compareToIgnoreCase( "integer" ) == 0 )
        {
            md = IntegerMetadata.fromXML( e );
        }
        else if ( type.compareToIgnoreCase( "double" ) == 0 )
        {
            md = DoubleMetadata.fromXML( e );
        }
        else if ( type.compareToIgnoreCase( "string" ) == 0 )
        {
            md = StringMetadata.fromXML( e );
        }
        else if ( type.compareToIgnoreCase( "meta" ) == 0 )
        {
            md = ListMetadata.fromXML( e );
        }
        else
        {
            throw (new DeserializeException( "Unknown metadata type" ));
        }

        return (md);
    }
}