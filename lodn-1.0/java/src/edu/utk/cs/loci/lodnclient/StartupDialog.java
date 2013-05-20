package edu.utk.cs.loci.lodnclient;

import javax.swing.*;
import com.jgoodies.forms.layout.*;
import com.jgoodies.forms.builder.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class StartupDialog extends JDialog
{

    private JLabel label;

    public StartupDialog()
    {
        this( -1 );
    }

    public StartupDialog( int i )
    {
        if ( i == -1 )
            label = new JLabel( "Retrieving Exnode..." );
        else
            label = new JLabel( "Retrieving Exnode #" + i + "..." );

        JPanel panel = new JPanel();
        label.setVisible( true );

        JProgressBar bar = new JProgressBar();
        bar.setIndeterminate( true );
        bar.setVisible( true );

        FormLayout layout = new FormLayout( "3dlu,p,3dlu", "3dlu,p,3dlu,p,3dlu" );
        PanelBuilder builder = new PanelBuilder( panel, layout );
        CellConstraints cc = new CellConstraints();
        builder.add( label, cc.xy( 2, 2 ) );
        builder.add( bar, cc.xy( 2, 4 ) );

        setContentPane( panel );
        pack();
    }
}