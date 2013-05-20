/*
 * Created on Mar 9, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnclient;

import java.awt.event.ActionEvent;
import javax.swing.*;
import com.jgoodies.forms.layout.*;
import com.jgoodies.forms.builder.*;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class AboutAction extends AbstractAction
{

    public AboutAction()
    {
        super( "About" );
    }

    public void actionPerformed( ActionEvent e )
    {
        JPanel panel = new JPanel();

        FormLayout layout = new FormLayout(
            "3dlu,p,3dlu", "3dlu,p,3dlu,p,3dlu,p,3dlu,p,3dlu,p,3dlu" );

        PanelBuilder builder = new PanelBuilder( panel, layout );
        CellConstraints cc = new CellConstraints();
        builder.add( new JLabel( "LoDN Client" ), cc.xy( 2, 2 ) );
        builder.add( new JLabel( "Version 0.811" ), cc.xy( 2, 4 ) );
        builder.add(
            new JLabel(
                "Logistical Computing and Internetworking (LoCI) Laboratory" ),
            cc.xy( 2, 6 ) );
        builder.add( new JLabel( "Department of Computer Science" ), cc.xy(
            2, 8 ) );
        builder.add( new JLabel( "University of Tennessee" ), cc.xy( 2, 10 ) );

        JOptionPane.showMessageDialog(
            null, panel, "About LoDN Client", JOptionPane.PLAIN_MESSAGE );
    }

}