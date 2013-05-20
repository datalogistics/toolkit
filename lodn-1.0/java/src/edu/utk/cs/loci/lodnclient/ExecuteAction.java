/* $Id: ExecuteAction.java,v 1.4 2008/05/24 22:25:53 linuxguy79 Exp $ */

package edu.utk.cs.loci.lodnclient;

import java.awt.event.ActionEvent;
import java.io.IOException;
import javax.swing.*;
import edu.utk.cs.loci.exnode.*;

public class ExecuteAction extends AbstractAction implements ProgressListener
{

    final static boolean DBG = false;

    private LoDNClient client;

    public ExecuteAction( LoDNClient client )
    {
        super( "Execute" );
        this.client = client;
    }

    public void actionPerformed( ActionEvent e )
    {
        int okcancel = JOptionPane.showConfirmDialog(
            null, "Do you really want to execute this file once downloaded ?",
            "Warning", JOptionPane.OK_CANCEL_OPTION );
        if ( okcancel == JOptionPane.CANCEL_OPTION )
            return;

        client.downloadAction.actionPerformed( e );
        client.exnodes[0].addProgressListener( this );

    }

    private void error( String msg )
    {
        JOptionPane.showMessageDialog(
            null, msg, "Error", JOptionPane.ERROR_MESSAGE );
    }

    public void progressUpdated( ProgressEvent e )
    {
        if ( DBG )
            System.out.println( "ExecuteAction: File download in progress!" );
    }

    public void progressDone( ProgressEvent e )
    {

        if ( DBG )
            System.out.println( "ExecuteAction: File download completed!" );

        String outputFile = client.outputFileField.getText();

        String os = System.getProperty( "os.name" );

        Runtime r = Runtime.getRuntime();

        if ( os.indexOf( "Mac" ) != -1 )
        {
            try
            {
                r.exec( "/usr/bin/open " + outputFile );
            }
            catch ( IOException e1 )
            {
                error( e1.getMessage() );
            }
        }
        else if ( os.indexOf( "Windows" ) != -1 )
        {
            try
            {
                String outputPath = outputFile.substring(
                    0, outputFile.lastIndexOf( '\\' ) );
                String file = outputFile.substring( outputFile.lastIndexOf( '\\' ) + 1 );
                r.exec( "cmd /c start /d \"" + outputPath + "\" " + file );
            }
            catch ( IOException e2 )
            {
                e2.printStackTrace();
                error( e2.getMessage() );
            }
        }
        else
        {
            //  	    try {
            // 		String cmdx = "/bin/sh -c \"mplayer " + outputFile + "\"";
            // 		System.out.println("run -> " + cmdx);
            //  		r.exec(cmdx);
            //  	    } catch (IOException e3) {
            //  		e3.printStackTrace();
            //  	    }
        }
    }

    public void progressError( ProgressEvent e )
    {
        if ( DBG )
            System.out.println( "ExecuteAction: File download error!" );
    }

}