/* $Id: LogisticalDownload.java,v 1.4 2008/05/24 22:25:51 linuxguy79 Exp $ */

package edu.utk.cs.loci.cli;

import java.awt.event.*;
import javax.swing.*;
import java.io.*;
import java.util.*;
import java.net.*;

import edu.utk.cs.loci.exnode.*;
import edu.utk.cs.loci.lbone.*;
import edu.utk.cs.loci.transfer.DepotScore;
import edu.utk.cs.loci.transfer.Scoreboard;
import edu.utk.cs.loci.transfer.TransferStat;
import edu.utk.cs.loci.ibp.*;

public class LogisticalDownload
{
    public static Log DEBUG = new Log( true );

    final static String VERSION = "0.01a";

    final static int DFLT_CONNECTIONS = 1;

    Exnode exnode = null;

    public LogisticalDownload()
    {
    }

    /**
     * 
     */
    private void printScoreboard()
    {
        DEBUG.println( "Scoreboard: " );
        ReadJob.DEBUG.setOn( true );
        Scoreboard scoreboard = ReadJob.getScoreboard();
        Collection c = scoreboard.getDepotScores();
        for ( Iterator i = c.iterator(); i.hasNext(); )
        {
            DepotScore depotScore = (DepotScore) i.next();
            TransferStat depotStat = depotScore.getDepotStat();
            ReadJob.displayDepotStat( depotStat );
        }
    }

    public void download( String xndfile, String outputfilename,
        int connections, int transferSize, int maxDepotThreads, boolean withGUI )
    {
        try
        {
            long startTime = System.currentTimeMillis();

            if ( maxDepotThreads <= 0 )
            {
                ReadJob.MAX_DEPOT_THREADS = connections;
            }
            else
            {
                ReadJob.MAX_DEPOT_THREADS = maxDepotThreads;
            }

            System.out.println( "Downloading " + xndfile + ", to "
                + outputfilename + " using " + connections
                + " connections, with max " + ReadJob.MAX_DEPOT_THREADS
                + " connections per depot" );

            exnode = Exnode.fromURI( xndfile );
            long length = exnode.getLength();
            exnode.progress = new Progress( length );

            SyncFile syncFile;
            if ( outputfilename.equals( "/dev/null" ) )
            {
                syncFile = new NullSyncFile( "/dev/null", length );
            }
            else
            {
                syncFile = new PhysicalSyncFile( outputfilename );
            }
            
            ProgressDialog progressDialog = null;
            if ( withGUI )
            {
                progressDialog = new ProgressDialog( exnode );
                progressDialog.setTitle( "LoDN Download: " + outputfilename );
                progressDialog.setVisible( true );
                ActionListener exitActionListener = new ActionListener()
                    {
                        public void actionPerformed( ActionEvent ae )
                        {
                            System.out.println( "Operation ended by user pressing "
                                + ((JButton) ae.getSource()).getText()
                                + " button" );
                            System.exit( 0 );
                        }
                    };
                progressDialog.addOKActionListener( exitActionListener );
                progressDialog.addCancelActionListener( exitActionListener );
            }

            exnode.read( syncFile, 0, length, transferSize, connections );

            long elapsedTime = System.currentTimeMillis() - startTime;

            System.out.println( "DONE in " + elapsedTime + " ms = "
                + ((double) length / (elapsedTime * 1e3)) + "MB/s" );

            this.printScoreboard();

            exnode.progress.setDone();
            exnode.progress.fireProgressChangedEvent();
            exnode.progress.fireProgressDoneEvent();

            if ( withGUI )
            {
                progressDialog.setVisible( false );
                System.exit( 0 );
            }
        }
        catch ( NullPointerException e )
        {
            e.printStackTrace();
            DEBUG.error( "Please specify an input" );
        }
        catch ( Exception e )
        {
            System.out.println( "Unable to complete read operation: "
                + e.getMessage() );
            e.printStackTrace();

        }
    }

    public static void usage()
    {

        System.out.println( "LogisticalDownload v." + VERSION + " (LoCI 2004)" );
        System.out.println( "usage: java LogisticalDownload [OPTION] XND-file" );

        String textoptions = ""
            + "  -f\tSpecify the output filename by removing .xnd to\n"
            + "    \tthe exnode filename.\n"
            + "  -t\tSpecify the maximum number of threads to use to\n"
            + "    \tperform Download.\n"
            + "  -b\tSpecify the maximum transfer block size to use (in KB).\n"
            + "  -m\tSpecify the maximum number of threads to use PER DEPOT to\n"
            + "    \tperform Download.\n" 
            + "  -g\tShow Progress GUI.\n";

        System.out.println( textoptions );

    }

    public static void main( String[] args )
    {
        boolean sameoutput = false;
        int connections = DFLT_CONNECTIONS;
        int transferSize = -1;
        int maxDepotThreads = 0;
        boolean withGUI = false;
        boolean nullFile = false;

        String outputfilename = "";
        String inputfilename = "";

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
                    case 'o':
                        outputfilename = args[i + 1];
                        i++;
                        break;
                    case 'b':
                        transferSize = Integer.parseInt( args[i + 1] ) * 1024;
                        i++;
                        break;
                    case 't':
                        connections = Integer.parseInt( args[i + 1] );
                        i++;
                        break;
                    case 'm':
                        maxDepotThreads = Integer.parseInt( args[i + 1] );
                        i++;
                        break;
                    case 'g':
                        withGUI = true;
                        break;
                    case 'T':
                        i++;
                        break;
                }
            }
            else
            {
                inputfilename = args[i];
            }
        }

        if ( sameoutput || outputfilename.equals( "" ) )
        {
            // delete extension (.xnd)
            outputfilename = inputfilename.substring(
                0, inputfilename.length() - 4 );
        }

        LogisticalDownload ld = new LogisticalDownload();
        ld.download(
            inputfilename, outputfilename, connections, transferSize,
            maxDepotThreads, withGUI );

    }
}