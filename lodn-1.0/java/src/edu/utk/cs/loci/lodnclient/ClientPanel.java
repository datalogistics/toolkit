/*
 * Created on Jan 23, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnclient;

import javax.swing.JPanel;
import com.jgoodies.forms.layout.*;
import com.jgoodies.forms.builder.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class ClientPanel extends JPanel
{

    public ClientPanel( LoDNClient client )
    {
        FormLayout layout = new FormLayout(
            "3dlu,right:max(40dlu;p),3dlu,p,3dlu,p,3dlu,p,3dlu",
            "3dlu,p,3dlu,p, 3dlu,p,3dlu,p, 9dlu,p,3dlu,p,3dlu,p,3dlu,p,3dlu,p,    9dlu,p,3dlu,p,3dlu,p,   9dlu,p,3dlu" );

        layout.setRowGroups( new int[][]
            {
                { 4, 8, 10 } } );

        PanelBuilder builder = new PanelBuilder( this, layout );
        CellConstraints cc = new CellConstraints();
        builder.addSeparator( "Input File", cc.xywh( 2, 2, 7, 1 ) );
        builder.add( client.inputFileField, cc.xywh( 2, 4, 5, 1 ) );
        builder.add( client.inputBrowseButton, cc.xy( 8, 4 ) );
        builder.addSeparator( "Output File", cc.xywh( 2, 6, 7, 1 ) );
        builder.add( client.outputFileField, cc.xywh( 2, 8, 5, 1 ) );
        builder.add( client.outputBrowseButton, cc.xy( 8, 8 ) );

        //--------------------------------------------------
//        builder.addSeparator( "Connection Type", cc.xyw( 2, 10, 7 ) );
//
//        builder.add( client.dialupButton, cc.xy( 6, 12 ) );
//        builder.add( client.dslcableButton, cc.xy( 6, 14 ) );
//        builder.add( client.t3Button, cc.xy( 6, 16 ) );
//        builder.add( client.highspeedButton, cc.xy( 6, 18 ) );

        //--------------------------------------------------
        builder.addSeparator( "Advanced Parameters", cc.xywh( 2, 20, 7, 1 ) );
        builder.add( client.connectionsLabel, cc.xy( 2, 22 ) );
        builder.add( client.connectionsCombo, cc.xy( 4, 22 ) );
        
// We'll re -add this after i can make it work        
        builder.add( client.closeAfterFinishedCheck, cc.xy( 2, 24 ) );
//--------------------------------------------
        
//        builder.add( client.sizeLabel, cc.xy( 2, 24 ) );
//        builder.add( client.sizeField, cc.xy( 4, 24 ) );
//        builder.add( client.kiloButton, cc.xy( 6, 24 ) );
//        builder.add( client.megaButton, cc.xy( 8, 24 ) );
        builder.add( client.downloadButton, cc.xy( 4, 26 ) );
//        builder.add( client.executeButton, cc.xy( 6, 26 ) );
        builder.add( client.cancelButton, cc.xy( 8, 26 ) );
    }
}