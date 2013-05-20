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
public class ListMetadata extends Metadata
{
    public ListMetadata( String name )
    {
        this.name = name;
        this.value = new HashMap();
    }

    public void add( Metadata md )
    {
        HashMap map = (HashMap) value;
        map.put( md.getName(), md );
    }

    public void remove( String name )
    {
        HashMap map = (HashMap) value;
        map.remove( name );
    }

    public Metadata getChild( String name )
    {
        HashMap map = (HashMap) value;
        return ((Metadata) map.get( name ));
    }

    public Iterator getChildren()
    {
        HashMap map = (HashMap) value;
        List list = new ArrayList();
        Set keys = map.keySet();
        Iterator i = keys.iterator();
        String name;
        while ( i.hasNext() )
        {
            name = (String) i.next();
            list.add( getChild( name ) );
        }
        return (list.iterator());
    }

    public String toString()
    {
        StringBuffer buf = new StringBuffer();

        buf.append( "Metadata(" + name + ",LIST," + "(" );

        Iterator i = getChildren();
        Metadata md;
        while ( i.hasNext() )
        {
            md = (Metadata) i.next();
            buf.append( md.toString() + "," );
        }

        buf.append( "))" );

        return (buf.toString());
    }

    public String toXML()
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<exnode:metadata name=\"" + name + "\" type=\"meta\">\n" );

        Iterator i = getChildren();
        Metadata child;
        while ( i.hasNext() )
        {
            child = (Metadata) i.next();
            sb.append( child.toXML() );
        }

        sb.append( "</exnode:metadata>\n" );

        return (sb.toString());
    }

    public static Metadata fromXML( Element e ) throws DeserializeException
    {
        String name = e.getAttribute( "name" );

        ListMetadata md = new ListMetadata( name );

        Metadata child;
        NodeList children = e.getElementsByTagNameNS(
            Exnode.namespace, "metadata" );
        for ( int i = 0; i < children.getLength(); i++ )
        {
            child = Metadata.fromXML( (Element) children.item( i ) );
            md.add( child );
        }

        return (md);
    }
}