/*
 * Created on Feb 5, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.lodnclient;

import java.io.File;
import javax.swing.filechooser.FileFilter;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class XNDFileFilter extends FileFilter
{

    public boolean accept( File f )
    {
        if ( f.getName().endsWith( ".xnd" ) || f.isDirectory() )
        {
            return (true);
        }
        else
        {
            return (false);
        }
    }

    public String getDescription()
    {
        return ("Exnode (*.xnd)");
    }

}