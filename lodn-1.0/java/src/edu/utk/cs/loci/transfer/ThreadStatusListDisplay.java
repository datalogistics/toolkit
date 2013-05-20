/*
 * Created on Feb 10, 2005
 *
 * TODO To change the template for this generated file go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
package edu.utk.cs.loci.transfer;

import java.util.*;
import javax.swing.*;

import edu.utk.cs.loci.exnode.Exnode;
import edu.utk.cs.loci.ibp.Log;

import java.awt.*;

/**
 * @author lfgs
 *
 * TODO To change the template for this generated type comment go to
 * Window - Preferences - Java - Code Style - Code Templates
 */
public class ThreadStatusListDisplay extends JFrame
{
    public static Log DEBUG = new Log( true );

    protected Exnode exnode = null;
    protected TransferThread[] threads = null;
    protected ThreadStatusPanel[] statPanels = null;
    protected JScrollPane scrollPane;
    protected JPanel statListPanel;
    protected ThreadStatusPanel header;

    public ThreadStatusListDisplay( Exnode exnode )
    {
        //DEBUG.setOn( true );

        this.exnode = exnode;
        this.threads = exnode.getThreads();

        Container c = this.getContentPane();

        c.setLayout( new BorderLayout() );
        this.header = new ThreadStatusPanel( "Thread", null );
        //        c.add( this.header, BorderLayout.NORTH );

        this.statListPanel = new JPanel();
        this.statListPanel.setLayout( new BoxLayout(
            this.statListPanel, BoxLayout.Y_AXIS ) );
        this.statListPanel.add( this.header );

        this.scrollPane = new JScrollPane( statListPanel );
        //, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED,
        //    JScrollPane.HORIZONTAL_SCROLLBAR_NEVER );

        c.add( this.scrollPane, BorderLayout.CENTER );

        DEBUG.println( "TransferStatListDisplay(): finished creating scroll pane.  Calling updateStats() ... " );

        if ( this.threads != null )
        {
            this.layoutThreadList();
            this.updateStats();
        }
        this.setSize( 800, 600 );
        this.validate();
    }

    public void layoutThreadList()
    {
        this.statListPanel.removeAll();
        this.statListPanel.add( this.header );
        this.statPanels = new ThreadStatusPanel[threads.length];
        for ( int i = 0; i < this.statPanels.length; i++ )
        {
            this.statPanels[i] = new ThreadStatusPanel( "" + i, threads[i] );
            this.statListPanel.add( this.statPanels[i] );
        }

        this.statListPanel.validate();
    }

    public void updateStats()
    {
        if ( this.isVisible() )
        {
            TransferThread[] exnodeThreads = this.exnode.getThreads();
            if ( this.threads != exnodeThreads )
            {
                this.threads = exnodeThreads;
                this.layoutThreadList();
            }

            if ( this.threads != null )
            {
                for ( int i = 0; i < this.statPanels.length; i++ )
                {
                    this.statPanels[i].updateStats();
                }
            }
        }
    }

}