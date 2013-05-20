/*
 * Created on Jan 23, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnclient;

import java.awt.event.ActionEvent;
import javax.swing.AbstractAction;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class CancelAction extends AbstractAction
{
    private LoDNClient client;

    public CancelAction( LoDNClient client )
    {
        super( "Exit" );
        this.client = client;
    }

    public CancelAction()
    {
        super( "Exit" );
    }

    public void actionPerformed( ActionEvent e )
    {
        System.exit( 0 );
    }

}