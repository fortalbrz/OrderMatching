/*
Implementation for a simple "exchange-like" cached order matcher, as described on README.txt

May 24th, 2024


Compilation flags:
	#define USE_CACHED_MATCHING_AT_ADD_ORDER       - uses the order matching at insertion time (see description bellow)
	#define EXTENDED_INTERFACE                     - enables extended interfaces for gets orders matched pairs (see description bellow)
	#define THROW_EXCEPTIONS                       - enables the validation of methods parameters throwing exceptions (DO NOT USE THIS with the unit tests)
	#define EXTENDED_TESTING                       - runs aditional unit tests
	#define SHOW_EXECUTION_TIMES                   - shows execution times on console (debug only)
	#define _DEBUG                                 - enables debug messages on console (automatically defined on Visual Studio IDE when selecting "Debug/Release" configuration)
	#define _CRT_SECURE_NO_WARNINGS                - disables some warnings on Visual Studio IDE


This code finds matching buy and sell orders in call auctionss, NOT taking into account order price (only the volume,
i.e., quantity of lots) or any financial related criteria (e.g. the number of transactions, filled quantities, moneyness, etc).
This scenario can be understood as and exchanged that only accepts "market orders" (i.e. at current market price, no stops or limits, etc).

Notice that specifing different criteria, a different algorithmical approach should be used.

Assuming only the time execution as the only criteria, this follow O(n) "Unsorted Greedy" order pair matching (Algorithm 1, page 31)
as described in Jonsou, V. and Steen, A. (2023) (available at <https://www.diva-portal.org/smash/get/diva2:1765801/FULLTEXT01.pdf>,
see file at "./docs/paper.pdf").

The algorithm was adapted to multithreading (with locking at order filling level)

There are also, two main available approaches for order matching:
	- searches for matches at "getMatchingSizeForSecurity()" (multithread), case USE_CACHED_MATCHING_AT_ADD_ORDER is NOT defined
	- searches for matches at "addOrder()", case macro USE_CACHED_MATCHING_AT_ADD_ORDER is defined

On second approach, the matches values are found at insertion time ("addOrder") are stored in cached.
Threrefore there is no computational effort on calling the critical method "getMatchingSizeForSecurity()"
(that works as a simple read-only property, i.e., O(1))


Remark: this is not a optimal code, just a C++17 exercise under the proposed problem (see discursion above).


Remark: notes on "order" data structure:

The problem interface expects a orders vector at "getAllOrders()"
There is  design trade-off between:
  - vector access interface on "getAllOrders()" (with O(1) using a internal vector state variable)
  - batch removal of orders on "cancelOrdersForUser()" and "cancelOrdersForSecIdWithMinimumQty()" interfaces

Current implementation uses a internal list of orders data structure in order to remove items
with O(1), and therefore the batch orders removal methods (above) can be O(n), otherwise they will be O(n.m).
The trade-off is that the current "getAllOrders()" method is O(n) instead of O(1) as expected.


Remark: getters and setters

There are two class properties defined only for testing purposes / performance comparision:
   - multiThread() / setMultiThread(): enable/disable multi-thread support
   - verbose() / setVerbose(): enable/disable full verbosity on debug mode (_DEBUG)



Jorge Albuquerque, Ph.D
	- https://www.linkedin.com/in/jorgealbuquerque/
	- book call: https://bit.ly/bookjorge
	- teams: https://bit.ly/chat_jorge_teams
	- whatsapp: https://bit.ly/wa_jorge
	- jorgealbuquerque@gmail.com


Copyright (c) 2024, Jorge Albuquerque (jorgealbuquerque@gmail.com)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Hossein Moein and/or the DataFrame nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Hossein Moein BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "OrderCache.h"
#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <unordered_set>
#include <shared_mutex>
#ifdef THROW_EXCEPTIONS
#include <stdexcept>
#endif
#include <mutex>
#include <thread>
#include <type_traits>


/// <summary>
/// Adds the order into current order cache.
/// Remark: O(1)
/// </summary>
/// <param name="order">The order.</param>
void OrderCache::addOrder(Order order) {

	// thread-safe lock (writting data)
	write_lock lock = lockForUpdateOrders();

	#ifdef SHOW_EXECUTION_TIMES
	auto start = TestUtils::tic();
	#endif

	#ifdef _DEBUG
	utils::osyncstream out;
	if (_verbose)
		out << "adding new order [OrderCache::addOrder()]\n";
	#endif // _DEBUG
		
	// parameters validation: checks for duplicated orders 
	if (exists(order.orderId())) {
		#ifdef THROW_EXCEPTIONS
		throw std::invalid_argument("error adding order: duplicated order id");
		#else
		return;
		#endif // THROW_EXCEPTIONS
	}
	
	// stores the order (remark: uses 'std::move' for not coping the Order instance)
	_orders.push_front(std::move(order));

	// gets the order interator/pointer
	order_ptr ptr = _orders.begin();
	
	// stores the indexes for fast access - O(1)
	_orderIndex.insert({ ptr->orderId(), ptr}); // index by order ID  (orderId => order ptr [1:1])		
	_userOrdersIndex[ptr->user()].insert(ptr->orderId()); // index by user (user => orderId [1:n])
	_securityOrdersIndex[ptr->securityId()].insert(ptr->orderId()); // index by security (securityId => orderId [1:n])

	// stores indexes specialized for matching algorithim (critical path - O(1))	
	order_list& matchIndex = isBuySide(ptr) ? 
		_securityLongOrdersIndex[ptr->securityId()] : 
		_securityShortOrdersIndex[ptr->securityId()];
	
	matchIndex.push_back(ptr);
		
	#ifdef _DEBUG
	if (_verbose) {
		out << "OrderCache{size: " << size() << "} - order added: " << ptr->str() << '\n';
		out.flush();
	}
	#endif // _DEBUG

	#ifdef USE_CACHED_MATCHING_AT_ADD_ORDER		
	//
	// does order matching (order filling) 
	// as the orders are inserted (and cache matched values)
	//	
	matchOrderInCache(ptr, false);	
	#endif

	#ifdef SHOW_EXECUTION_TIMES
	TestUtils::toc(start, "adding order execution time: ");
	#endif
}


/// <summary>
/// Cancels the order by specified Id (thread-safe).
/// Remark: O(1)
/// </summary>
/// <param name="orderId">The order identifier.</param>
void OrderCache::cancelOrder(const std::string& orderId) {
	
	#ifdef SHOW_EXECUTION_TIMES
	auto start = TestUtils::tic();
	#endif

	// thread-safe lock (writting data)
	write_lock lock = lockForUpdateOrders();

	cancelSingleOrder(orderId, false);	

	#ifdef SHOW_EXECUTION_TIMES
	TestUtils::toc(start, "cancel order execution time: ");
	#endif

}


/// <summary>
/// Cancels the orders for the specified user  (thread-safe).
/// </summary>
/// <param name="user">The user.</param>
void OrderCache::cancelOrdersForUser(const std::string& user) {
	
	// thread-safe lock (writting data)
	write_lock lock = lockForUpdateOrders();

	#ifdef SHOW_EXECUTION_TIMES
	auto start = TestUtils::tic();
	#endif

	#ifdef _DEBUG
	utils::osyncstream out;
	if (_verbose)
		out << "\nCanceling all orders by user: '" << user << "' [OrderCache::cancelOrdersForUser()]\n";
	#endif // _DEBUG
	
	// parameters validation: checks for nonexistent user
	if (!_userOrdersIndex.count(user)) {
		#ifdef _DEBUG
		if (_verbose)
			out << "\nNo orders for user: '" << user << "'\n";
		#endif // _DEBUG

		#ifdef THROW_EXCEPTIONS
		throw std::range_error("error cancelling order for user: user not found!");
		#else		
		return;
		#endif 
	}

	// gets all orders from user with O(1)
	orders_keys ordersKeys = _userOrdersIndex[user];

	#ifdef _DEBUG
	if (_verbose) {
		out << " - Users orders:\n";
		for (auto& orderId : ordersKeys)
			out << "   " << orderId << '\n';
		out.flush();
	}
	#endif // _DEBUG

	cancelOrders(ordersKeys, 0);

	#ifdef SHOW_EXECUTION_TIMES
	TestUtils::toc(start, "cancel all orders from user execution time: ");
	#endif
}


/// <summary>
/// Cancels the orders for sec identifier with minimum quantity of lots (thread-safe).
/// </summary>
/// <param name="securityId">The security identifier.</param>
/// <param name="minQty">The minimum size to cancel the order.</param>
void OrderCache::cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty) {
	
	// thread-safe lock (writting data)
	write_lock lock = lockForUpdateOrders();

	#if defined(SHOW_EXECUTION_TIMES)	
	auto start = TestUtils::tic();	
	#endif

	#ifdef _DEBUG
	utils::osyncstream out;

	if (_verbose) {
		out << "\nCanceling all orders by security: '" << securityId << "' ";
		if (minQty > 0)
			out << "[min: " << minQty << "] ";
		out << "[OrderCache::cancelOrdersForSecIdWithMinimumQty()]\n";
	}
	#endif // _DEBUG

	// parameters validation: checks for nonexistent security
	if (!_securityOrdersIndex.count(securityId)) {
		#ifdef _DEBUG
		if (_verbose)
			out << "\nNo orders for security: '" << securityId << "'\n";
		#endif // _DEBUG

		#ifdef THROW_EXCEPTIONS
		throw std::range_error("error cancelling order for security: security id not found");
		#else
		return;
		#endif
	}
	
	// gets all orders from security with O(1)
	orders_keys ordersKeys = _securityOrdersIndex[securityId];
	
	#ifdef _DEBUG
	if (_verbose) {
		out << " - Security orders:\n";
		for (auto& orderId : ordersKeys)
			out << "   " << orderId << '\n';
		out.flush();
	}
	#endif // _DEBUG

	cancelOrders(ordersKeys, minQty);
	
	#ifdef SHOW_EXECUTION_TIMES
	TestUtils::toc(start, "cancel all orders from security execution time: ");
	#endif
}


/// <summary>
/// Gets the matching size for security.
/// Remark: O(1)
/// </summary>
/// <param name="securityId">The security identifier.</param>
/// <returns></returns>
unsigned int OrderCache::getMatchingSizeForSecurity(const std::string& securityId) {

	// thread-safe lock (reading data)
	read_lock lock = lockForReadOrders();

	#if defined(_DEBUG) || defined(SHOW_EXECUTION_TIMES)
	auto start = debug::TestUtils::tic();
	utils::osyncstream out;
	#endif

	// parameters validation: checks for nonexistent security
	if (!_securityOrdersIndex.count(securityId)) {
		#ifdef _DEBUG
		if (_verbose)
			out << "\nNo orders for securitu '" << securityId << "'\n";
		#endif // _DEBUG

		#ifdef THROW_EXCEPTIONS
		throw std::range_error("error matching orders for security: security id not found");
		#else
		return 0;
		#endif
	}
	
	int qty = 0;

#ifndef USE_CACHED_MATCHING_AT_ADD_ORDER
	//
	// single thread appproach - O(n)
	//

	#ifdef _DEBUG
	if (_verbose)
		out << "evaluating all matches at 'getMatchingSizeForSecurity()' call.\n";
	#endif // _DEBUG

	//
	// The order matching precedure should only be called if
	// the matching at insertion mode is not enabled
	// i.e., USE_CACHED_MATCHING_AT_ADD_ORDER is not defined
	//	
	if (!_multiThread || size() < 2) {
		//
		// single thread / iteractive approach - O(n) (one loop per buy order)
		// (performance comparison purposes only)
		//
		for (order_ptr& order : _securityLongOrdersIndex[securityId])
			qty += matchOrderInCache(order, false);
	}
	else {
		//
		// multithread approach - O(n) (i.e., one thread per buy order)
		//		
		_ThreadPool.clear();
		unsigned int nthreads = std::thread::hardware_concurrency();		

		for (order_ptr& order : _securityLongOrdersIndex[securityId]) {
			{
				// adds new thread to the pool
				std::lock_guard<std::mutex> lock(_ThreadPoolMutex);
				_ThreadPool.emplace_back(
					std::thread(&OrderCache::matchOrderInPool, this, order));
			}
			while (true) {
				// wait for a slot to be freed.
				std::this_thread::yield();
				std::lock_guard<std::mutex> lock(_ThreadPoolMutex);
				if (_ThreadPool.size() < nthreads)
					break;
			}
		}

		// wait for the last tasks to be done
		while (true) {
			std::this_thread::yield();
			std::lock_guard<std::mutex> lock(_ThreadPoolMutex);
			if (_ThreadPool.empty())
				break;			
		}

		//
		//// Simplistic approach (no thread pool)		
		// 
		//std::vector<std::thread> threads;
		//for (order_ptr& order : _securityLongOrdersIndex[securityId]) {
		//	threads.push_back(std::thread(&OrderCache::matchOrderInCache, this, order, true));
		//}
		//// waits for all threads to finish
		//for (auto& thread : threads) 
		//	thread.join();
		
		// returns the values (in cache after the matches)
		qty = getMatchedQuantityInCache(securityId);
	}
		
#else // USE_CACHED_MATCHING_AT_ADD_ORDER

	#ifdef _DEBUG
	if (_verbose) {
		out << "gets matched order value from cached - O(1)\n";
		out.flush();
	}
	#endif // _DEBUG

	// values are already stored in cache (no need to do anything)
	qty = getMatchedQuantityInCache(securityId);

#endif // USE_CACHED_MATCHING_AT_ADD_ORDER
		
	#ifdef SHOW_EXECUTION_TIMES
	TestUtils::toc(start, "finding order matches security execution time: ");
	#endif

	return qty;
}


/// <summary>
/// Gets all orders in current cache instance as a vector
/// </summary>
/// <returns>
/// vector of orders
/// </returns>
std::vector<Order> OrderCache::getAllOrders() const {
	
	#ifdef SHOW_EXECUTION_TIMES
	auto start = TestUtils::tic();
	#endif

	// thread-safe lock (reading data)
	read_lock lock = lockForReadOrders();
	
	//
	// retruns cached list to vector	
	// 
	// This is a design trade-off between:
	//   - vector access interface on "getAllOrders()" (current method)
	//   - batch removal of orders on "cancelOrdersForUser()" and "cancelOrdersForSecIdWithMinimumQty()"
	//
	// The list data structure enabled to remove items with O(1), and therefore the 
	// orders batch removal methods can be O(n), otherwise they will be O(n.m).
	// The trade-off is that the current method is O(n) instead of O(1) if orther were at a "std::vector"
	//
	return std::vector<Order>(_orders.cbegin(), _orders.cend());

	#ifdef SHOW_EXECUTION_TIMES
	TestUtils::toc(start, "get all orders execution time: ");
	#endif
}


/// <summary>
/// Gets the order by the specified order identifier.
/// ** Convenience accessor **
/// remark: ** this is NOT required for the proposed problem itself, just a "aditional feature"... **
/// </summary>
/// <param name="orderId">The order identifier.</param>
/// <returns></returns>
const Order& OrderCache::operator[] (const std::string& orderId) {
	
	return getOrder(orderId);
}


/// <summary>
/// Gets order by specified order id.
/// remark: O(1)
/// remark: ** this is NOT required for the proposed problem itself, just a "aditional feature"... **
/// </summary>
/// <param name="user">The order id.</param>
/// <returns>the order</returns>
Order& OrderCache::getOrder(const std::string& orderId) const {	
	
	#ifdef SHOW_EXECUTION_TIMES
	auto start = TestUtils::tic();
	#endif

	read_lock lock = lockForReadOrders();

	// parameters validation: checks for nonexistent orders
	#ifdef THROW_EXCEPTIONS		
	if (!exists(orderId))
		throw std::range_error("order not found");	
	#endif

	// gets order by index - O(1)
	return *_orderIndex.at(orderId);

	#ifdef SHOW_EXECUTION_TIMES
	TestUtils::toc(start, "get order by id execution time: ");
	#endif
}


/// <summary>
/// Gets all orders matches by specified security identifier
/// </summary>
/// <param name="securityId">The security identifier.</param>
/// <returns></returns>
std::vector<OrderFill> OrderCache::getAllOrderMatches() const {

	read_lock lock = lockForReadOrders();

	// moves cached internal list to vector - O(n), see comments on
	// code of OrderCache::getAllOrders()
	return std::vector<OrderFill>(_orderMatches.cbegin(), _orderMatches.cend());
}


/// <summary>
/// Gets all orders matches by specified security identifier
/// </summary>
/// <param name="securityId">The security identifier.</param>
/// <returns></returns>
std::vector<OrderFill> OrderCache::getOrderMatchesBySecurity(const std::string& securityId) const {

	read_lock lock = lockForReadOrders();

	// parameters validation: checks for nonexistent security
	if (!_securityOrdersIndex.count(securityId))
		return std::vector<OrderFill>();

#ifndef USE_CACHED_MATCHING_AT_ADD_ORDER	
	//
	// not implemented for this case (lack of time... kkkk) 
	//
	return std::vector<OrderFill>();
#else

	// returns cached values
	auto& orders = _orderMatches;

	// moves cached internal list to vector - O(n), see comments on
	// code of OrderCache::getAllOrders()
	return std::vector<OrderFill>(orders.cbegin(), orders.cend());

#endif	

};


/// <summary>
/// Checks the order existence by the specified order identifier (Id).
/// remark: O(1)
/// remark: ** this is NOT required for the proposed problem itself, just a "aditional feature"... **
/// </summary>
/// <param name="orderId">The order identifier.</param>
/// <returns>
/// True case order is found, False otherwise
/// </returns>
const bool OrderCache::exists(const std::string& orderId) const {

	// checks index map hash with O(1) - using "map.contains_key()"
	return _orderIndex.count(orderId) > 0;
}


/// <summary>
/// Gets the number of orders in current cache instance.
/// remark: O(1)
/// remark: ** this is NOT required for the proposed problem itself, just a "aditional feature"... **
/// </summary>
/// <returns></returns>
const size_t OrderCache::size() const {

	// gets cache size with O(1) - using "map.size()"
	return _orderIndex.size();
}



/********************************************************************************************************************************

															PRIVATE MEMBERS

********************************************************************************************************************************/



/// <summary>
/// Cancels the order [PRIVATE]
/// remark: O(1)
/// </summary>
/// <param name="orderId">The order identifier.</param>
/// <param name="minQty">Only cancel the specified order if the order quantity if greather than minQty value.</param>
void OrderCache::cancelSingleOrder(const std::string& orderId, unsigned int minQty, bool lockOrder) {
	
	#if defined(_DEBUG) || defined(SHOW_EXECUTION_TIMES)
	utils::osyncstream out;
	#endif // _DEBUG

	// parameters validation: checks for nonexistent orders
	if (!_orderIndex.count(orderId)) {
		#ifdef _DEBUG
		if (!exists(orderId) && !lockOrder)
			out << "WARNING: order id not found: '" << orderId << "'\n";
		#endif // _DEBUG
		#ifdef THROW_EXCEPTIONS	
		throw std::invalid_argument("error cancelling order: order id not found");
		#else
		return;
		#endif // THROW_EXCEPTIONS	
	}

	// only locks on multi-thread 
	//lockOrder = lockOrder && _multiThread;

	#ifdef _DEBUG
	if (_verbose) {
		out << " - cancelling order id: '" << orderId << "'";
		if (minQty > 0)
			out << "[min:" << minQty << "]";
		out << " [cancelSingleOrder()] " << (lockOrder ? "[lock]\n" : "[no lock]\n");
	}
	#endif // _DEBUG

	// retrives order pointer (as iterator) by "orderId" with O(1)  
	// (fast order access)
	order_ptr ptr = _orderIndex[orderId];
		
	if (minQty > 0 && ptr->qty() < minQty)
		// checkes for mininum quantity of lots criteria for cancelation, if applicable.
		return;

	if (lockOrder) 
		ptr->lock();

	//std::mutex _lockTest;
	//std::lock_guard<std::mutex> guard(_lockTest);
	
	// removes order cached indexes	with O(1)
	_userOrdersIndex[ptr->user()].erase(orderId);
	_securityOrdersIndex[ptr->securityId()].erase(orderId);
	
	// removes order indexes (optimized for the order matching procedure)
	std::shared_mutex& mtx = isBuySide(ptr) ?
		_longOrdersMutex : _shortOrdersMutex;

	order_list& index = isBuySide(ptr) ?
		_securityLongOrdersIndex[ptr->securityId()] :
		_securityShortOrdersIndex[ptr->securityId()];
		
	index.erase(std::remove_if(index.begin(), index.end(),
		[&orderId](auto& o) { return o->orderId() == orderId; }));
		
	// removes the main order index with O(1) 
	_orderIndex.erase(orderId);

	if (lockOrder) 
		ptr->unlock();
	
	// removes the order instance itself with O(1)	
	_orders.erase(ptr);
	
	#ifdef _DEBUG
	if (exists(orderId)) 
		out << "WARN: order not deleted: " << orderId << "\n";			
	else if (_verbose)
		out << "cache{size: " << size() << "} - order deleted: " << orderId << "\n";
	
	out.flush();
	#endif // _DEBUG
}



/// <summary>
/// Cancels the orders (uses multithreading if required) [PRIVATE - auxiliar function]
/// </summary>
/// <param name="orders">The order identifiers.</param>
/// <param name="minQty">Only cancel the specified order if the order quantity if greather than minQty value.</param>
void OrderCache::cancelOrders(orders_keys& orders, unsigned int minQty) {

	// number of elements in the set of orders identifiers to remove
	const unsigned int size = (unsigned int)orders.size();
	// number of threads
	const unsigned int nthreads = _multiThread ? std::thread::hardware_concurrency() : 1;
	// the set will be processed in chunks os size:
	const unsigned int chunkSize = (nthreads == 1 || size <= nthreads) ? size : 1 + ((size - 1) / nthreads);
	// single thread approach: if required OR the chunk size 
	// of the vector does not compensate for the thread overhead!		
	bool singleThreadDelete = !_multiThread || chunkSize < DELETE_CHUNK_SIZE;

	#ifdef _DEBUG
	utils::osyncstream out;
	out << "Cancel orders [OrderCache::cancelOrders() - private]: \n";
	out << " - nthreads: " << nthreads << "\n";
	out << " - chunk size: " << chunkSize << "\n";
	out << " - multithread: " << (singleThreadDelete ? "no\n" : "yes\n");
	out.flush();
	#endif // _DEBUG
	
	singleThreadDelete = true;

	if (singleThreadDelete) {
		// single thread approach: if required OR the chunk size 
		// of the vector does not compensate for the thread overhead!		
		for (auto& orderId : orders) 			
			cancelSingleOrder(std::ref(orderId), minQty, false);				
	}
	else
	{
		//
		// multithread approach
		//		
		std::vector<std::thread> threads;

		// creates a "removal" thread for each order chunk set (of size "chunkSize")
		utils::chunks(orders.begin(), orders.end(), chunkSize,
			[&](orders_keys::iterator start, orders_keys::iterator end) {
				threads.push_back(std::thread(&OrderCache::cancelOrdersRange, this, orders, start, end, minQty));
			});
				
		// waits for all threads to finish
		for (auto& thread : threads)
			thread.join();
	}	
}


/// <summary>
/// Cancels the orders on the specified range
/// [PRIVATE - auxiliar function: used only at OrderCache::cancelOrders()]
/// 
/// Remark: defined for order removal multi-threading approach (maintability and readability)
/// </summary>
/// <param name="orders">The order identifiers.</param>
/// <param name="start">start iterator.</param>
/// <param name="start">end iterator.</param>
/// <param name="minQty">Only cancel the specified order if the order quantity if greather than minQty value.</param>
void OrderCache::cancelOrdersRange(orders_keys& orders, orders_keys::iterator& start, orders_keys::iterator& end, unsigned int minQty) {

	#ifdef _DEBUG
	utils::osyncstream out;
	out << " - starting thread for deleting orders - start id: '" << *start << "' \n";
	out.flush();
	#endif // _DEBUG

	// for each order between the iterators (removes sequentially the chunk)
	for (orders_keys::iterator it = start; it != end; it++) {		
		cancelSingleOrder(*it, minQty, true);
	}	
}



/// <summary>
/// 
/// Finds matches for the specified order and store it in cache [PRIVATE]
/// 
/// This is the O(n) implementation of "Unsorted Greedy" order pair matching (Algorithm 1, page 31)
/// as described in Jonsou, V. and Steen, A. (2023), 
/// available at <https://www.diva-portal.org/smash/get/diva2:1765801/FULLTEXT01.pdf> 
/// 
/// This function can used to:
///    - searches for matches at "getMatchingSizeForSecurity()" (multithread), case USE_CACHED_MATCHING_AT_ADD_ORDER is NOT defined
///    - searches for matches at "addOrder()", case macro USE_CACHED_MATCHING_AT_ADD_ORDER is defined
///      
/// On second approach, the matches values are stored in cached, and the method "getMatchingSizeForSecurity()"
/// itself workes with O(1)
/// 
/// see file at "./docs/paper.pdf"
/// </summary>
/// <param name="orderId">The order identifier.</param>
unsigned int OrderCache::matchOrderInCache(order_ptr& order, bool lockOrder) {

	#ifdef _DEBUG 
	auto start = debug::TestUtils::tic();
	utils::osyncstream out;
	if (_verbose)
		out << " - OrderCache::matchOrderInCache()\n";
	#endif // _DEBUG

	// locks the current order only!! 
	// Others order can be written by other threads!
	if (lockOrder)
		order->lock();

	if (order->isFilled()) {
		// already filled: nothing to do!
		// release order and return immediately (0 matched lots)
		#ifdef _DEBUG 
		if (_verbose)
			out << "  order already filled! [working lots: " << order->workingQty() << "\n";
		#endif // _DEBUG

		if (lockOrder)
			order->unlock();
		
		return 0;
	}

	#ifdef _DEBUG	
	if (_verbose) {
		debug::TestUtils::print(out, "*");
		out << " - searching for matches (i.e. after add) and storing it in cache: \n   " << order->str() << '\n';
	}
	#endif // _DEBUG

	// gets possible counterparties for the specified order 
	// (defined by order security and "buy/sell" side)
	const bool isBuy = isBuySide(order);	

	// - uses sell counterparties to matches buy orders
	// - uses buy counterparties to matches sell orders
	order_list counterParties = isBuy ?
		_securityShortOrdersIndex[order->securityId()] :
		_securityLongOrdersIndex[order->securityId()];


	if (!counterParties.size()) {
		// no counterparties to match!		
		#ifdef _DEBUG
		if (_verbose) {
			out << "   No " << (isBuy ? "Sell" : "Buy") << " counterparties for match on security: " << order->securityId() << '\n';
			debug::TestUtils::print(out, "*");
		}
		#endif // _DEBUG
		// release order and return immediately (0 matched lots)
		if (lockOrder)
			order->unlock();
		return 0;
	}

	#ifdef _DEBUG
	// list all counterparties for specified order
	if (_verbose) {
		out << "   avaliable (possible) counterparties:\n";
		debug::TestUtils::print(out, counterParties, 6);
	} 
	#endif // _DEBUG


	unsigned int matchedQuantity = 0;
	int counter = 0;

	// for each possible counterparty order
	for (order_ptr& counterPartyOrder : counterParties) {
		
		// locks the counterparty order candidate
		if (lockOrder)
			counterPartyOrder->lock();
		
		#ifdef _DEBUG
		if (_verbose)
			out << "  checking counterparty: " << counterPartyOrder->str() << '\n';
		#endif // _DEBUG	 

		// company cannot trade with itself (no internal trades): skip orders from same company!
		if (counterPartyOrder->isFilled() 
			|| order->company() == counterPartyOrder->company()) {

			#ifdef _DEBUG
			if (_verbose) {
				out << "   skipping conterparty: ";
				if (order->company() == counterPartyOrder->company())
					out << "same company [" << order->company() << "]\n";
				else
					out << "no remaining position [" << counterPartyOrder->workingQty() << "]\n";
			}
			#endif

			// unlock and skip counterparty
			if (lockOrder)
				counterPartyOrder->unlock();
			continue;
		}
		// gets the tradable quantity of lots between current buy and sell order
		// i.e., the counterparties should have at LEAST the same working quantity in order to trade!
		unsigned int qty = std::min(order->workingQty(), counterPartyOrder->workingQty());
		if (qty == 0) {

			#ifdef _DEBUG
			if (_verbose) {
				out << " - no matches (0 lots) for:\n";
				out << "       order:        " << order->str() << '\n';
				out << "       counterparty: " << counterPartyOrder->str() << '\n';
			}
			#endif // _DEBUG
			counterPartyOrder->unlock();
			continue;
		}

		#ifdef _DEBUG
		if (_verbose) {
			out << " - Matched " << qty << " lots:\n";
			out << "   order:        " << order->str() << '\n';
			out << "   counterparty: " << counterPartyOrder->str() << '\n';
		}
		#endif // _DEBUG

		// partially fill orders (i.e., on "qty" lots)
		order->fillLots(qty);
		counterPartyOrder->fillLots(qty);
		counterPartyOrder->unlock();
	
		matchedQuantity += qty;

#ifdef EXTENDED_INTERFACE
		//
		// stores deal information - Extended feature (not required for the proposed problem)
		//
		OrderFill& filledOrder = isBuy ?
			OrderFill{ order->orderId(), counterPartyOrder->orderId(), qty } :
			OrderFill{ counterPartyOrder->orderId(), order->orderId(), qty };
		
		// just stores deal information
		_orderMatches.push_back(filledOrder);

#endif //EXTENDED_INTERFACE
		

		#ifdef _DEBUG
		if (_verbose) {
			out << " - after match : \n";
			out << "   order:        " << order->str() << '\n';
			out << "   counterparty: " << counterPartyOrder->str() << '\n';
			out << "   total matched quantity on order '" << order->orderId() << "': " << matchedQuantity << " lots\n";
		}
		#endif // _DEBUG

		if (order->isFilled()) {
			// the order is filled: all work is done!!!
			break;
		}
	}
		
	if (lockOrder)
		order->unlock();

	// stores matched quantity of lots on cache (thread safe writing)
	_matchedQuantityMutex.lock();
	_matchedQuantity[order->securityId()] += matchedQuantity;
	_matchedQuantityMutex.unlock();


	#ifdef _DEBUG
	if (_verbose) {
		out << "   final matched quantity for order '" << order->orderId() << "': " << matchedQuantity << " lots.\n";
		out << "   total matched quantity (cached) for security '" << order->securityId() << "': " << getMatchedQuantityInCache(order->securityId()) << " lots.\n";
		debug::TestUtils::toc(out, start, "   elapsed time: ");
		debug::TestUtils::print(out, "*");
	}
	#endif // _DEBUG

	// ************* HERE *****************

	return matchedQuantity;
}


void OrderCache::matchOrderInPool(order_ptr& ptr){

	matchOrderInCache(std::ref(ptr), true);

	// task is done, remove thread from pool
	if (_ThreadPool.size() == 0)
		return;

	std::lock_guard<std::mutex> lock(_ThreadPoolMutex);
	_ThreadPool.erase(
		std::find_if(_ThreadPool.begin(), _ThreadPool.end(),
			[](std::thread& x)
			{
				if (x.get_id() == std::this_thread::get_id())
				{
					// detach inside running thread
					x.detach(); 
					return true;
				}
				return false;
			})
	);
}


/// <summary>
/// Gets the current cached matched quantity by security (thread-safe).
/// </summary>
/// <param name="securityId">The security identifier.</param>
/// <returns></returns>
unsigned int OrderCache::getMatchedQuantityInCache(const std::string& securityId) const {
	
	// locks for reading values
	_matchedQuantityMutex.lock_shared();	

	// remark: retunrs 0 in case of the security identifier was not found
	unsigned int value = (_matchedQuantity.count(securityId) == 0) ? 0 :
		_matchedQuantity.at(securityId);

	_matchedQuantityMutex.unlock_shared();

	return value;
}
