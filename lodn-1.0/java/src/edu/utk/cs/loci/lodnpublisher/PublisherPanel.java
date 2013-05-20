/*
 * Created on Feb 27, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnpublisher;

import javax.swing.JOptionPane;
import javax.swing.JPanel;
import com.jgoodies.forms.layout.*;
import com.jgoodies.forms.builder.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class PublisherPanel extends JPanel
{

    public PublisherPanel( LoDNPublisher pub )
    {
        FormLayout layout = new FormLayout(
            "3dlu, p,3dlu, p,3dlu, p,3dlu, p,3dlu", /* columns */
            "3dlu," + /* rows */
            "p,3dlu, p,3dlu," + "p,3dlu, p,9dlu,"
                + "p,3dlu, p,3dlu, p,3dlu, p,3dlu, p, 3dlu, p,9dlu,"
                + "p,3dlu, p,3dlu, p,3dlu, p,3dlu, p,9dlu,"
                + "p,3dlu, p,3dlu, p,3dlu, p,3dlu, p,3dlu, p,3dlu," + "p,3dlu" );
        //right:max(40dlu;p)
        PanelBuilder builder = new PanelBuilder( this, layout );
        CellConstraints cc = new CellConstraints();

        //--------------------------------------------------
        builder.addSeparator( "Input File", cc.xyw( 2, 2, 7 ) );

        builder.add( pub.inputFileField, cc.xyw( 2, 4, 5 ) );
        builder.add( pub.inputBrowseButton, cc.xy( 8, 4 ) ); // Browse

        //--------------------------------------------------
        builder.addSeparator( "Output File", cc.xyw( 2, 6, 7 ) );

        builder.add( pub.outputFileField, cc.xyw( 2, 8, 5 ) );

        //--------------------------------------------------
        builder.addSeparator( "Location and Connection Type", cc.xyw( 2, 10, 7 ) );

        builder.add( pub.locationLabel1, cc.xy( 2, 12 ) );
        builder.add( pub.locationPrefixCombo, cc.xy( 4, 12 ) );
        builder.add( pub.locationLabel2, cc.xy( 2, 14 ) );
        builder.add( pub.locationField, cc.xy( 4, 14 ) );

        builder.add( pub.locationHintsLabel, cc.xywh( 2, 16, 3, 3 ) );

        builder.add( pub.dialupButton, cc.xy( 6, 12 ) );
        builder.add( pub.dslcableButton, cc.xy( 6, 14 ) );
        builder.add( pub.t3Button, cc.xy( 6, 16 ) );
        builder.add( pub.highspeedButton, cc.xy( 6, 18 ) );

        //--------------------------------------------------
        builder.addSeparator( "Advanced Parameters", cc.xyw( 2, 20, 7 ) );

        builder.add( pub.connectionsLabel, cc.xy( 2, 22 ) );
        builder.add( pub.connectionsCombo, cc.xy( 4, 22 ) );

        builder.add( pub.sizeLabel, cc.xyw( 2, 24, 3 ) );
        builder.add( pub.sizeField, cc.xy( 4, 24 ) );

        builder.add( pub.kiloButton, cc.xy( 6, 24 ) ); // KB
        builder.add( pub.megaButton, cc.xy( 8, 24 ) ); // MB

        builder.add( pub.copiesLabel, cc.xy( 2, 26 ) );
        builder.add( pub.copiesCombo, cc.xy( 4, 26 ) );

        builder.add( pub.depotsLabel, cc.xyw( 2, 28, 3 ) );
        builder.add( pub.depotsField, cc.xy( 4, 28 ) );

        builder.add( pub.lboneLabel, cc.xyw(2, 30, 3));
        builder.add( pub.lboneCheckBox, cc.xy(4, 30));
        
        //--------------------------------------------------
        builder.addSeparator( "Extra services", cc.xyw( 2, 32, 7 ) );

        builder.add( pub.noneButton, cc.xy( 2, 34 ) ); // None
        builder.add( pub.xorButton, cc.xy( 2, 36 ) ); // XOR
        builder.add( pub.aesButton, cc.xy( 2, 38 ) ); // AES
        builder.add( pub.md5Button, cc.xy( 2, 40 ) ); // MD5
        builder.add( pub.zipButton, cc.xy( 2, 42 ) ); // ZIP

        builder.add( pub.extraservicesLabel, cc.xywh( 4, 36, 3, 5 ) );

        builder.add( pub.uploadButton, cc.xy( 6, 44 ) ); // Upload
        builder.add( pub.cancelButton, cc.xy( 8, 44 ) ); // Cancel
        
        
        /***----  Sun Java 1.6 < 1.6.0_03 bug fix ---***/
        
        if(pub.badJVMversion)
        {
        	JOptionPane.showMessageDialog(this, 
        		"LoDNPublisher has detected that the WebStart Runtime Environment \n" +
        		"is using an early version of Sun's implementation of Java 1.6.\n" +
        		"There is a documented bug with its SSL sockets (SDN Bug #6514454)\n" +
        		"that may prevent this application from running correctly.\n\n" +
        		"Please consider upgrading to a newer version of Java 1.6 or\n" +
        		"downgrading webstart to 1.4.2 or 1.5.", 
        		"Java 1.6 Warning Message", JOptionPane.WARNING_MESSAGE);
        }
    }
}