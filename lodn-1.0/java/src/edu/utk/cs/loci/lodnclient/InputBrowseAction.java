/*
 * Created on Feb 5, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnclient;

import java.awt.event.ActionEvent;

import javax.swing.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class InputBrowseAction extends AbstractAction
{
    private LoDNClient client;

    public InputBrowseAction( LoDNClient client )
    {
        super( "Browse" );
        this.client = client;
    }

    public void actionPerformed( ActionEvent e )
    {
        JFileChooser chooser = new JFileChooser();
        chooser.setFileFilter( new XNDFileFilter() );

        boolean done = false;
        boolean cancelled = false;
        String path = "";

        while ( !done )
        {
            int result = chooser.showOpenDialog( null );

            if ( result == JFileChooser.APPROVE_OPTION )
            {
                path = chooser.getSelectedFile().getPath();

                done = client.retrieveExnode( path );
            }
            else
            {
                path = new String( "" );
                done = true;
                cancelled = true;
            }
        }

        if ( !cancelled )
        {
            client.inputFileField.setText( path );
            client.outputFileField.setText( client.getDefaultOutputFile("") );
        }
    }
}