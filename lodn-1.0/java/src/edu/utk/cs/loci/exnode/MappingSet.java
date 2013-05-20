/*
 * Created on Jan 9, 2004
 *
 * To change the template for this generated file go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
package edu.utk.cs.loci.exnode;

import java.util.*;

import edu.utk.cs.loci.ibp.Allocation;
import edu.utk.cs.loci.ibp.Log;

/**
 * @author millar
 *
 * To change the template for this generated type comment go to
 * Window&gt;Preferences&gt;Java&gt;Code Generation&gt;Code and Comments
 */
public class MappingSet
{
    public static Log DEBUG = new Log( true );
    
    private TreeMap mappings;

    public MappingSet()
    {
        mappings = new TreeMap();
    }

    public MappingSet( TreeMap map )
    {
        mappings = map;
    }

    public synchronized void add( Mapping m )
    {
        // mappings contains a map of TreeMaps
        // keyed by offset
        
        // under each offset, there is a TreeMap keyed by length
        // Under each entry of that TreeMap, is an ArrayList
        
        TreeMap thisOffsetTree;
        Long offsetKey = new Long( m.getExnodeOffset() );
        Long lengthKey = new Long( m.getLogicalLength() );

        if ( mappings.containsKey( offsetKey ) )
        {
            thisOffsetTree = (TreeMap) mappings.get( offsetKey );
        }
        else
        {
            thisOffsetTree = new TreeMap();
            mappings.put( offsetKey, thisOffsetTree );
        }

        ArrayList list;

        if ( thisOffsetTree.containsKey( lengthKey ) )
        {
            list = (ArrayList) thisOffsetTree.get( lengthKey );
        }
        else
        {
            list = new ArrayList();
            thisOffsetTree.put( lengthKey, list );
        }

        list.add( m );
    }

    /**
     * Returns a new Arraylist containing all the mappings in this MappingSet.
     * 
     * @return
     */
    public synchronized ArrayList getAllMappings()
    {
        ArrayList list = new ArrayList();
        Iterator i = mappings.values().iterator();
        while ( i.hasNext() )
        {
            TreeMap subTree = (TreeMap) i.next();
            Iterator j = subTree.values().iterator();
            while ( j.hasNext() )
            {
                List mappingList = (List) j.next();
                list.addAll( mappingList );
            }
        }

        return list;
    }

    /**
     * Returns a new ArrayList containing all the mappings that
     * contain the block [offset,offset+length)
     * 
     * @param offset
     * @param length
     * @return
     */
    public synchronized ArrayList getContainingMappings( long offset, long length )
    {
        //DEBUG.println( "getContainingMappings( " + offset + "," + length + ")" );
        
        ArrayList list = new ArrayList();
        Long offsetPlusOneKey = new Long( offset + 1 );

        // get submap with all entries with offset <= offset
        SortedMap headMap = mappings.headMap( offsetPlusOneKey );

        //DEBUG.println( "headMap.size()=" + headMap.size() );
        
        // get set of all entries in the headmap
        Set entries = headMap.entrySet();
        
        for ( Iterator i = entries.iterator(); i.hasNext(); )
        {
            Map.Entry entry = (Map.Entry) i.next();

            //DEBUG.println( "checking headMap entry " + entry );
            
            // get offset of this entry
            Long offsetKey = (Long) entry.getKey();
            
            // get TreeMap that is stored under this entry
            SortedMap subMap = (SortedMap) entry.getValue();

            //DEBUG.println( "subMap.size()=" + subMap.size() );
            
            // get the length needed.  If offsetKey is lower than offset,
            // then the mapping would have to be bigger than length
            // so that it can cover the internal [offset,offset+length)
            long lengthNeeded = ( offset + length ) - offsetKey.longValue();
            
            Long lengthKey = new Long( lengthNeeded );
            
            // get entries whose lengths are >= lengthNeeded
            SortedMap qualifyingEntries = subMap.tailMap( lengthKey );

            //DEBUG.println( "subMap.tailMap.size()=" + qualifyingEntries.size() );
            
            for ( Iterator qualifyingLists = qualifyingEntries.values().iterator();
                  qualifyingLists.hasNext(); )
            {
                // entries of the map are ArrayLists
                ArrayList qualifyingList = (ArrayList) qualifyingLists.next();
                
                //DEBUG.println( "Checking qualifyingList " + qualifyingList + ", size="
                //    + qualifyingList.size() );
                
                // list.addAll( qualifyingList );
                
                for ( Iterator k = qualifyingList.iterator(); k.hasNext(); )
                {
                    Mapping m = (Mapping) k.next();
                    Allocation a = m.getAllocation();
                    String host = a.getDepot().getHost();
                    int port = a.getDepot().getPort();
                    long exnodeOffset = m.getExnodeOffset();
                    long logicalLength = m.getLogicalLength();
                    DEBUG.println( "Adding mapping for [" + offset + "," + length +"] : "
                        + host + ":" + port + " [" + exnodeOffset + "," + logicalLength + "]" );
                    list.add( m );
                }
            }
        }
        
        return list;
    }
}