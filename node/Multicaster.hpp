/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_MULTICASTER_HPP
#define _ZT_MULTICASTER_HPP

#include <stdint.h>
#include <string.h>

#include <stdexcept>
#include <map>
#include <set>
#include <list>
#include <algorithm>

#include "Constants.hpp"
#include "Mutex.hpp"
#include "MulticastGroup.hpp"
#include "Address.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

/**
 * Multicast propagation algorithm core and database
 */
class Multicaster
{
public:
	Multicaster();
	~Multicaster();

	/**
	 * Add or renew a peer's subscription to a multicast group
	 *
	 * @param nwid Network ID
	 * @param a Address that LIKEd
	 * @param mg Multicast group
	 * @param now Current time
	 */
	void likesGroup(uint64_t nwid,const Address &a,const MulticastGroup &mg,uint64_t now);

	/**
	 * Bring a peer closer in terms of propagation priority
	 *
	 * @param nwid Network ID
	 * @param a Address to bring closer (e.g. due to unicast message)
	 * @param now Current time
	 */
	void bringCloser(uint64_t nwid,const Address &a);

	/**
	 * Erase entries for expired LIKEs and GOT records
	 */
	void clean();

	/**
	 * Multicast deduplicator
	 *
	 * This checks to see if a multicast GUID has been seen before. If not, it
	 * adds it to the history and returns false.
	 *
	 * @param nwid Network ID
	 * @param mcGuid Multicast GUID (sender address + sender unique ID)
	 * @return True if multicast IS a duplicate, false otherwise
	 */
	inline bool deduplicate(uint64_t nwid,uint64_t mcGuid)
		throw()
	{
		Mutex::Lock _l(_lock);
		_NetInfo &n = _nets[nwid];
		for(unsigned int i=0;i<ZT_MULTICAST_DEDUP_HISTORY_LENGTH;++i) {
			if (n.multicastHistory[i] == mcGuid)
				return true;
		}
		n.multicastHistory[n.multicastHistoryPtr++ % ZT_NETWORK_MULTICAST_DEDUP_HISTORY_LENGTH] = mcGuid;
		return false;
	}

	/**
	 * Pick next hops for a multicast by proximity
	 *
	 * The function or function object must return true if more hops are desired
	 * or false to stop finding new hops and return.
	 *
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param nextHopFunc Function to call for each address, search stops if it returns false
	 * @return Number of results returned through function
	 */
	template<typename F>
	inline unsigned int getNextHops(uint64_t nwid,const MulticastGroup &mg,F nextHopFunc)
	{
		Mutex::Lock _l(_lock);

		std::map< uint64_t,_NetInfo >::iterator n(_nets.find(nwid));
		if (n == _nets.end())
			return 0;
		std::map< MulticastGroup,std::list< Address > >::iterator p(n->second.proximity.find(mg));
		if (p == n->second.proximity.end())
			return 0;

		unsigned int cnt = 0;
		for(std::list< Address >::iterator a(p->second.begin());a!=p->second.end();++a) {
			if (!nextHopFunc(*a))
				break;
		}
		return cnt;
	}

private:
	// Information about a subscription
	struct _SubInfo
	{
		_SubInfo() :
			lastLike(0),
			proximitySlot() {}

		// Time of last MULTICAST_LIKE for this group
		uint64_t lastLike;

		// Slot in corresponding list in _proximity
		std::list< Address >::iterator proximitySlot;
	};

	// An address and multicast group tuple
	typedef std::pair< Address,MulticastGroup > _Subscription;

	// Multicast info for a given network
	struct _NetInfo
	{
		_NetInfo()
			throw()
		{
			memset(multicastHistory,0,sizeof(multicastHistory));
			multicastHistoryPtr = 0;
		}

		// Ring buffer of most recently injected multicast packet GUIDs
		uint64_t multicastHistory[ZT_MULTICAST_DEDUP_HISTORY_LENGTH];
		unsigned int multicastHistoryPtr;

		// Peer proximity ordering for peers subscribed to each group
		std::map< MulticastGroup,std::list< Address > > proximity;

		// Peer subscriptions to multicast groups
		std::map< _Subscription,_SubInfo > subscriptions;
	};

	std::map< uint64_t,_NetInfo > _nets;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
