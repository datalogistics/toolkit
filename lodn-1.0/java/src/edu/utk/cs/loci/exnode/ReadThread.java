/* $Id: ReadThread.java,v 1.4 2008/05/24 22:25:52 linuxguy79 Exp $ */

package edu.utk.cs.loci.exnode;

import java.util.*;

import edu.utk.cs.loci.ibp.Log;
import edu.utk.cs.loci.transfer.TransferThread;

public class ReadThread extends TransferThread
{
    public static Log DEBUG = new Log( true );

    /** 
     * Number of times to retry AFTER having failed once already.
     * (Set this to 0 if you don't want to retry at all.)
     */
    public static int MAX_EXHAUST_RETRIES = 2;
    
    public static long RETRY_AGE = 60000;

    private JobQueue mainQueue;
    private JobQueue pendingQueue;
    private JobQueue retryQueue;

    public ReadThread( int id, JobQueue mainQueue, JobQueue pendingQueue,
        JobQueue retryQueue )
    {
        super( id );
        this.mainQueue = mainQueue;
        this.pendingQueue = pendingQueue;
        this.retryQueue = retryQueue;
    }

    public void run()
    {
        boolean done = false;

        while ( !done )
        {
            this.setCurJob( null );

            ReadJob job = null;
            
            boolean foundJob = false;

            // First, check for retries.
            // Jobs in the retry queue are jobs which
            // were already exhausted.  Prioritize
            // them because if they fail enough times,
            // we need to abort the whole download.
            // We don't want to waste time downloading
            // other data if we are going to abort anyway in the end.

            // TODO: Consider adding a delay parameter
            // to give the job some time before it is retried
            // again, just in case network conditions improve.

            try
            {
                synchronized ( retryQueue )
                {
                    int count = retryQueue.size();

                    // Look for a job that is retriable
                    // Requeue jobs which are not retriable

                    // FIXME: It might be more efficient and more
                    // predictable to go through the list and delete from
                    // the middle rather than keep on requeueing
                    // At present, we don't do this because 
                    // retryQueue is a JobQueue, and JobQueue
                    // does not expose a method for removing items
                    // in the middle of the queue

                    while ( !foundJob && (count > 0) )
                    {
                        try
                        {
                            job = (ReadJob) retryQueue.remove();
                        }
                        catch ( NoSuchElementException e3 )
                        {
                            // I don't think this should occur anymore since we always check size before removing
                            //done = true;
                            DEBUG.println( "Thread " + this + ": "
                                + " Exception removing from empty retryQueue" );
                        }

                        // NOTE: we use count instead of
                        // calling pendingQueue.size() in the while
                        // condition because in some cases, we
                        // requeue jobs into the pendingQueue
                        // Using count guarantees that we don't
                        // recheck jobs that we requeued.

                        count--;

                        if ( job != null )
                        {
                            long timeSinceLastTry = System.currentTimeMillis() - job.getLastTryTime();
                            
                            if ( timeSinceLastTry < RETRY_AGE )
                            {
                                DEBUG.println( "Thread " + this + ": Job " + job + " from retryQueue is not old enough yet.  Requeueing ..." );
                                retryQueue.add( job );
                            }
                            else
                            {
                                DEBUG.println( "Thread " + this + ": found job " + job
                                    + " from retry Queue ..." );
                                foundJob = true;
                            }
                        }
                        else
                        {
                            DEBUG.error( "Thread " + this
                                + ": got null from retryQueue" );
                        }
                    }
                }
            }
            catch ( NoSuchElementException e )
            {
            }

            // If there are no jobs in the retry queue, try
            // the main Queue

            if ( !foundJob )
            {
                // DEBUG.println( "Thread " + this + ": "
                //     + " retryQueue empty, reading from mainQueue" );
                try
                {
                    job = (ReadJob) mainQueue.remove();
                    if ( job != null )
                    {
                        DEBUG.println( "Thread " + this + ": got Job " + job
                            + " from main queue ..." );
                        foundJob = true;
                    }
                }
                catch ( NoSuchElementException e2 )
                {
                }
            }

            // If there are no jobs in the main queue, try the
            // pending queue

            if ( !foundJob )
            {
                DEBUG.println( "Thread " + this + ": "
                    + " mainQueue empty, reading from pendingQueue" );
                synchronized ( pendingQueue )
                {
                    int count = pendingQueue.size();

                    // Look for a job that is retriable
                    // Requeue jobs which are not retriable

                    // FIXME: It might be more efficient and more
                    // predictable to go through the list and delete from
                    // the middle rather than keep on requeueing
                    // At present, we don't do this because 
                    // pendingQueue is a JobQueue, and JobQueue
                    // does not expose a method for removing items
                    // in the middle of the queue

                    while ( !foundJob && (count > 0) )
                    {
                        try
                        {
                            job = (ReadJob) pendingQueue.remove();
                        }
                        catch ( NoSuchElementException e3 )
                        {
                            // I don't think this should occur anymore since we always check size before removing
                            done = true;
                            DEBUG.println( "Thread " + this + ": "
                                + " Exception removing from empty pendingQueue" );
                        }

                        // NOTE: we use count instead of
                        // calling pendingQueue.size() in the while
                        // condition because in some cases, we
                        // requeue jobs into the pendingQueue
                        // Using count guarantees that we don't
                        // recheck jobs that we requeued.

                        count--;

                        if ( job != null )
                        {
                            DEBUG.println( "Thread " + this
                                + " considering pending job " + job
                                + " with conc. tries "
                                + job.getConcurrentTries() );

                            boolean retry = job.shouldProbablyBeRetriedInParallel();
                            if ( !retry )
                            {
                                DEBUG.println( "Requeuing pending job ..." );
                                pendingQueue.add( job );
                            }
                            else
                            {
                                DEBUG.println( "Job " + job
                                    + " can be retried ..." );
                                foundJob = true;
                            }
                        }
                        else
                        {
                            DEBUG.error( "Thread " + this
                                + ": got null from pendingQueue" );
                        }
                    }
                }
            }

            if ( !foundJob )
            {
                if ( pendingQueue.size() > 0 )
                {
                    try
                    {
                        long delay = (ReadJob.READ_TIMEOUT * 1000) / 4;
                        DEBUG.println( "Thread "
                            + this
                            + ": There are pending jobs, but none retriable right now. Sleeping for "
                            + delay + " ms" );

                        this.setStatus( "Waiting for pending jobs to be retriable..." );
                        
                        Thread.sleep( delay );
                    }
                    catch ( InterruptedException ie )
                    {
                        DEBUG.println( "Thread " + this
                            + " interrupted while sleeping on pending queue" );
                    }

                    // Go back to beginning of while and read queues again
                    continue; // with while
                }
                else
                {
                    // if pendingQueue.size() == 0

                    done = true;
                    DEBUG.println( "Thread " + this + ": "
                        + " pendingQueue empty too" );
                }
            }

            if ( !done && (job != null) && job.isDone() )
            {
                DEBUG.println( "Thread " + this + ": Dequeued Job " + job
                    + " is already done." );
            }
            else if ( !done && (job != null) && !job.isDone() )
            {
                if ( job.isExhausted() )
                {
                    if ( job.getTimesRetried() < MAX_EXHAUST_RETRIES )
                    {
                        DEBUG.println( "Thread "
                            + this
                            + ": Job "
                            + job
                            + " is already exhausted but still retriable.  Resetting ... " );

                        job.resetAfterExhausted();
                    }
                    else
                    {
                        DEBUG.error( "Thread "
                            + this
                            + ": Job "
                            + job
                            + " gotten from queue already exhausted and retried "
                            + MAX_EXHAUST_RETRIES
                            + " times.  Quitting this job!" );

                        // quit the run() method and end the Thread.
                        // Basically, this file transfer is dead because the read job cannot be finished

                        // cancel the exnode download
                        Exnode exnode = job.getExnode();

                        // synchronized ( exnode )
                        // {
                        exnode.setCancel( true );
                        exnode.setError( new ReadException(
                            "Unable to accomplish " + "ReadJob " + job ) );
                        // }

                        // end the while loop
                        done = true;
                        continue;
                    }
                }

                DEBUG.println( "Thread " + this + ": Adding Job " + job
                    + " to pendingQueue" );
                pendingQueue.add( job );

                DEBUG.println( "Thread " + this + ": executing Job " + job );

                // *** THIS IS WHERE THE ACTION TAKES PLACE ***
                this.setCurJob( job );
                
                job.execute();

                if ( job.isDone() )
                {
                    try
                    {
                        DEBUG.println( "Thread " + this + ": Job " + job
                            + " DONE! Removing from pendingQueue." );
                        pendingQueue.remove( job );
                    }
                    catch ( NoSuchElementException e )
                    {
                        DEBUG.println( "Thread " + this
                            + ": trying to remove from empty pendingQueue" );
                    }
                }
                else
                {
                    Exnode exnode = job.getExnode();

                    if ( exnode.cancel )
                    {
                        DEBUG.println( "Thread " + this + ": the exnode "
                            + " download has been cancelled.  Aborted Job "
                            + job );

                        // end the while loop
                        done = true;
                        continue;
                    }

                    DEBUG.println( "Thread " + this + ": Job " + job
                        + " exited but was not done." );
                    

                    if ( job.getConcurrentTries() == 0 )
                    {
                        DEBUG.println( "Thread " + this + ": Removing Job "
                            + job + " from pendingQueue" // );
                            + " and adding it to retryQueue ..." );

                        pendingQueue.remove( job );

                        if ( job.getTimesRetried() < MAX_EXHAUST_RETRIES )
                        {
                            if ( job.isExhausted() )
                            {
                                DEBUG.println( "Thread " + this + ": Job " + job
                                    + " has exhausted all its mappings, but is still retriable." );
                            }
                            else
                            {
                                DEBUG.println( "Thread " + this + ": Job " + job
                                    + " is not exhausted yet.  Putting it on retryQueue ..." );
                            }
                            
                            retryQueue.add( job );
                        }
                        else
                        {
                            DEBUG.error( "Thread "
                                + this
                                + ": Job "
                                + job
                                + " has already been retried "
                                + MAX_EXHAUST_RETRIES
                                + " times.  Quitting this job and NOT adding it to retryQueue!" );

                            // quit the run() method and end the Thread.
                            // Basically, this file transfer is dead because the read job cannot be finished

                            // cancel the exnode download

                            // synchronized ( exnode )
                            // {
                            exnode.setCancel( true );
                            exnode.setError( new ReadException(
                                "Unable to accomplish " + "ReadJob " + job ) );
                            // }

                            // end the while loop
                            done = true;
                            continue;
                        }
                    }
                    else
                    {
                        DEBUG.println( "Thread "
                            + this
                            + ": However, job "
                            + job
                            + " still has "
                            + job.getConcurrentTries()
                            + " conc. tries, so not removing it from pending queue yet." );
                    }
                }
            }
        }
        DEBUG.println( "Thread " + this + " done. exiting ..." );
    }
}