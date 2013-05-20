/*
 * Created on Mar 1, 2004
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
public class ProgressOKAction extends AbstractAction
{

    private JDialog dialog;
    private LoDNPublisher pub;

    public ProgressOKAction( JDialog dialog, LoDNPublisher pub )
    {
        super( "OK" );
        this.dialog = dialog;
        this.pub = pub;
    }

    public void actionPerformed( ActionEvent e )
    {
        dialog.dispose();
        pub.cancelButton.setText( "Exit" );
    }
}