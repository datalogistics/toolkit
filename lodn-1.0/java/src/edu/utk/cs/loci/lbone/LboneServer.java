/* $Id: LboneServer.java,v 1.6 2008/05/24 22:25:53 linuxguy79 Exp $ */

package edu.utk.cs.loci.lbone;

import java.io.*;
import java.net.*;
import edu.utk.cs.loci.ibp.Depot;

public class LboneServer
{

    public static final String VERSION = "0.0.1";
    public static final String REQUEST_TYPE = "1";
    public static final int MESSAGE_LEN = 512;
    public static final int MAX_HOST_LEN = 256;
    public static final int FIELD_LEN = 10;
    public static final int SUCCESS = 1;
    public static final int FAILURE = 2;

    private String host;
    private int port;

    public LboneServer( String host, String port )
    {
        this.host = host;
        this.port = Integer.parseInt( port );
    }

    public LboneServer( String host, int port )
    {
        this.host = host;
        this.port = port;
    }

    public Depot[] getDepots( int numDepots, int hard, int soft, int duration,
        int timeout ) throws Exception
    {
        return (getDepots( numDepots, hard, soft, duration, "NULL", timeout ));
    }
    
    public Depot[] getDepots( int numDepots, int hard, int soft, int duration,
            String loc, Depot[] depots, int timeout ) throws Exception
    {
    	/* Declarations */
    	
    	if(depots == null)
    	{
    		depots = getDepots(numDepots, hard, soft, duration, "NULL", timeout);
    	}
    	
    	return depots;
    }

    public Depot[] getDepots( int numDepots, int hard, int soft, int duration,
        String loc, int timeout ) throws Exception
    {
        if ( hard < 0 || soft < 0 || (hard == 0 && soft == 0) )
        {
            throw (new IllegalArgumentException());
        }

        if ( loc == null )
        {
            loc = new String( "NULL" );
        }

        StringBuffer buf = new StringBuffer();
        buf.append( padField( VERSION ) );
        buf.append( padField( REQUEST_TYPE ) );
        buf.append( padField( String.valueOf( numDepots ) ) );
        buf.append( padField( String.valueOf( hard ) ) );
        buf.append( padField( String.valueOf( soft ) ) );
        buf.append( padField( String.valueOf( duration ) ) );
        buf.append( loc );
        String request = padRequest( buf.toString() );

        Socket s = null;
        try
        {
            s = new Socket( host, port );
            s.setSoTimeout( timeout * 1000 );

            OutputStream os = s.getOutputStream();
            //InputStream is=s.getInputStream();
            BufferedReader is = new BufferedReader( new InputStreamReader(
                s.getInputStream() ) );

            //DEBUG.println("getDepots: send request...");

            os.write( request.getBytes() );

            StringBuffer response = new StringBuffer();
            //int data=is.read();
            //while(data!=-1) {
            //while(is.available() > 0) {
            //response.append((char)is.read());
            //    response.append((char)data);
            //    data=is.read();
            //}
            response.append( is.readLine() );
            //DEBUG.println("getDepots: response= " + response.toString());

            int returnCode = Integer.parseInt( response.substring( 0, 10 ).trim() );

            if ( returnCode == FAILURE )
            {
                String error = response.substring( 10, 20 );
                throw (new Exception( "LBone Error: " + error ));
            }
            else
            {
                return (parseDepotList( response.substring( 10 ) ));
            }
        }
        finally
        {
            if ( s != null )
            {
                s.close();
            }
        }
    }

    private String padField( String field )
    {
        StringBuffer buf = new StringBuffer();
        for ( int i = 0; i < FIELD_LEN - field.length(); i++ )
        {
            buf.append( ' ' );
        }
        buf.append( field );
        return (buf.toString());
    }

    private String padRequest( String request )
    {
        StringBuffer buf = new StringBuffer( request );
        for ( int i = 0; i < MESSAGE_LEN - request.length(); i++ )
        {
            buf.append( '\0' );
        }
        return (buf.toString());
    }

    private Depot[] parseDepotList( String list )
    {
        int numDepots = Integer.parseInt( list.substring( 0, 10 ).trim() );

        Depot[] depots = new Depot[numDepots];

        for ( int i = 0; i < numDepots; i++ )
        {
            int startHost = i * (MAX_HOST_LEN + FIELD_LEN) + 10;
            int endHost = startHost + MAX_HOST_LEN;
            int startPort = endHost;
            int endPort = startPort + 10;
            String host = list.substring( startHost, endHost ).trim();
            String port = list.substring( startPort, endPort ).trim();
            depots[i] = new Depot( host, port );
        }

        return (depots);
    }
}