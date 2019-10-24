/*
 * IPFSValues.cc
 * Save and restore of atom values.
 *
 * Copyright (c) 2008,2009,2013,2017,2019 Linas Vepstas <linas@linas.org>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <stdlib.h>
#include <unistd.h>

#include <opencog/atoms/base/Atom.h>
#include <opencog/atoms/atom_types/NameServer.h>
#include <opencog/atoms/value/FloatValue.h>
#include <opencog/atoms/value/LinkValue.h>
#include <opencog/atoms/value/StringValue.h>
#include <opencog/atoms/base/Valuation.h>
#include <opencog/atoms/truthvalue/TruthValue.h>

#include "IPFSAtomStorage.h"

using namespace opencog;

/* ================================================================== */

/// Delete the valuation, if it exists. This is required, in order
/// to prevent garbage from accumulating in the Values table.
/// It also simplifies, ever-so-slightly, the update of valuations.
void IPFSAtomStorage::deleteValuation(const Handle& key, const Handle& atom)
{
	throw SyntaxException(TRACE_INFO, "Not Implemented!");
}

/// Return a value, given by the VUID identifier, taken from the
/// Values table. If the value type is a link, then the full recursive
/// fetch is performed.
ValuePtr IPFSAtomStorage::getValue(VUID vuid)
{
	throw SyntaxException(TRACE_INFO, "Not Implemented!");
}

/// Return a value, given by the key-atom pair.
/// If the value type is a link, then the full recursive
/// fetch is performed.
ValuePtr IPFSAtomStorage::getValuation(const Handle& key,
                                      const Handle& atom)
{
	throw SyntaxException(TRACE_INFO, "Not Implemented!");
}

void IPFSAtomStorage::deleteValue(VUID vuid)
{
	throw SyntaxException(TRACE_INFO, "Not Implemented!");
}

/// Store ALL of the values associated with the atom.
void IPFSAtomStorage::store_atom_values(const Handle& atom)
{
	// First, build some json that encodes the key-value pairs
	ipfs::Json jvals;
	HandleSet keys = atom->getKeys();
	for (const Handle& key: keys)
	{
		// Special-case for TruthValues.  Avoid storing default TV's
		// so ast to not clog things up.
		if (key == tvpred)
		{
			TruthValuePtr tv(atom->getTruthValue());
			if (tv->isDefaultTV()) continue;
		}
		ValuePtr pap = atom->getValue(key);
		jvals[encodeValueToStr(key)] = encodeValueToStr(pap);
	}

	// Next, park that json with the atom.
	ipfs::Json jatom = encodeAtomToJSON(atom);
	jatom["values"] = jvals;

	// Store the thing in IPFS
	ipfs::Json result;
	ipfs::Client* conn = conn_pool.pop();
	conn->DagPut(jatom, &result);
	conn_pool.push(conn);

	std::string id = result["Cid"]["/"];
	std::cout << "Valued Atom: " << encodeValueToStr(atom)
	          << " CID: " << id << std::endl;
}

/// Get ALL of the values associated with an atom.
void IPFSAtomStorage::get_atom_values(Handle& atom)
{
	if (nullptr == atom) return;

// XXX FIXME  need to implement this stuff, but for now just
// return to avoid a throw.
	// throw SyntaxException(TRACE_INFO, "Not Implemented!");
}

/* ================================================================ */

void IPFSAtomStorage::getValuations(AtomTable& table,
                                   const Handle& key, bool get_all_values)
{
	rethrow();
	throw SyntaxException(TRACE_INFO, "Not Implemented!");
}

/* ============================= END OF FILE ================= */
