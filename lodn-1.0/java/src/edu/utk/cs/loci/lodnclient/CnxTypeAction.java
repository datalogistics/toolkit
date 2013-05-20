package edu.utk.cs.loci.lodnclient;

import java.awt.event.ActionEvent;
import javax.swing.AbstractAction;

public class CnxTypeAction extends AbstractAction
{

    private LoDNClient client;

    public CnxTypeAction( LoDNClient client )
    {
        this.client = client;
    }

    public void actionPerformed( ActionEvent e )
    {
        String ac = e.getActionCommand();

        if ( ac.equals( client.dialupString ) )
        {
            client.connectionsCombo.setSelectedIndex( 0 );
            client.sizeField.setText( "128" );
            client.kiloButton.setSelected( true );

        }
        else if ( ac.equals( client.dslcableString ) )
        {
            client.connectionsCombo.setSelectedIndex( 1 );
            client.sizeField.setText( "512" );
            client.kiloButton.setSelected( true );

        }
        else if ( ac.equals( client.t3String ) )
        {
            client.connectionsCombo.setSelectedIndex( 2 );
            client.sizeField.setText( "1024" );
            client.megaButton.setSelected( true );

        }
        else if ( ac.equals( client.highspeedString ) )
        {
            client.connectionsCombo.setSelectedIndex( 3 );
            client.sizeField.setText( "2" );
            client.megaButton.setSelected( true );

        }
    }

}