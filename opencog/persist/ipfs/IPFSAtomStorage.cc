/*
 * IPFSAtomStorage.cc
 * Persistent Atom storage, IPFS-backed.
 *
 * Atoms and Values are saved to, and restored from, an IPFS DB using
 * one of the available database drivers. Currently, the postgres
 * native libpq-dev API and the ODBC API are supported. Note that
 * libpq-dev is about three times faster than ODBC.
 *
 * Atoms are identified by means of unique ID's (UUID's), which are
 * correlated with specific in-RAM atoms via the TLB.
 *
 * Copyright (c) 2008,2009,2013,2015,2017 Linas Vepstas <linas@linas.org>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <stdlib.h>
#include <unistd.h>

#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atomspaceutils/TLB.h>

#include "IPFSAtomStorage.h"

using namespace opencog;

// Number of write-back queues
#define NUM_WB_QUEUES 6

/* ================================================================ */
// Constructors

void IPFSAtomStorage::init(const char * uri)
{
	_uri = uri;

	if (strncmp(uri, "ipfs://", 7))
		throw IOException(TRACE_INFO, "Unknown URI '%s'\n", uri);

	// We expect the URI to be for the form
	//    ipfs:///atomspace-key
	//    ipfs://hostname/atomspace-key
	// where the key will be used to publish the IPNS for the atomspace.

	std::string hostname;
	int port = 5001;
	if ('/' == uri[7])
	{
		hostname = "localhost";
		_keyname = &uri[8];
	}
	else
	{
		const char* start = &uri[7];
		hostname = start;
		char* p = strchr((char *)start, '/');
		if (nullptr == p)
			throw IOException(TRACE_INFO, "Bad URI format '%s'\n", uri);
		size_t len = p - start;
		hostname[len] = 0;
		_keyname = &uri[len+7];
	}

	// Create pool of IPFS server connections.
	_initial_conn_pool_size = NUM_OMP_THREADS + NUM_WB_QUEUES;
	for (int i=0; i<_initial_conn_pool_size; i++)
	{
		ipfs::Client* conn = new ipfs::Client(hostname, port);
		conn_pool.push(conn);
	}

	// Create the IPFS key, if it does not yet exist.
	try
	{
		ipfs::Client clnt(hostname, port);
		std::string key_id;
		clnt.KeyNew(_keyname, &key_id);
		std::cout << "Generated AtomSpace key: "
		          << key_id << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "AtomSpace key already exists: "
		          << e.what() << std::endl;
	}

	max_height = 0;
	bulk_load = false;
	bulk_store = false;
	clear_stats();

	tvpred = createNode(PREDICATE_NODE, "*-TruthValueKey-*");
	kill_data();
}

IPFSAtomStorage::IPFSAtomStorage(std::string uri) :
	_write_queue(this, &IPFSAtomStorage::vdo_store_atom, NUM_WB_QUEUES),
	_async_write_queue_exception(nullptr)
{
	init(uri.c_str());
}

IPFSAtomStorage::~IPFSAtomStorage()
{
	flushStoreQueue();

	while (not conn_pool.is_empty())
	{
		ipfs::Client* conn = conn_pool.pop();
		delete conn;
	}
}

/**
 * connected -- unconditionally true, right now. I guess.
 * XXX FIXME, return false if IPFS connection cannot be made.
 */
bool IPFSAtomStorage::connected(void)
{
	return true;
}

/**
 * Publish the AtomSpace CID to IPNS.
 */
void IPFSAtomStorage::publish(void)
{
	ipfs::Client* conn = conn_pool.pop();

	std::cout << "Publishing AtomSpace CID: " << _atomspace_cid << std::endl;

	// XXX hack alert -- lifetime set to 4 hours, it should be
	// infinity or something.... the TTL is 30 seconds, but should
	// be shorter or user-configurable .. set both with scheme bindings.
	std::string name;
	conn->NamePublish(_atomspace_cid, _keyname, &name, "4h", "30s");
	std::cout << "Published AtomSpace: " << name << std::endl;
	conn_pool.push(conn);
}

void IPFSAtomStorage::add_cid_to_atomspace(const std::string& cid)
{
	std::cout << "duude gonna add the CID! " << cid << std::endl;
}

/// Rethrow asynchronous exceptions caught during atom storage.
///
/// Atoms are stored asynchronously, from a write queue, from some
/// other thread. If that thread has an exception, e.g. due to some
/// IPFS error, and the exception is uncaught, then the process will
/// die. So we have to catch that exception.  Once caught, what do
/// we do with it? Well, we culd ignore it, but then the user would
/// not know that the IPFS backend was damaged. So, instead, we throw
/// it at the first user, any user that is doing soem other IPFS stuff.
void IPFSAtomStorage::rethrow(void)
{
#if 0
	if (_async_write_queue_exception)
	{
		std::exception_ptr exptr = _async_write_queue_exception;
		_async_write_queue_exception = nullptr;
		std::rethrow_exception(exptr);
	}
#endif
}

/* ================================================================== */
/// Drain the pending store queue. This is a fencing operation; the
/// goal is to make sure that all writes that occurred before the
/// barrier really are performed before before all the writes after
/// the barrier.
///
/// Caution: this is potentially racey in two different ways.
/// First, there is a small window in the async_caller implementation,
/// where, if the timing is just so, the barrier might return before
/// the last element is written.  (Although everything else will have
/// gone out; only the last element is in doubt). Technically, that's
/// a bug, but its sufficiently "minor" so we don't fix it.
///
/// The second issue is more serious: there's no fence or barrier in
/// Postgres (that I can find or think of), and so although we've sent
/// everything to PG, there's no guarantee that PG will process these
/// requests in order. How likely this could be, I don't know.
///
void IPFSAtomStorage::flushStoreQueue()
{
	rethrow();
	_write_queue.barrier();
	rethrow();
}

void IPFSAtomStorage::barrier()
{
	flushStoreQueue();
}

/* ================================================================ */

void IPFSAtomStorage::registerWith(AtomSpace* as)
{
	BackingStore::registerWith(as);
}

void IPFSAtomStorage::unregisterWith(AtomSpace* as)
{
	BackingStore::unregisterWith(as);

	flushStoreQueue();
}

/* ================================================================ */

/**
 * kill_data -- Publish an empty atomspace. Dangerous!
 * This will forget the IPFS reference to the atomspace containing
 * all of the atoms, resulting in data loss, unless you've done
 * something to keep ahold of that CID.
 *
 * This routine is meant to be used only for running test cases.
 * It is extremely dangerous, as it can lead to total data loss.
 */
void IPFSAtomStorage::kill_data(void)
{
	rethrow();

	std::string text = "AtomSpace " + _uri;
	ipfs::Json result;

	ipfs::Client* client = conn_pool.pop();
	client->FilesAdd({{"AtomSpace",
		ipfs::http::FileUpload::Type::kFileContents,
		text}}, &result);
	conn_pool.push(client);

	_atomspace_cid = result[0]["hash"];

	// Special case for TruthValues - must always have this atom.
	do_store_single_atom(tvpred);
	publish();
}

/* ================================================================ */

void IPFSAtomStorage::set_hilo_watermarks(int hi, int lo)
{
	_write_queue.set_watermarks(hi, lo);
}

void IPFSAtomStorage::set_stall_writers(bool stall)
{
	_write_queue.stall(stall);
}

void IPFSAtomStorage::clear_stats(void)
{
	_stats_time = time(0);
	_load_count = 0;
	_store_count = 0;
	_valuation_stores = 0;
	_value_stores = 0;

	_write_queue.clear_stats();

	_num_get_nodes = 0;
	_num_got_nodes = 0;
	_num_rec_nodes = 0;
	_num_get_links = 0;
	_num_got_links = 0;
	_num_rec_links = 0;
	_num_get_insets = 0;
	_num_get_inlinks = 0;
	_num_node_inserts = 0;
	_num_link_inserts = 0;
	_num_atom_removes = 0;
	_num_atom_deletes = 0;
}

void IPFSAtomStorage::print_stats(void)
{
	printf("ipfs-stats: Currently open URI: %s\n", _uri.c_str());
	time_t now = time(0);
	// ctime returns string with newline at end of it.
	printf("ipfs-stats: Time since stats reset=%lu secs, at %s",
		now - _stats_time, ctime(&_stats_time));


	size_t load_count = _load_count;
	size_t store_count = _store_count;
	double frac = store_count / ((double) load_count);
	printf("ipfs-stats: total loads = %zu total stores = %zu ratio=%f\n",
	       load_count, store_count, frac);

	size_t valuation_stores = _valuation_stores;
	size_t value_stores = _value_stores;
	printf("ipfs-stats: valuation updates = %zu value updates = %zu\n",
	       valuation_stores, value_stores);

	size_t num_atom_removes = _num_atom_removes;
	size_t num_atom_deletes = _num_atom_deletes;
	printf("ipfs-stats: atom remove requests = %zu total atom deletes = %zu\n",
	       num_atom_removes, num_atom_deletes);
	printf("\n");

	size_t num_get_nodes = _num_get_nodes;
	size_t num_got_nodes = _num_got_nodes;
	size_t num_rec_nodes = _num_rec_nodes;
	size_t num_get_links = _num_get_links;
	size_t num_got_links = _num_got_links;
	size_t num_rec_links = _num_rec_links;
	size_t num_get_insets = _num_get_insets;
	size_t num_get_inlinks = _num_get_inlinks;
	size_t num_node_inserts = _num_node_inserts;
	size_t num_link_inserts = _num_link_inserts;

	frac = 100.0 * num_got_nodes / ((double) num_get_nodes);
	printf("num_get_nodes=%zu num_got_nodes=%zu (%f pct) recursive=%zu\n",
	       num_get_nodes, num_got_nodes, frac, num_rec_nodes);

	frac = 100.0 * num_got_links / ((double) num_get_links);
	printf("num_get_links=%zu num_got_links=%zu (%f pct) recursive=%zu\n",
	       num_get_links, num_got_links, frac, num_rec_links);

	frac = num_get_inlinks / ((double) num_get_insets);
	printf("num_get_incoming_sets=%zu set total=%zu avg set size=%f\n",
	       num_get_insets, num_get_inlinks, frac);

	unsigned long tot_node = num_node_inserts;
	unsigned long tot_link = num_link_inserts;
	frac = tot_link / ((double) tot_node);
	printf("total stores for node=%lu link=%lu ratio=%f\n",
	       tot_node, tot_link, frac);

	// Store queue performance
	unsigned long item_count = _write_queue._item_count;
	unsigned long duplicate_count = _write_queue._duplicate_count;
	unsigned long flush_count = _write_queue._flush_count;
	unsigned long drain_count = _write_queue._drain_count;
	unsigned long drain_msec = _write_queue._drain_msec;
	unsigned long drain_slowest_msec = _write_queue._drain_slowest_msec;
	unsigned long drain_concurrent = _write_queue._drain_concurrent;
	int high_water = _write_queue.get_high_watermark();
	int low_water = _write_queue.get_low_watermark();
	bool stalling = _write_queue.stalling();

	double dupe_frac = duplicate_count / ((double) (item_count - duplicate_count));
	double flush_frac = (item_count - duplicate_count) / ((double) flush_count);
	double fill_frac = (item_count - duplicate_count) / ((double) drain_count);

	unsigned long dentries = drain_count + drain_concurrent;
	double drain_ratio = dentries / ((double) drain_count);
	double drain_secs = 0.001 * drain_msec / ((double) dentries);
	double slowest = 0.001 * drain_slowest_msec;

	printf("\n");
	printf("hi-water=%d low-water=%d stalling=%s\n", high_water,
	       low_water, stalling? "true" : "false");
	printf("write items=%lu dup=%lu dupe_frac=%f flushes=%lu flush_ratio=%f\n",
	       item_count, duplicate_count, dupe_frac, flush_count, flush_frac);
	printf("drains=%lu fill_fraction=%f concurrency=%f\n",
	       drain_count, fill_frac, drain_ratio);
	printf("avg drain time=%f seconds; longest drain time=%f\n",
	       drain_secs, slowest);

	printf("currently in_drain=%d num_busy=%lu queue_size=%lu\n",
	       _write_queue._in_drain, _write_queue.get_busy_writers(),
	       _write_queue.get_size());

	printf("current conn_pool free=%u of %d\n", conn_pool.size(),
	       _initial_conn_pool_size);

	// Some basic TLB statistics; could be improved;
	// The TLB remapping theory needs some work...
	// size_t noh = 0;
	// size_t remap = 0;

	printf("\n");
}

/* ============================= END OF FILE ================= */
