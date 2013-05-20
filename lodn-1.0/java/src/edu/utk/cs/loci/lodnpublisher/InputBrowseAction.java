/*
 * Created on Feb 27, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnpublisher;

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

    private LoDNPublisher pub;

    public InputBrowseAction( LoDNPublisher pub )
    {
        super( "Browse" );
        this.pub = pub;
    }

    public void actionPerformed( ActionEvent e )
    {
        JFileChooser chooser = new JFileChooser();

        chooser.setCurrentDirectory( pub.currentDirectory );
        int result = chooser.showOpenDialog( null );
        pub.currentDirectory = chooser.getCurrentDirectory();

        if ( result == JFileChooser.APPROVE_OPTION )
        {
            pub.setInputFileName( chooser.getSelectedFile().getPath() );
            pub.setOutputFileName( chooser.getSelectedFile().getName() );
        }
    }

}

// TODO: multi-selection
// JFileChooser chooser = new JFileChooser();

// // Enable multiple selections
// chooser.setMultiSelectionEnabled(true);

// // Show the dialog; wait until dialog is closed
// chooser.showOpenDialog(frame);

// // Retrieve the selected files. This method returns empty
// // if multiple-selection mode is not enabled.
// File[] files = chooser.getSelectedFiles();

// e891. Getting and Setting the Current Directory of a JFileChooser Dialog

//     JFileChooser chooser = new JFileChooser();

//     try {
//         // Create a File object containing the canonical path of the
//         // desired directory
//         File f = new File(new File(".").getCanonicalPath());

//         // Set the current directory
//         chooser.setCurrentDirectory(f);
//     } catch (IOException e) {
//     }

//     // The following method call sets the current directory to the home directory
//     chooser.setCurrentDirectory(null);

//     // Show the dialog; wait until dialog is closed
//     chooser.showOpenDialog(frame);

//     // Get the current directory
//     File curDir = chooser.getCurrentDirectory();
