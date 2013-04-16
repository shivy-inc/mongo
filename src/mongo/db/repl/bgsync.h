/**
 *    Copyright (C) 2012 10gen Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/thread/mutex.hpp>

#include "mongo/util/queue.h"
#include "mongo/db/oplogreader.h"
#include "mongo/db/repl/rs.h"
#include "mongo/db/jsobj.h"

namespace mongo {

    // This interface exists to facilitate easier testing;
    // the test infrastructure implements these functions with stubs.
    class BackgroundSyncInterface {
    public:
        virtual ~BackgroundSyncInterface();

        // Gets the head of the buffer, but does not remove it. 
        // Returns true if an element was present at the head;
        // false if the queue was empty.
        virtual bool peek(BSONObj* op) = 0;

        // Deletes objects in the queue;
        // called by sync thread after it has applied an op
        virtual void consume() = 0;

        // Returns the member we're currently syncing from (or NULL)
        virtual Member* getSyncTarget() = 0;

        // wait up to 1 second for more ops to appear
        virtual void waitForMore() = 0;
    };


    /**
     * Lock order:
     * 1. rslock
     * 2. rwlock
     * 3. BackgroundSync::_mutex
     */
    class BackgroundSync : public BackgroundSyncInterface {
        static BackgroundSync *s_instance;
        // protects creation of s_instance
        static boost::mutex s_mutex;

        // _mutex protects all of the class variables
        boost::mutex _mutex;

        // Production thread
        BlockingQueue<BSONObj> _buffer;

        GTID _lastGTIDFetched;
        // if produce thread should be running
        bool _pause;

        Member* _currentSyncTarget;

        // Notifier thread

        // used to wait until another op has been replicated
        boost::condition_variable _lastOpCond;
        boost::mutex _lastOpMutex;

        OpTime _consumedOpTime; // not locked, only used by notifier thread

        struct QueueCounter {
            QueueCounter();
            unsigned long long waitTime;
            unsigned int numElems;
        } _queueCounter;

        BackgroundSync();
        BackgroundSync(const BackgroundSync& s);
        BackgroundSync operator=(const BackgroundSync& s);


        // Production thread
        void _producerThread();
        // Adds elements to the list, up to maxSize.
        void produce();
        // Check if rollback is necessary
        bool isRollbackRequired(OplogReader& r);
        void getOplogReader(OplogReader& r);
        // check latest GTID against the remote's earliest GTID, filling in remoteOldestOp.
        bool isStale(OplogReader& r, BSONObj& remoteOldestOp);
        // stop syncing when this becomes a primary
        void stop();
        // restart syncing
        void start();

        bool hasCursor();
    public:
        static BackgroundSync* get();
        static void shutdown();
        virtual ~BackgroundSync() {}

        // starts the producer thread
        void producerThread();

        // Interface implementation

        virtual bool peek(BSONObj* op);
        virtual void consume();
        virtual Member* getSyncTarget();
        virtual void waitForMore();

        // For monitoring
        BSONObj getCounters();
    };


} // namespace mongo
