/** Test TTL collections with replication
 *  Part 1: Initiate replica set. Insert some docs and create a TTL index. Check that both primary
 *          and secondary get userFlags=1 (indicating that usePowerOf2Sizes is on),
 *          and check that the correct # of docs age out.
 *  Part 2: Add a new member to the set.  Check it also gets userFlags=1 and correct # of docs.
 */

var rt = new ReplSetTest( { name : "ttl_repl" , nodes: 2 } );

/******** Part 1 ***************/

// setup set
var nodes = rt.startSet();
rt.initiate();
var master = rt.getMaster();
rt.awaitSecondaryNodes();
var slave1 = rt.liveNodes.slaves[0];

// shortcuts
var masterdb = master.getDB( 'd' );
var mastercol = masterdb[ 'c' ];
var slave1col = slave1.getDB( 'd' )[ 'c' ];

// create new collection. insert 24 docs, aged at one-hour intervalss
mastercol.drop();
now = (new Date()).getTime();
for ( i=0; i<24; i++ )
    mastercol.insert( { x : new Date( now - ( 3600 * 1000 * i ) ) } );
masterdb.getLastError();
rt.awaitReplication();
assert.eq( 24 , mastercol.count() , "docs not inserted on primary" );
assert.eq( 24 , slave1col.count() , "docs not inserted on secondary" );

// before TTL index created, check that userFlags are 0
print("Initial Stats:")
print("Master:");
printjson( mastercol.stats() );
print("Slave1:");
printjson( slave1col.stats() );

// create TTL index, wait for TTL monitor to kick in, then check that
// the correct number of docs age out
mastercol.ensureIndex( { x : 1 } , { expireAfterSeconds : 20000 } );
masterdb.getLastError();
rt.awaitReplication();

sleep(70*1000); // TTL monitor runs every 60 seconds, so wait 70

print("Stats after waiting for TTL Monitor:")
print("Master:");
printjson( mastercol.stats() );
print("Slave1:");
printjson( slave1col.stats() );

assert.eq( 6 , mastercol.count() , "docs not deleted on primary" );
assert.eq( 6 , slave1col.count() , "docs not deleted on secondary" );


/******** Part 2 ***************/

// add a new secondary, wait for it to fully join
var slave = rt.add();
rt.reInitiate();
rt.awaitSecondaryNodes();

var slave2col = slave.getDB( 'd' )[ 'c' ];

// check that it has right number of docs
print("New Slave stats:");
printjson( slave2col.stats() );

assert.eq( 6 , slave2col.count() , "wrong number of docs on new secondary");

// finish up
rt.stopSet();
