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

Remark: project was keept on 2 files only for sending/testing easyness


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
#pragma once

/*----------------------------------------------------------------
    Uncomment this if required (see file header for reference)
 ----------------------------------------------------------------*/
#define USE_CACHED_MATCHING_AT_ADD_ORDER
// #define EXTENDED_INTERFACE
// #define EXTENDED_TESTING
// #define SHOW_EXECUTION_TIMES
// #define THROW_EXCEPTIONS
// #define _DEBUG
#define _CRT_SECURE_NO_WARNINGS

/*----------------------------------------------------------------
    MACRO PARAMETERS
 ----------------------------------------------------------------*/
constexpr unsigned int DELETE_CHUNK_SIZE = 64; // this is arbitrary


#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <chrono>
#include <sstream>
#include <iostream>
#include <mutex>

#ifdef THROW_EXCEPTIONS
#include <stdexcept>
#endif



/*----------------------------------------------------------------
	MULTITHREADING RESOURCES AND UTILITIES
 ----------------------------------------------------------------*/

namespace utils {

    /// <summary>
    /// multithread string buffer (C++17 implementation)
    /// </summary>
    /// <seealso cref="std::ostringstream" />
    class syncbuffer : public std::stringbuf
    {
    public:

        syncbuffer() = default;

        ~syncbuffer() {
            sync();
        }

        /// <summary>
        /// Synchronizes this instance (writes to console - thread-safe.
        /// </summary>
        /// <returns></returns>
        int sync()
        {
            // when the buffer is sync, send to console in thread-safe fashion
            static std::mutex _mutex;
            std::lock_guard<std::mutex> guard(_mutex);
            std::cout << str();
        }

    private:
        static std::mutex _mutex;
    };


    /// <summary>
    /// multithread string stream (a.k.a., "std::cout") (C++17 implementation)
    /// </summary>
    /// <seealso cref="std::ostringstream" />
    class osyncstream : public std::ostringstream
    {
    public:

        osyncstream() : std::ostringstream() { }

        ~osyncstream() {
            // when the object is destroyed, send to buffered stream 
            // to console thread-safelly
            flush();
        }

        /// <summary>
        /// Flushes this instance (writes to console - thread-safe.
        /// </summary>
        /// <returns></returns>
        /// </summary>
        void flush() {
            // thread-safe flush
            static std::mutex _mutex;
            std::lock_guard<std::mutex> guard(_mutex);
            std::cout << this->str();
            // clear string buffer
            this->clean();
        }

        /// <summary>
        /// Cleans this instance.
        /// </summary>
        void clean() {
            // clear string buffer
            this->str(std::string());
            this->clear();
        }

        /// <summary>
        /// Add value to stream
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name=""></param>
        template <typename T>
        void add(const T&) {
            ((std::ostringstream)(*this)).operator<<(t);
        }


        template <typename T>
        friend osyncstream& operator<<(osyncstream& os, const T& value) {
            ((std::ostringstream&)os) << value;
            return os;
        }

    private:
        static std::mutex _mutex;
    };


    /// <summary>
    /// Process an interator in chunks of the specified size
    /// </summary>
    /// <typeparam name="Iterator">iterator type</typeparam>
    /// <typeparam name="Func">functor type</typeparam>
    /// <typeparam name="Distance">chunk size type</typeparam>
    /// <param name="begin">starting interator</param>
    /// <param name="end">finishing interator</param>
    /// <param name="chunkSize">chunk size</param>
    /// <param name="functor">processing functor</param>
    /// <param name="maxChunks">[optional] number max of chunks to process (default: all)</param>
    template<typename Iterator, typename Func, typename Distance>
    void chunks(Iterator begin, Iterator end, Distance chunkSize, Func functor, unsigned int maxChunks = UINT_MAX) {
        Iterator chunkBegin;
        Iterator chunkEnd;
        chunkEnd = chunkBegin = begin;
        unsigned int counter = 1;

        do {
            // gets the chunk end position
            if (std::distance(chunkEnd, end) < chunkSize)
                chunkEnd = end;
            else
                std::advance(chunkEnd, chunkSize);

            // process current chunk
            functor(chunkBegin, chunkEnd);

            // next chunk
            chunkBegin = chunkEnd;
        } while (std::distance(chunkBegin, end) > 0 && counter++ < maxChunks);
    }
}


/*----------------------------------------------------------------
    PROBLEM INTERFACE
 ----------------------------------------------------------------*/


/// <summary>
/// Order Transfer Object
/// </summary>
class Order
{
 
 public:
         
     /// <summary>
     /// Initializes a new instance of the <see cref="Order"/> class.
     /// </summary>
     /// <param name="ordId">The ord identifier.</param>
     /// <param name="secId">The sec identifier.</param>
     /// <param name="side">The side.</param>
     /// <param name="qty">The qty.</param>
     /// <param name="user">The user.</param>
     /// <param name="company">The company.</param>
     Order(
      const std::string& ordId,
      const std::string& secId,
      const std::string& side,
      const unsigned int qty,
      const std::string& user,
      const std::string& company)
      : m_orderId(ordId),
        m_securityId(secId),
        m_side(side),
        m_qty(qty),
        m_user(user),
        m_company(company) {
      m_workingQty = qty; 
      m_mutex = std::make_shared<std::shared_mutex>();
      m_locked = false;
  }
  
  /// <summary>
  /// Gets the order unique identifier.
  /// </summary>
  /// <returns></returns>
  std::string orderId() const { return m_orderId; }  

  /// <summary>
  /// Gets the security unique identifier.
  /// </summary>
  /// <returns></returns>
  std::string securityId() const { return m_securityId; }  

  /// <summary>
  /// Gets order sides ("Buy"/"Sell").
  /// </summary>
  /// <returns></returns>
  std::string side() const { return m_side; }  

  /// <summary>
  /// Gets the user identifier .
  /// </summary>
  /// <returns></returns>
  std::string user() const { return m_user; }  

  /// <summary>
  /// Gets the Company identifier
  /// </summary>
  /// <returns></returns>
  std::string company() const { return m_company; }  

  /// <summary>
  /// Gets the order quantity of lots.
  /// </summary>
  /// <returns></returns>
  unsigned int qty() const { return m_qty; }
  
  //----------------------------------------------------------------

  /// <summary>
  /// Gets the number of working lots.
  /// </summary>
  /// <returns></returns>
  unsigned int workingQty() const { return m_workingQty; }
  
  /// <summary>
  /// Gets the number of filled lots.
  /// </summary>
  /// <returns></returns>
  unsigned int filledQty() const { return m_qty - m_workingQty; }
    
  /// <summary>
  /// Subtracts the quantity of working lots to the current order.
  /// </summary>
  /// <param name="qty">The qty.</param>
  inline void fillLots(const unsigned int& qty) {
      if (m_workingQty > qty)
          m_workingQty -= qty;
      else
          m_workingQty = 0;
  }

  /// <summary>
  /// Adds the specified quantity of working lots to the current order.
  /// </summary>
  /// <param name="qty">The qty.</param>
  inline void unfillLots(const unsigned int& qty) {
      m_workingQty += qty;
      if (m_workingQty > m_qty)
          m_workingQty = m_qty;
  }
  
  /// <summary>
  /// Return true if the order is fully filled (i.e. no working lots), otherwise false.
  /// </summary>
  inline bool isFilled() const { return m_workingQty == 0; }

  /// <summary>
  /// Resets the order matches.
  /// </summary>
  inline void resetFills() { m_workingQty = m_qty; }
  
  // ** convenience accessor **
  unsigned int operator+=(unsigned int qty) { unfillLots(qty); return m_workingQty; }  

  // ** convenience accessor **
  unsigned int operator-=(unsigned int qty) { fillLots(qty); return m_workingQty; }
  
  /// <summary>
  /// Locks the current order instance.
  /// </summary>
  /// <param name="readonly">true for read shared access, false to block write acess.</param>
  /// <returns></returns>
  //bool lock(const bool shared = false) const {
  bool lock(const bool shared = false) {
      m_locked = shared? m_mutex->try_lock_shared(): m_mutex->try_lock();
	  return m_locked;
  }
  
  /// <summary>
  /// Unlocks the current order instance.
  /// </summary>
  /// <param name="readonly">true for read shared access, false to block write acess.</param>
  bool unlock(const bool shared = false) {
      if (!m_locked)
          return false;

      if (shared)
          m_mutex->unlock_shared();
      else
          m_mutex->unlock(); 

	  m_locked = false;
	  return true;
  }


  bool locked() const { return m_locked; }

  /// <summary>
  /// Returns order as string.
  /// </summary>
  /// <returns></returns>
  std::string str() const {
      utils::osyncstream os;
      os << "order{id: " << orderId() << ", security: " << securityId() << ", side: " << side()
          << ", qty: " << qty() << ", working: " << workingQty() << ", filled: " << filledQty()
          << ", user: " << user() << ", company: " << company() << "}";
      return os.str();
  }

  // ** convenience operator (cast to string) **
  operator std::string() const { return str(); }
  
  /// <summary>
  /// Formatted data operator (printing)
  /// </summary>
  /// <param name="os">The os.</param>
  /// <param name="order">The order.</param>
  /// <returns></returns>
  friend utils::osyncstream& operator<<(utils::osyncstream& os, const Order& order) {
      os << order.str();
      return os;
  }
      
 private:
  std::string m_orderId;     // unique order id
  std::string m_securityId;  // security identifier
  std::string m_side;        // side of the order, eg Buy or Sell
  unsigned int m_qty = 0;    // qty for this order
  std::string m_user;        // user name who owns this order
  std::string m_company;     // company for user

  unsigned int m_workingQty = 0;   
  bool m_locked = false;
  std::shared_ptr<std::shared_mutex> m_mutex;  
};




/// <summary>
/// Order Cache interface
/// </summary>
class OrderCacheInterface
{

 public:
   
     /// <summary>
     /// Adds the order.
     /// </summary>
     /// <param name="order">The order.</param>
     virtual void addOrder(Order order) = 0;
   
     /// <summary>
     /// Removes the order with this unique order id
     /// </summary>
     /// <param name="orderId">The order identifier.</param>
     virtual void cancelOrder(const std::string& orderId) = 0;
   
     /// <summary>
     /// Removes all orders for this user
     /// </summary>
     /// <param name="user">The user.</param>
     virtual void cancelOrdersForUser(const std::string& user) = 0;
       
     /// <summary>
     /// Removes all orders in the cache for this security with qty >= minQty
     /// </summary>
     /// <param name="securityId">The security identifier.</param>
     /// <param name="minQty">The minimum qty.</param>
     virtual void cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty) = 0;
      
     /// <summary>
     /// Returns the total qty that can match for the security id
     /// </summary>
     /// <param name="securityId">The security identifier.</param>
     /// <returns></returns>
     virtual unsigned int getMatchingSizeForSecurity(const std::string& securityId) = 0;
   
     /// <summary>
     /// Returns all orders in cache in a vector
     /// </summary>
     /// <returns></returns>
     virtual std::vector<Order> getAllOrders() const = 0;

};



/// <summary>
/// Provides an implementation for the OrderCache class (Order iterator/pointer). 
/// </summary>
typedef typename std::list<Order>::iterator order_ptr;

/// <summary>
/// Provides an implementation for the OrderCache class (vector of Order iterator/pointer). 
/// </summary>
typedef typename std::vector<order_ptr> order_list;


/// <summary>
/// Order fill information
/// 
/// ** this structure is NOT required for the proposed problem itself, just a "extended feature" **
/// 
/// Remark: only used at the OrderCache::GetMatches()/GetMatchesBySecurity() methods
/// Remark: requeires the definition of EXTENDED_INTERFACE macro in order to be evaluated
/// </summary>
class OrderFill {

 public:
    OrderFill(
        const std::string& buyOrderId,
        const std::string& sellOrderId,
        const unsigned int qty): 
        m_buyOrderId(buyOrderId),
        m_sellOrderId(sellOrderId),
        m_qty(qty) {  }

    /// <summary>
    /// The buy order identifier
    /// </summary>
    const std::string buyOrderId() const { return m_buyOrderId; }
        
    /// <summary>
    /// The sell order identifier
    /// </summary>
    const std::string sellOrderId() const { return m_sellOrderId; }

    /// <summary>
    /// The filled quantity of lots
    /// </summary>
    const unsigned int qty() const { return m_qty; }

    /// <summary>
    /// Returns order fill as string.
    /// </summary>
    /// <returns></returns>
    std::string str() const {
        utils::osyncstream os;
        os << "order fill {buy: " << buyOrderId() << ", sell: " << sellOrderId() << ", qty: " << qty() << "}";
        return os.str();
    }

    // ** convenience operator (cast to string) **
    operator std::string() const { return str(); }

    /// <summary>
    /// Formatted data operator (printing)
    /// </summary>
    /// <param name="os">The os.</param>
    /// <param name="order">The order.</param>
    /// <returns></returns>
    friend utils::osyncstream& operator<<(utils::osyncstream& os, const OrderFill& fill) {
        os << fill.str();
        return os;
    }
    
 private:
    std::string m_buyOrderId;
    std::string m_sellOrderId;    
    unsigned int m_qty = 0;
};




/// <summary>
/// Order Cache
/// </summary>
/// <seealso cref="OrderCacheInterface" />
class OrderCache : public OrderCacheInterface
{

  public:    
    /// <summary>
    /// Adds the order into current order cache.
    /// Remark: O(1)
    /// </summary>
    /// <param name="order">The order.</param>
    void addOrder(Order order) override;
    
    /// <summary>
    /// Cancels the order by specified Id (thread-safe).
    /// Remark: O(1)
    /// </summary>
    /// <param name="orderId">The order identifier.</param>
    void cancelOrder(const std::string& orderId) override;
    
    /// <summary>
    /// Cancels the orders for the specified user  (thread-safe).
    /// </summary>
    /// <param name="user">The user.</param>
    void cancelOrdersForUser(const std::string& user) override;
    
    /// <summary>
    /// Cancels the orders for sec identifier with minimum quantity of lots (thread-safe).
    /// </summary>
    /// <param name="securityId">The security identifier.</param>
    /// <param name="minQty">The minimum size to cancel the order.</param>
    void cancelOrdersForSecIdWithMinimumQty(const std::string& securityId, unsigned int minQty) override;
    
    /// <summary>
    /// Gets the matching size for security.
    /// Remark: O(1)
    /// </summary>
    /// <param name="securityId">The security identifier.</param>
    /// <returns></returns>
    unsigned int getMatchingSizeForSecurity(const std::string& securityId) override;
    
    /// <summary>
    /// Gets all orders in current cache instance as a vector
    /// </summary>
    /// <returns>
    /// vector of orders
    /// </returns>
    std::vector<Order> getAllOrders() const override;

    //----------------------------------------------------------------
        
    /// <summary>
    /// Checks the order existence by the specified order identifier.
    /// 
    /// Remark: ** this method is NOT required for the proposed problem itself, just a "aditional feature"... **
    /// </summary>
    /// <param name="orderId">The order identifier.</param>
    /// <returns>
    /// True case order is found, False otherwise
    /// </returns>
    const bool exists(const std::string& orderId) const;
    
    /// <summary>
    /// Gets the number of orders in current cache instance.
    /// 
    /// Remark: ** this method is NOT required for the proposed problem itself, just a "aditional feature"... **
    /// </summary>
    /// <returns></returns>
    const size_t size() const;

   /// <summary>
   /// Gets order by specified order id.
   /// 
   /// Remark: ** this method is NOT required for the proposed problem itself, just a "aditional feature"... **
   /// </summary>
   /// <param name="user">The order id.</param>
   /// <returns>the order</returns>
    Order& getOrder(const std::string& orderId) const;

    /// <summary>
    /// Gets the order by the specified order identifier.
    /// 
    /// ** Convenience accessor **
    /// </summary>
    /// <param name="orderId">The order identifier.</param>
    /// <returns></returns>
    const Order& operator[] (const std::string& orderId);
    
    /// <summary>
    /// Gets all orders matches
    /// 
    /// Remark: ** this method is NOT required for the proposed problem itself, just a "aditional feature"... **
    ///
    /// Remark: requeires the definition of EXTENDED_INTERFACE macro in order to be evaluated, 
    ///         otherwise it will return an empty vector (in oder not penalize standard code 
    ///         performance avaliation).
    /// </summary>
    /// <param name="securityId">The security identifier.</param>
    /// <returns></returns>
    std::vector<OrderFill> getAllOrderMatches() const;

    /// <summary>
    /// Gets all orders matches by specified security identifier
    /// 
    /// Remark: ** this method is NOT required for the proposed problem itself, just a "aditional feature"... **
    /// 
    /// Remark: requeires the definition of EXTENDED_INTERFACE macro in order to be evaluated, 
    ///         otherwise it will return an empty vector (in oder not penalize standard code 
    ///         performance avaliation)
    /// </summary>
    /// <param name="securityId">The security identifier.</param>
    /// <returns></returns>
    std::vector<OrderFill> getOrderMatchesBySecurity(const std::string& securityId) const;
        
    /// <summary>
    /// Returns true case current order cache is in the single thread mode (for debug/performance purposes), false otherwise.
    /// </summary>
    /// <returns></returns>
    const bool multiThread() const { return _multiThread; }
    
    /// <summary>
    /// Sets current order cache the single thread mode (for debug/performance purposes).
    /// </summary>
    /// <param name="value">The value.</param>
    void setMultiThread(const bool& value) { _multiThread = value; }

    /// <summary>
    /// Returns true case current order cache is in the verbose mode (for debug purposes), false otherwise.
    /// </summary>
    /// <returns></returns>
    const bool verbose() const { return _verbose; }

    /// <summary>
    /// Sets current order cache the verbose mode (for debug purposes).
    /// </summary>
    /// <param name="value">The value.</param>
    void setVerbose(const bool& value) { _verbose = value; }


private:        
    typedef typename std::list<OrderFill>::iterator order_match_ptr;
    typedef typename std::unordered_set<std::string> orders_keys;
    typedef typename std::unordered_map<std::string, orders_keys> order_index_map;
    typedef typename std::unordered_map<std::string, order_list> order_match_index;

	typedef typename std::shared_lock<std::shared_timed_mutex> read_lock;
	typedef typename std::unique_lock<std::shared_timed_mutex> write_lock;
    
	//----------------------------------------------------------------

	// internal states (used for testing purposes only)
    // (compares single thread vc multithread processing times, debug verbosity, etc)
    bool _multiThread = true;
    bool _verbose = true;

    /// <summary>
	/// The orders access mutex (thread-saveting)
    /// </summary>
    mutable std::shared_timed_mutex _ordersMutex;
    mutable std::shared_mutex _longOrdersMutex;
    mutable std::shared_mutex _shortOrdersMutex;
    
    /// <summary>
    /// Returns a scoped lock that can be shared by multiple
    /// readers at the same time while excluding any writers
    /// </summary>
    /// <returns></returns>
    read_lock lockForReadOrders() const {
		return read_lock(_ordersMutex);
    }
    
    /// <summary>
    /// This returns a scoped lock that is exclusing to one
    /// writer preventing any readers
    /// </summary>
    /// <returns></returns>
    write_lock lockForUpdateOrders() {
        return write_lock(_ordersMutex);
    }
        
    //----------------------------------------------------------------
    
    /// <summary>
    /// The orders list 
    /// 
    /// Remark: storing orders as a list allows to remove itens at 0(1)
    /// </summary>
    std::list<Order> _orders;
                
    /// <summary>
	/// The orders index - O(1) access to orders by index
    /// 
    /// Remark: implements a relation 1:1 from "orderId" => order pointer (on list)
    /// </summary>
    std::unordered_map<std::string, order_ptr> _orderIndex;
        
    /// <summary>
    /// The user orders index - O(1) access to orders by user
    /// 
    /// Remark: implements a relation 1:n from "user" => "orderId"
    /// </summary>
    order_index_map _userOrdersIndex;
    
    /// <summary>
    /// The security orders index - O(1) access to orders by security
    /// 
    /// Remark: implements a relation 1:n from "securityId" => "orderId"
    /// </summary>
    order_index_map _securityOrdersIndex;


    //----------------------------------------------------------------
        
    // thread-safe reading matched quantity
    mutable std::shared_timed_mutex _matchedQuantityMutex;

    /// <summary>
    /// The security long orders index (buy side) - optimization
    /// </summary>
    order_match_index _securityLongOrdersIndex;

    /// <summary>
    /// The security short orders index (sell side) - optimization
    /// </summary>
    order_match_index _securityShortOrdersIndex;
            
    /// <summary>
	/// The matched quantity cache by securityId (main cache)
    /// 
    /// Remark: use "getMatchedQuantityInCache()" for thread-safe 
    /// </summary>
    std::unordered_map<std::string, unsigned int> _matchedQuantity;
    
    /// <summary>
    /// The orders matches list 
    /// Remark: required for extended interface: "getOrderMatches()", "getOrderMatchesBySecurity()", etc
    /// Remark: requeires the definition of EXTENDED_INTERFACE macro in order to be evaluated, 
    ///         otherwise it will return an empty vector (in oder not penalize standard code 
    ///         performance avaliation)
    /// </summary>
    std::list<OrderFill> _orderMatches;


    /// <summary>
    /// Gets the current cached matched quantity by security (thread-safe).
    /// </summary>
    /// <param name="securityId">The security identifier.</param>
    /// <returns></returns>
    unsigned int getMatchedQuantityInCache(const std::string& securityId) const;

   
    //----------------------------------------------------------------
    
    /// <summary>
    /// Cancels the orders (uses multithreading if required, i.e. number of orders > DELETE_CHUNK_SIZE) [private]
    /// </summary>
    /// <param name="orders">The order identifiers.</param>
    /// <param name="minQty">Only cancel the specified order if the order quantity if greather than minQty value.</param>
    void cancelOrders(orders_keys& orders, unsigned int minQty = 0);


    // <summary>
	/// Cancels the orders on the specified range (helper snippet) [private]
    /// </summary>
    /// <param name="orders">The order identifiers.</param>
    /// <param name="start">start iterator.</param>
    /// <param name="start">end iterator.</param>
    /// <param name="minQty">Only cancel the specified order if the order quantity if greather than minQty value.</param>
    void OrderCache::cancelOrdersRange(orders_keys& orders, orders_keys::iterator& start, orders_keys::iterator& end, unsigned int minQty = 0);


    /// <summary>
    /// Cancels the order (without locks - thread unsafe) [private]
    /// </summary>
    /// <param name="orderId">The order identifier.</param>
    /// <param name="minQty">Only cancel the specified order if the order quantity if greather than minQty value.</param>
    void cancelSingleOrder(const std::string& orderId, unsigned int minQty = 0, bool lockOrder = false);


    //----------------------------------------------------------------

    /// <summary>
    /// Finds matches for the specified order and store it in cache (without locks - thread unsafe) [private]
    /// Remark: solution for getMatchingSizeForSecurity() with O(1)
    /// </summary>
    /// <param name="orderId">The order identifier.</param>
    unsigned int matchOrderInCache(order_ptr& ptr, bool lockOrder = true);


    std::mutex _ThreadPoolMutex;
    std::vector<std::thread> _ThreadPool;    
    /// <summary>
    /// Finds matches for the specified order and store it in cache, handling thread pool
    /// </summary>
    /// <param name="ptr">The PTR.</param>
    void matchOrderInPool(order_ptr& ptr);

    //----------------------------------------------------------------

    /// <summary>
    /// Determines whether specified order is a buy order (long).
    /// </summary>
    /// <param name="ptr">The order pointer.</param>
    /// <returns>
    ///   <c>true</c> if order is buy side; <c>false</c> if order is sell side.
    /// </returns>
    static const bool isBuySide(order_ptr& ptr) {
		return ptr->side() != "Sell";
    }
    
    /// <summary>
    /// Comparator predicate intended to sorting orders by workings lots (descending).
    /// </summary>
    /// <param name="left">The left order.</param>
    /// <param name="right">The right order.</param>
    /// <returns></returns>
    static const bool workingLotsComparator(const order_ptr& left, const order_ptr& right) {        
        return left->workingQty() > right->workingQty();
    }
};


namespace debug {


    /*----------------------------------------------------------------
        TESTING & DEBUGGING
     ----------------------------------------------------------------*/


    /*
    //
    // this fuctor was originally written for sorting orders by working lots,
    //       e.g.: std::sort(_orders.begin(), _orders.end(), cmp);
    // on sorted greedy algorithm (see paper), but unsorted approach is indeed more efficient
    //
    auto cmp = [](const order_ptr& a, const order_ptr& b) { return a->workingQty() < b->workingQty(); }
    */


    /// <summary>
    /// timer_start alias ("TestUtils::tic()"/"TestUtils::toc()")
    /// </summary>
    typedef typename std::chrono::steady_clock::time_point timer_start;


    /// <summary>
    /// Utilities functions
    /// </summary>
    class TestUtils {
    public:

        /// <summary>
        /// Start time (performance debug)
        /// </summary>
        /// <returns></returns>
        static timer_start tic() {
            return (timer_start)std::chrono::steady_clock::now();
        }

        /// <summary>
        /// Evaluates elapsed time (microseconds) from last start time (performance debug)
        /// </summary>
        /// <param name="start">The start time.</param>
        /// <param name="msg">The optional console message.</param>
        /// <returns></returns>
        static long long toc(const timer_start& start, const std::string& msg = "") {
            if (msg == "")
                return getElapsedTime(start);

            return toc(utils::osyncstream(), start, msg);
        }

        /// <summary>
        /// Evaluates elapsed time (microseconds) from last start time (performance debug)
        /// </summary>
        /// <param name="start">The start time.</param>
        /// <param name="msg">The optional console message.</param>
        /// <returns></returns>
        static long long toc(utils::osyncstream& out, const timer_start& start, const std::string& msg = "") {

            long long elapsedTime = getElapsedTime(start);
            if (msg == "")
                return elapsedTime;

            #if defined(_DEBUG) || defined(SHOW_EXECUTION_TIMES)
            out << '\n' << msg << " [" << elapsedTime << "us]" << '\n';
            #endif //  _DEBUG

            return elapsedTime;
        }

        #ifdef _DEBUG

        /// <summary>
        /// Prints the specified order on console.
        /// </summary>    
        /// <param name="order">The order.</param>
        /// <param name="tabs">The number of empty spaces on the begining.</param>
        static void print(order_ptr& order, unsigned int tabs = 0) {
            print(utils::osyncstream(), order, tabs);
        }

        /// <summary>
        /// Prints the specified order on console.
        /// </summary>    
        /// <param name="out">The syncronized output console (thread safe).</param>
        /// <param name="order">The order.</param>
        /// <param name="tabs">The number of empty spaces on the begining.</param>
        static void print(utils::osyncstream& out, order_ptr& order, unsigned int tabs = 0) {
            printTabs(out, tabs);
            out << order->str();
        }

        /// <summary>
        /// Prints the specified order list.
        /// </summary>
        /// <param name="orders">The orders list.</param>
        /// <param name="tabs">The number of empty spaces on the begining.</param>
        static void print(order_list& orders, unsigned int tabs = 0) {
            for (order_ptr& order : orders)
                print(utils::osyncstream(), order, tabs);
        }

        /// <summary>
        /// Prints the specified order list.
        /// </summary>
        /// <param name="out">The syncronized output console (thread safe).</param>
        /// <param name="orders">The orders list.</param>
        /// <param name="tabs">The number of empty spaces on the begining.</param>
        static void print(utils::osyncstream& out, order_list& orders, unsigned int tabs = 0) {
            for (order_ptr& order : orders)
                print(out, order, tabs);
        }

        /// <summary>
        /// Prints the specified mensage on console.
        /// </summary>
        /// <param name="msg">The text mensage.</param>
        /// <param name="tabs">The number of empty spaces on the begining.</param>
        static void print(const std::string& msg, unsigned int tabs = 0) {
            utils::osyncstream out;
            print(out, msg, tabs);
        }

        /// <summary>
        /// Prints the specified mensage on console.
        /// remark: "*" for a line of "*"
        ///         "-" for a line of "-"  
        /// </summary>
        /// <param name="out">The syncronized output console (thread safe).</param>
        /// <param name="msg">The text mensage.</param>
        /// <param name="tabs">The number of empty spaces on the begining.</param>
        static void print(utils::osyncstream& out, const std::string& msg, unsigned int tabs = 0) {
            if (msg == "br")
                out << '\n';
            else if (msg == "*")
                out << "*******************************************************************************\n";
            else if (msg == "-")
                out << "-------------------------------------------------------------------------------\n";
            else {
                if (tabs > 0)
                    printTabs(out, tabs);
                out << msg << '\n';
            }
        }

    #endif // _DEBUG

    private:
        /// <summary>
        /// Gets the elapsed time (microseconds) from last start time
        /// </summary>
        /// <param name="start">The start.</param>
        /// <returns></returns>
        static long long getElapsedTime(const timer_start& start) {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        }

        #ifdef _DEBUG 
        /// <summary>
        /// Prints the number of spaces.
        /// </summary>
        /// <param name="out">The out.</param>
        /// <param name="tabs">The tabs.</param>
        static void printTabs(utils::osyncstream& out, unsigned int tabs) {

            for (unsigned int i = 0; i < tabs; i++)
                out << " ";
        }
        #endif // _DEBUG
    };
}