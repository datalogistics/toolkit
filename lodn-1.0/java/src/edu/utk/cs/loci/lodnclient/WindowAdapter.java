/*
 * Created on Jan 23, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnclient;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class WindowAdapter extends java.awt.event.WindowAdapter
{

    public WindowAdapter()
    {
    }

    public void windowClosing( java.awt.event.WindowEvent e )
    {
        System.exit( 0 );
    }
}