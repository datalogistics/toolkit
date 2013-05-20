/* $Id: LogisticalUpload.java,v 1.4 2008/05/24 22:25:51 linuxguy79 Exp $ */

package edu.utk.cs.loci.cli;

import java.io.*;
import java.util.*;
import java.net.*;

import edu.utk.cs.loci.exnode.*;
import edu.utk.cs.loci.lbone.*;
import edu.utk.cs.loci.ibp.*;

public class LogisticalUpload
{

    final static String VERSION = "0.01a";
    final static String LBONE_SERVER_LIST = "vertex.cs.utk.edu:6767 "
        + "acre.sinrg.cs.utk.edu:6767 " + "galapagos.cs.utk.edu:6767";

    final static int DFLT_COPIES = 1;
    final static int DFLT_MAX_DEPOTS = 10;
    final static int DFLT_DURATION = 60 * 60 * 24; // 1 day
    final static int DFLT_TRANSFERSIZE = 512 * 1024;
    final static int DFLT_CONNECTIONS = 1;
    final static String DFLT_LOCATION = "state= TN";

    final static int LBONE_SERVER_NBMAX = 10;

    private static int VERBOSE = 1;

    private LboneServer servers[];

    public Exnode exnode = null;
    String homeDirectory;
    private String user;
    private String pass;

    public LogisticalUpload()
    {
        exnode = new Exnode();
    } // this limits upload to 1 file only per instance!

    // replace characters not supported (TODO)
    private String cleanName( String name )
    {
        final char[] chars = name.toCharArray();
        for ( int x = 0; x < chars.length; x++ )
        {
            final char c = chars[x];
            if ( ((c >= 'a') && (c <= 'z')) )
                continue; // a - z
            if ( ((c >= 'A') && (c <= 'Z')) )
                continue; // A - Z
            if ( ((c >= '0') && (c <= '9')) )
                continue; // 0 - 9
            if ( c == '-' )
                continue; // hyphen
            if ( c == '.' )
                continue; // dot
            chars[x] = '_'; // if not replaced by underscore
        }
        return String.valueOf( chars );
    }

    public void fill_LBoneServerList( String lboneAddr, int lbonePort )
    {
        int i = 0;
        servers = new LboneServer[LBONE_SERVER_NBMAX + 1];

        if ( lboneAddr.compareTo( "" ) != 0 )
        {
            servers[i] = new LboneServer( lboneAddr, lbonePort );
            i++;
        }

        StringTokenizer st = new StringTokenizer( LBONE_SERVER_LIST );
        while ( st.hasMoreTokens() && i < LBONE_SERVER_NBMAX )
        {
            String entry = st.nextToken().trim();
            StringTokenizer ste = new StringTokenizer( entry, ":" );
            String hostname = ste.nextToken();
            int port = Integer.parseInt( ste.nextToken() );
            servers[i] = new LboneServer( hostname, port );
            i++;
        }
    }

    public void upload( File inputfile, String outputfilename, int copies,
        int maxDepots, int duration, int transferSize, int connections,
        String location )
    {
        final String functionName = null;
        final int mbPerDepot = (int) ((inputfile.length() * copies) / maxDepots
            / 1024 / 1024 + 1);

        final int maxDepots_f = maxDepots;
        final int duration_f = duration;
        final String location_f = location;
        final File inputfile_f = inputfile;
        final String outputfilename_f = outputfilename;
        final int transferSize_f = transferSize;
        final int connections_f = connections;
        final int copies_f = copies;

        Thread t = new Thread( new Runnable()
            {
                public void run()
                {

                    try
                    {
                        boolean done = false;

                        // ======================================================

                        Depot[] depots = null;
                        for ( int i = 0; i < servers.length && done == false; i++ )
                        {
                            try
                            {
                                depots = servers[i].getDepots(
                                    maxDepots_f, mbPerDepot, // hard
                                    0, // soft
                                    duration_f, location_f, 120 );
                                done = true;
                            }
                            catch ( Exception ignored )
                            {
                                System.err.println( "upload: " + ignored );
                            }
                        }
                        if ( done == false )
                        {
                            throw (new Exception( "Unable to contact L-Bone" ));
                        }

                        DepotList list = new DepotList();
                        list.add( depots );
                        list.randomizeNext(); // Avoid to start always on the same
                        // depot especially if user always
                        // use the same location.

                        if ( VERBOSE > 0 )
                        {
                            System.out.println( "Response from the L-Bone:" );
                            System.out.println( list.toString() );
                        }

                        // ======================================================

                        System.out.println( "Uploading "
                            + inputfile_f.getCanonicalPath() + "..." );

                        exnode.write(
                            inputfile_f.getCanonicalPath(), transferSize_f,
                            connections_f, duration_f, copies_f, list,
                            functionName );

                        // ======================================================

                        System.out.print( "Building " + outputfilename_f
                            + "... " );

                        exnode.addMetadata( new StringMetadata(
                            "filename", outputfilename_f.substring(
                                outputfilename_f.lastIndexOf( '/' ) + 1,
                                outputfilename_f.length() - 4 ) ) );

                        // SIZE_ESTIMATE
                        // -------------
                        // "original_filesize" should be named "size_estimate"
                        exnode.addMetadata( new IntegerMetadata(
                            "original_filesize", inputfile_f.length() ) );

                        exnode.addMetadata( new IntegerMetadata(
                            "number_of_copies", copies_f ) );

                        exnode.addMetadata( new DoubleMetadata(
                            "lorsversion", 0.82 ) );

                        Metadata type = new ListMetadata( "Type" );
                        type.add( new StringMetadata( "Name", "logistical_file" ) );
                        type.add( new StringMetadata( "Version", "1.0" ) );
                        exnode.addMetadata( type );

                        Metadata tool = new ListMetadata( "Authoring Tool" );
                        tool.add( new StringMetadata( "Name", "LoDN" ) );
                        tool.add( new StringMetadata( "Version", "0.7" ) );
                        exnode.addMetadata( tool );

                        System.out.println( "done." );

                        // ======================================================

                        System.out.print( "Writing " + outputfilename_f
                            + "... " );

                        FileOutputStream fos = new FileOutputStream(
                            outputfilename_f );
                        fos.write( exnode.toXML().getBytes() );
                        fos.close();

                        System.out.println( "done." );

                    }
                    catch ( Exception e )
                    {
                        e.printStackTrace();
                        exnode.setError( e );
                        exnode.setCancel( true );
                    }
                }
            } );
        t.start();

        while ( exnode.getProgress() == null )
        {
            try
            {
                this.wait( 1000 );
                System.out.println( "Progress object not yet initailized... Please wait..." );
            }
            catch ( Exception ignored )
            {
            }
        }
    }

    public static void usage()
    {
        System.out.println( "LogisticalUpload v." + VERSION + " (LoCI 2004)" );
        System.out.println( "usage: java LogisticalUpload [OPTION] SOURCE" );

        String textoptions = ""
            + "  -f\tSpecify the exNode filename by appending .xnd to the original filename.\n"
            + "  -c\tSpecify the number of copies.\n"
            + "  -o\tSpecify a specific name for the output exNode file.\n"
            + "  -m\tSpecify the maximum number of depots returned by the 'location' hint.\n"
            + "  -d\tSpecify the amount of time to allocate storage. The available modifiers\n"
            + "    \tare 'm' 'h' 'd' for 'minute' 'hour' and 'days' respectively. Default is\n"
            + "    \tseconds. (e.g. -d 1d or -d 1h)\n"
            + "  -b\tSpecify the logical data blocksize of input file. (e.g. -b 512k or -b 1M)\n"
            + "  -t\tSpecify the maximum number of threads to use to perform Upload.\n"
            + "  -l\tSpecify a location hint to pass the L-Bone Query. (e.g. -l \"state= TN\")\n";

        System.out.println( textoptions );
    }

    public static void main( String[] args )
    {

        boolean sameoutput = false;

        int copies = DFLT_COPIES;
        int maxDepots = DFLT_MAX_DEPOTS;
        int duration = DFLT_DURATION;
        int transferSize = DFLT_TRANSFERSIZE;
        int connections = DFLT_CONNECTIONS;
        String location = DFLT_LOCATION;

        String outputfilename = "";
        String inputfilename = "";

        String str = "";

        if ( args.length == 0 )
        {
            usage();
            System.exit( 0 );
        }

        for ( int i = 0; i < args.length; i++ )
        {
            String arg = args[i];
            if ( (char) arg.charAt( 0 ) == '-' )
            {
                switch ( (char) arg.charAt( 1 ) )
                {
                    case 'f':
                        sameoutput = true;
                        break;
                    case 'H':
                        i++;
                        break;
                    case 'c':
                        copies = Integer.parseInt( args[i + 1] );
                        i++;
                        break;
                    case 'o':
                        outputfilename = args[i + 1];
                        i++;
                        break;
                    case 'm':
                        maxDepots = Integer.parseInt( args[i + 1] );
                        i++;
                        break;
                    case 'n':
                        break;
                    case 'l':
                        location = args[i + 1] + " " + args[i + 2];
                        i += 2;
                        break;
                    case 'h':
                        break;
                    case 's':
                        break;
                    case 't':
                        connections = Integer.parseInt( args[i + 1] );
                        i++;
                        break;
                    case 'T':
                        i++;
                        break;
                    case 'd':
                        try
                        {
                            str = args[i + 1].toLowerCase();
                            int p = str.length();
                            if ( str.endsWith( "m" ) )
                            {
                                duration = Integer.parseInt( str.substring(
                                    0, p - 1 ) ) * 60;
                            }
                            else if ( str.endsWith( "h" ) )
                            {
                                duration = Integer.parseInt( str.substring(
                                    0, p - 1 ) ) * 3660;
                            }
                            else if ( str.endsWith( "d" ) )
                            {
                                duration = Integer.parseInt( str.substring(
                                    0, p - 1 ) ) * 86400;
                            }
                            else
                            {
                                duration = Integer.parseInt( str );
                            }
                        }
                        catch ( Exception e )
                        {
                            System.out.println( "Bad number: " + str );
                        }
                        i++;
                        break;
                    case 'b':

                        try
                        {
                            str = args[i + 1].toLowerCase();
                            int p = str.length();
                            if ( str.endsWith( "k" ) )
                            {
                                transferSize = Integer.parseInt( str.substring(
                                    0, p - 1 ) ) * 1024;
                            }
                            else if ( str.endsWith( "m" ) )
                            {
                                transferSize = Integer.parseInt( str.substring(
                                    0, p - 1 ) ) * 1024 * 1024;
                            }
                            else
                            {
                                transferSize = Integer.parseInt( str );
                            }
                            i++;
                        }
                        catch ( Exception e )
                        {
                            System.out.println( "Bad number: " + str );
                        }
                }
            }
            else
            {
                inputfilename = args[i];
            }
        }

        File inputfile = new File( inputfilename );
        if ( !inputfile.exists() || !inputfile.isFile() )
        {
            System.out.println( "Can not read " + inputfilename );
            System.exit( 1 );
        }

        if ( sameoutput || outputfilename.equals( "" ) )
        {
            outputfilename = inputfilename + ".xnd";
        }

        if ( VERBOSE > 0 )
        {
            String info = "" + "File to upload:\t\t" + inputfilename + "\n"
                + "Output file:\t\t" + outputfilename + "\n"
                + "Number of copies:\t" + copies + "\n"
                + "Max depots requested:\t" + maxDepots + "\n"
                + "Duration:\t\t" + duration + " sec.\n" + "Transfer size:\t\t"
                + transferSize + " bytes\n" + "Number of connections:\t"
                + connections + "\n" + "Location:\t\t" + location + "\n";

            System.out.println( info );
        }

        LogisticalUpload lu = new LogisticalUpload();
        lu.fill_LBoneServerList( "", 0 );
        lu.upload(
            inputfile, outputfilename, copies, maxDepots, duration,
            transferSize, connections, location );
    }
}