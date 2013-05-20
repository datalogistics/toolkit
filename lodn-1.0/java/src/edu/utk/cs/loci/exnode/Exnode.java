/* $Id: Exnode.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.util.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;
import org.xml.sax.*;

import edu.utk.cs.loci.ibp.Log;
import edu.utk.cs.loci.transfer.TransferThread;

public class Exnode extends MetadataContainer
{
    public static Log DEBUG = new Log( true );

    public static long READ_WATCHDOG_CYCLE_TIME = 3000;

    public static String namespace = new String(
        "http://loci.cs.utk.edu/exnode" );

    MappingSet mappings;
    public Progress progress;

    boolean cancel;
    Exception error;
    TransferThread[] threads;

    public Exnode()
    {
        mappings = new MappingSet();
        cancel = false;
        error = null;

        addMetadata( new StringMetadata( "Version", "3.0" ) );
    }

    /**
     * @return Returns the threads.
     */
    public TransferThread[] getThreads()
    {
        return this.threads;
    }

    public void addMapping( Mapping m )
    {
        mappings.add( m );
    }

    public ArrayList getAllMappings()
    {
        return (mappings.getAllMappings());
    }

    public ArrayList getContainingMappings( long offset, long length )
    {
        return mappings.getContainingMappings( offset, length );
    }

    public String toXML() throws SerializeException
    {
        StringBuffer sb = new StringBuffer();

        sb.append( "<?xml version=\"1.0\"?>\n" );
        sb.append( "<exnode xmlns:exnode=\"" + namespace + "\">\n" );

        Iterator i = getMetadata();
        Metadata md;
        while ( i.hasNext() )
        {
            md = (Metadata) i.next();
            sb.append( md.toXML() );
        }

        i = getAllMappings().iterator();
        Mapping m;
        while ( i.hasNext() )
        {
            m = (Mapping) i.next();
            sb.append( m.toXML() );
        }

        sb.append( "</exnode>" );

        return (sb.toString());
    }
    
    public static Exnode fromXML( String xml) throws DeserializeException {
        Document doc = null;
        DocumentBuilder db = null;
        DocumentBuilderFactory dbf = null;
        InputStream is = new ByteArrayInputStream(xml.getBytes());

        try
        {
            dbf = DocumentBuilderFactory.newInstance();
            dbf.setNamespaceAware( true );
            db = dbf.newDocumentBuilder();
            doc = db.parse(is);
        }
        catch ( SAXParseException ex )
        {
            DEBUG.error( "SAXParseException: " + ex );
            throw (new DeserializeException( ex.getMessage() ));
        }
        catch ( SAXException ex )
        {
            // do some logging
            DEBUG.error( "SAXException: " + ex );
            throw (new DeserializeException( ex.getMessage() ));
        }
        catch ( Exception e )
        {
            e.printStackTrace();
            throw (new DeserializeException( e.getMessage() ));
        }
    	
    	return generateExnodeFromDocument(doc);
    }

    public static Exnode fromURI( String uri ) throws DeserializeException
    {
        Document doc = null;
        DocumentBuilder db = null;
        DocumentBuilderFactory dbf = null;

        long startTime = System.currentTimeMillis();

        try
        {
            DEBUG.println( "uri = " + uri );

            dbf = DocumentBuilderFactory.newInstance();
            dbf.setNamespaceAware( true );
            db = dbf.newDocumentBuilder();
            doc = db.parse( uri );
        }
        catch ( SAXParseException ex )
        {
            DEBUG.error( "SAXParseException: " + ex );
            DEBUG.error( "SAXParse error in XMLParser.readDoc(" + uri + ")" );
            throw (new DeserializeException( ex.getMessage() ));
        }
        catch ( SAXException ex )
        {
            // do some logging
            DEBUG.error( "SAXException: " + ex );
            DEBUG.error( "SAXParse error in XMLParser.readDoc(" + uri + ")" );
            throw (new DeserializeException( ex.getMessage() ));
        }
        catch ( Exception e )
        {
            e.printStackTrace();
            throw (new DeserializeException( e.getMessage() ));
        }

        long saxParseEndTime = System.currentTimeMillis();

        DEBUG.println( "SAX Parsing " + (saxParseEndTime - startTime) + " ms" );

        return generateExnodeFromDocument(doc);
    }

	private static Exnode generateExnodeFromDocument(Document doc)
			throws DeserializeException {
		Exnode exnode = new Exnode();

        try
        {
            Element root = doc.getDocumentElement();

            long metadataStartTime = System.currentTimeMillis();

            // NOTE: lfgs 20050120.1529
            // I changed the way the metadata elements are read.
            // The old version read all metadata elements first (including
            // ones which are not children of root).  Here, I
            // just read the child nodes first.  This is MUCH MUCH
            // faster.  In one case, parsing time is reduced
            // from 44s to 800ms.

            NodeList childNodes = root.getChildNodes();

            long getElementsEndTime = System.currentTimeMillis();

            DEBUG.println( "Root getchildNodes time = "
                + (getElementsEndTime - metadataStartTime) + " ms."
                + " # of nodes found: " + childNodes.getLength() );

            int rootMetadata = 0;
            int rootMappings = 0;

            Metadata md;
            Mapping m;

            for ( int i = 0; i < childNodes.getLength(); i++ )
            {
                Node node = childNodes.item( i );
                if ( node.getNodeType() == Node.ELEMENT_NODE )
                {
                    Element e = (Element) childNodes.item( i );
                    if ( e.getNamespaceURI().equals( namespace ) )
                    {
                        if ( e.getLocalName().equals( "metadata" ) )
                        {
                            md = Metadata.fromXML( e );
                            exnode.addMetadata( md );
                            rootMetadata++;
                        }
                        else if ( e.getLocalName().equals( "mapping" ) )
                        {

                            // TODO: Mapping.fromXML is still written old-style
                            // and inefficient.  However, since the number of elements
                            // in a mapping is small, it doesn't seem to hurt
                            // that much.  Fix it later for performance and
                            // neatness' sake.
                            m = Mapping.fromXML( e );
                            exnode.addMapping( m );
                            rootMappings++;
                        }
                    }
                }
            }

            long mappingsEndTime = System.currentTimeMillis();

            DEBUG.println( "Mappings total parse time = "
                + (mappingsEndTime - metadataStartTime) + " ms."
                + " Total mappings found: " + rootMappings
                + ". Total Metadata elements: " + exnode.metadata.size() );

        }
        catch ( Exception e )
        {
            e.printStackTrace();
            throw (new DeserializeException( e.getMessage() ));
        }
        return (exnode);
	}

    public long getLength()
    {
        Iterator mappings = getAllMappings().iterator();
        Mapping m;
        long length = 0;
        long offset = -1;

        while ( mappings.hasNext() )
        {
            m = (Mapping) mappings.next();

            long exnodeOffset = m.getExnodeOffset();
            long logicalLength = m.getLogicalLength();

            if ( exnodeOffset > offset )
            {
                offset = exnodeOffset;
                length = exnodeOffset + logicalLength;
            }
        }

        return (length);
    }

    public void addProgressListener( ProgressListener pl )
    {
        progress.addProgressListener( pl );
    }

    public void updateProgress( long progressValue, String statusMsg )
    {
        if ( progress != null )
            progress.update( progressValue, statusMsg );
    }

    public Progress getProgress()
    {
        return (progress);
    }

    public void setCancel( boolean flag )
    {
        cancel = flag;
    }

    public void setError( Exception e )
    {
        DEBUG.error( "Thread " + Thread.currentThread() + ", Exnode.setError: "
            + e );
        error = e;
    }

    public SortedSet getTransferBoundaries( ArrayList mappingsList,
        int maxTransferSize )
    {
        SortedSet transferBoundaries = new TreeSet();

        for ( Iterator i = mappingsList.iterator(); i.hasNext(); )
        {
            Mapping m = (Mapping) i.next();
            long exnodeOffset = m.getExnodeOffset();
            long logicalLength = m.getLogicalLength();
            //long allocLength=m.getAllocationLength();

            transferBoundaries.add( new Long( exnodeOffset ) );

            // NOTE: stop points also need to be transfer boundaries
            // in case there are "holes" in one of the replicas,
            // such that one block in one replica ends before another
            // one begins
            transferBoundaries.add( new Long( exnodeOffset + logicalLength ) );

            DEBUG.println( "Exnode.getTransferBoundaries: exnodeOffset : "
                + exnodeOffset + ", logicalLength : " + logicalLength );

            //DEBUG.println("  allocLength : " + allocLength);

            // 			if(m.hasE2EBlocks()) {
            // 				long e2eBlockSize=m.getE2EBlockSize();
            // 				int numBlocks=(int)(logicalLength/e2eBlockSize);
            // 				for(int j=0;j<numBlocks+1;j++) {
            // 					if(exnodeOffset+(j*e2eBlockSize)<offset+length) {
            // 						transferBoundaries.add(new Long(exnodeOffset+(j*e2eBlockSize)));						
            // 					}
            // 				}
            // 			} else 

            /*
             YES. The Block Size field in the LoDN Client GUI is now useless with these lines out

             if( blockSize > 0 ) {
             int numBlocks;
             if(logicalLength % blockSize == 0) {
             numBlocks=(int)(logicalLength / blockSize);
             } else {
             numBlocks=(int)(logicalLength / blockSize + 1);
             }
             
             for(int j=0; j < numBlocks+1; j++) {
             if(exnodeOffset + (j * blockSize) < offset + length) {
             transferBoundaries.add(new Long(exnodeOffset + (j * blockSize)));
             }
             }
             }
             */
        }

        if ( maxTransferSize > 0 )
        {
            // Insert new boundaries to guarantee maxTransferSize
            // Note: at this point, the last point is the end of the file.
            // We remove it AFTER inserting the necessary other points.
            Object[] boundaries = transferBoundaries.toArray();

            long prevPos = ((Long) boundaries[0]).longValue();

            for ( int i = 1; i < boundaries.length; i++ )
            {
                long curPos = ((Long) boundaries[i]).longValue();
                while ( curPos - prevPos > maxTransferSize )
                {
                    prevPos += maxTransferSize;
                    transferBoundaries.add( new Long( prevPos ) );
                    DEBUG.println( "Exnode.getTransferBoundaries: added transfer boundary at offset: "
                        + prevPos );
                }
                prevPos = curPos;
            }
        }

        // Because we include end points, the last point
        // will be the end of the file.
        // Remove that.
        transferBoundaries.remove( transferBoundaries.last() );

        return transferBoundaries;
    }

    public void read( String filename, long offset, long length,
        int transferSize, int numThreads ) throws Exception
    {
        SyncFile file = null;
        try
        {
            file = new PhysicalSyncFile( filename );
        }
        catch ( Exception e )
        {
            DEBUG.error( "Exnode read error opening output file " + filename
                + ": " + e );
            throw e;
        }

        read( file, offset, length, transferSize, numThreads );
    }

    public void read( SyncFile file, long offset, long length,
        int transferSize, int numThreads ) throws Exception
    {
        /*
         read data from the Logistical Network and write it in a local file
         */

        cancel = false;
        error = null;

        Object[] transferBoundaries = getTransferBoundaries(
            this.getAllMappings(), transferSize ).toArray();
        JobQueue mainQueue = new JobQueue();
        JobQueue pendingQueue = new JobQueue();
        JobQueue retryQueue = new JobQueue();

        //if(DBG) { DEBUG.println("Exnode10 (read filename): offset, length, blockSize = " + 
        //			     offset +", "+ length +", "+ blockSize); }

        Long temp;
        long readOffset;
        long nextOffset;
        int readLength;

        for ( int i = 0; i < transferBoundaries.length - 1; i++ )
        {
            temp = (Long) transferBoundaries[i];
            readOffset = temp.longValue();
            temp = (Long) transferBoundaries[i + 1];
            nextOffset = temp.longValue();
            long offsetDifference = nextOffset - readOffset;
            if ( offsetDifference > Integer.MAX_VALUE )
            {
                throw new RuntimeException(
                    "Exnode.read(): Length between transfer boundaries cannot be greater than 2GB." );
            }
            else
            {
                readLength = (int) offsetDifference;
            }

            DEBUG.println( "Exnode22: Adding ReadJob[" + readOffset + ","
                + readLength + "]" );

            ReadJob job = new ReadJob(
                this, readOffset, readLength, file, offset );
            mainQueue.add( job );
            //if(DBG) DEBUG.println("Exnode88: AddJob " + i);
        }

        temp = (Long) transferBoundaries[transferBoundaries.length - 1];
        readOffset = temp.longValue();
        long lastBlockLength = length - readOffset;
        if ( lastBlockLength > Integer.MAX_VALUE )
        {
            throw new RuntimeException(
                "Exnode.read(): last block length cannot be greater than 2GB." );
        }
        else
        {
            readLength = (int) lastBlockLength;
        }

        //if(DBG) { DEBUG.println("Exnode80: length " + length); }
        //if(DBG) { DEBUG.println("Exnode81: readOffset " + readOffset); }

        DEBUG.println( "Exnode23: Adding ReadJob[" + readOffset + ","
            + readLength + "]" );

        ReadJob job = new ReadJob( this, readOffset, readLength, file, offset );
        mainQueue.add( job );

        //if(DBG) DEBUG.println("Exnode88: AddJob.");

        this.threads = new ReadThread[numThreads];

        DEBUG.println( "Exnode: There are " + numThreads + " threads to start!" );

        for ( int i = 0; i < numThreads; i++ )
        {
            threads[i] = new ReadThread( i, mainQueue, pendingQueue, retryQueue );
            threads[i].start();

            DEBUG.println( "Exnode: Start ReadThread #" + i );
        }

        boolean doneOrCancelled = false;

        while ( !doneOrCancelled )
        {
            int mainSize = mainQueue.size();
            int pendingSize = pendingQueue.size();
            int retrySize = retryQueue.size();

            DEBUG.println( "Exnode.read() WATCHDOG: mainQueue= " + mainSize
                + ", pending=" + pendingSize + ", retryQueue=" + retrySize );

            // FIXME: This might lead to inefficiency.  Try it out for now.

            // Trigger ProgressDialog to display changes, if any
            this.progress.fireProgressChangedEvent();

            // FIXME: There is a small chance that we check the size here
            // between when the ReadThread removes from one of the queues (including the pendingQueue) 
            // and puts it in the pendingQueue
            // In that case, we can lose track of a pending job

            // Note: Checking get PercentComplete alone might not be accurate
            // since if we have redundancy, then towards the end,
            // percent complete may exceed 100%.  This is because
            // all successful tries are counted towards percent complete.
            // Currently, the code does not check whether the successful try
            // is already a duplicate one (where the previous one also succeeded).

            if ( this.cancel
                || ((this.getProgress().getPercentComplete() >= 1.0)
                    && (mainSize == 0) && (retrySize == 0) && (pendingSize == 0)) )
            {
                if ( this.cancel )
                {
                    DEBUG.println( "Exnode.read(): exnode cancelled! Emptying Queues ..." );
                    mainQueue.clear();
                    pendingQueue.clear();
                    retryQueue.clear();
                }

                doneOrCancelled = true;
                DEBUG.println( "Exnode.read() WATCHDOG: ALL DONE!" );

                for ( int i = 0; i < numThreads; i++ )
                {
                    Thread t = threads[i];

                    if ( t.isAlive() )
                    {
                        // FIXME: Apparently, t.interrupt doesn't stop the stuck thread

                        DEBUG.println( "Thread " + t
                            + " is still alive.  Interrupting..." );

                        t.interrupt();

                        //DEBUG.println( "joining on Thread " + t );
                        //t.join();
                        //DEBUG.println( "Thread " + t + " join done." );
                    }
                    else
                    {
                        DEBUG.println( "Thread " + t + " is already dead." );
                    }

                    DEBUG.println( "Exnode: ReadThread #" + i + " DONE!" );
                }

            }
            else
            {
                try
                {
                    Thread.sleep( READ_WATCHDOG_CYCLE_TIME );
                }
                catch ( InterruptedException e )
                {
                    DEBUG.println( "Exnode.read thread "
                        + Thread.currentThread() + " interrupted." );
                }
            }
        }

        try
        {
            file.close();
        }
        catch ( Exception e )
        {
            DEBUG.error( "Exnode.read: Error in file.close(): " + e );
            // e.printStackTrace();
        }

        // TODO: Check this.  This might be unnecessary or incorrect.  Does this mean
        // that it fails if there is ANY error at all?  lfgs 20050120.1755
        if ( error != null )
        {
            progress.fireProgressErrorEvent();
            throw (error);
        }

        //progress = null;
    } // read()

    public void write( String filename, int blockSize, int numThreads,
        int duration, DepotList depots, String functionName ) throws Exception
    {
        write(
            filename, blockSize, numThreads, duration, 1, depots, functionName );
    }

    public void write( SyncFile file, int blockSize, int numThreads,
        int duration, DepotList depots, String functionName ) throws Exception
    {
        write(
            file, blockSize, numThreads, duration, 1, depots, functionName );
    }
    
    
    public void write( String filename, int blockSize, int numThreads,
        int duration, int copies, DepotList depots, String functionName )
        throws Exception
    {
        SyncFile file = null;
        try
        {
            file = new PhysicalSyncFile( filename );
        }
        catch ( Exception e )
        {
            DEBUG.error( "Exnode wirte error opening input file " + filename
                + ": " + e );
            throw e;
        }

        write(
            file, blockSize, numThreads, duration, copies, depots, functionName );

    }

    public void write( SyncFile file, int blockSize, int numThreads,
        int duration, int copies, DepotList depots, String functionName )
        throws Exception
    {
        cancel = false;
        error = null;

        JobQueue queue = new JobQueue();

        long length = (long) file.length();

        progress = new Progress( length * copies );

        long numBlocks;
        if ( blockSize > 0 )
        {
            if ( length % blockSize == 0 )
            {
                numBlocks = (long) (length / blockSize);
            }
            else
            {
                numBlocks = (long) (length / blockSize + 1);
            }
        }
        else
        {
            numBlocks = 1;
            if ( length > Integer.MAX_VALUE )
            {
                throw new IllegalArgumentException(
                    "Length is > 2GB, and cannot be held in 1 block." );
            }
            else
            {
                blockSize = (int) length;
            }
        }

        for ( int i = 0; i < numBlocks; i++ )
        {
            for ( int j = 0; j < copies; j++ )
            {
                // NOTE: Now that blockSize is an int,
                // it is NECESSARY to cast to (long) here.  Otherwise
                // the result will just be an int, not a long
                long offset = i * (long) blockSize;
                int writeLength = (offset + blockSize <= length)
                    ? blockSize
                    : (int) (length - offset);
                WriteJob job = new WriteJob(
                    this, offset, writeLength, duration, file, depots,
                    functionName );
                queue.add( job );
            }
        }

        WriteThread[] threads = new WriteThread[numThreads];

        for ( int i = 0; i < numThreads; i++ )
        {
            threads[i] = new WriteThread( queue );
            threads[i].start();
        }

        try
        {
            for ( int i = 0; i < numThreads; i++ )
            {
                threads[i].join();
            }
        }
        catch ( Exception e )
        {
            DEBUG.error( "Exnode.write: error in threads.join: " + e );
            // e.printStackTrace();
        }

        if ( error != null )
        {
            progress.fireProgressErrorEvent();
            throw (error);
        }

        //progress=null;
    }

    public static void main( String[] args ) // for testing purpose only !
    {
        Exnode exnode = new Exnode();

        exnode.addMetadata( new StringMetadata( "filename", "icilenomdufichier" ) );
        exnode.addMetadata( new DoubleMetadata( "lorsversion", 0.82 ) );

        Metadata type = new ListMetadata( "Type" );
        type.add( new StringMetadata( "Name", "logistical_file" ) );
        type.add( new StringMetadata( "Version", "1.0" ) );
        exnode.addMetadata( type );

        Metadata tool = new ListMetadata( "Authoring Tool" );
        tool.add( new StringMetadata( "Name", "LoDN" ) );
        tool.add( new StringMetadata( "Version", "0.7" ) );
        exnode.addMetadata( tool );

        try
        {
            String serializedExnode = exnode.toXML();
            System.out.println( serializedExnode );
        }
        catch ( SerializeException e )
        {
            DEBUG.error( "" + e );
        }
    } // main()

}

// jar -cmf .../edu/utk/cs/loci/exnode/mainClass .../Exnode.jar edu
// ./compile.sh; ./buildjar.sh ; java -jar lodn/web/Exnode.jar 
