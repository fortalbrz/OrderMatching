# Implementation for a simple "exchange-like" order matcher.

May 24th, 2024

[Jorge Albuquerque, Ph.D](https://www.linkedin.com/in/jorgealbuquerque/)


This code finds matching buy and sell orders in "exchange-like" call auctions.


## The Order Matching Problem

Matching orders is the process by which a securities exchange pairs one or more unsolicited buy orders to one or more sell orders to make trades. 

If one investor wants to buy a quantity of stock and another wants to sell the same quantity at the same price, their orders match, and a transaction is effected. The work of pairing these orders is the process of order matching whereby exchanges identify buy orders, or bids, with corresponding sell orders, or asks, to execute them.


       --------------                  ----------------
      | Buy Order    |                | Sell Order     |
      | qty: 10 lots | <------------> | qty: 10 lots   |  
       --------------      MATCH       ----------------
       working: 10       (10 lots)      working: 10
       filled: 0             |          filled: 0 
                             |
                             |
       working: 0    <--------------->  working: 0 
       filled: 10                       filled: 10 
     (to party book)                  (to party book)


The orders can be "partially filled" as well:


       --------------                  ----------------
      | Buy Order    |                | Sell Order     |
      | qty: 10 lots | <------------> | qty: 20 lots   |  
       --------------      MATCH       ----------------
       working: 10       (10 lots)      working: 20
       filled: 0             |          filled: 0 
                             |
                             |
       working: 0    <--------------->  working: 10 
       filled: 10                       filled: 10 
     (to party book)                (10 lots to party book)
                                    (10 lots keeps working)




Quick, accurate order matching is a critical component of an exchange. Investors, particularly active investors and day traders, will look for ways to minimize inefficiencies in trading from every possible source. A slow order-matching system may cause buyers or sellers to execute trades at less-than-ideal prices, eating into investors’ profits. If some order-matching protocols tend to favor buyers, and others favor sellers, these methods become exploitable.

This is one of the areas where high-frequency trading (HFT) was able to improve efficiency. Exchanges aim to prioritize trades in a way that benefits buyers and sellers equally so as to maximize order volume—the lifeblood of the exchange ([investopedia](https://www.investopedia.com/terms/m/matchingorders.asp)).


## The Code

**Remark**: this is not a optimal code, just a C++17 exercise (STL only)!

The current solution DOES NOT take into account order prices (i.e., assuming ALL orders as MARKET orders). 

The code ONLY uses the VOLUME (i.e., order quantity of lots) as matching criteria. The code also DOES NOT uses any other financial related criteria (e.g. the number of transactions, filled quantities, moneyness, etc). 

In practice, this is a FIFO algorithm (or price-time-priority algorithm), the earliest active buy order takes priority over any subsequent order.

Specifing different criteria, a different algorithmical approach should be used!!

Assuming only the time execution as the only criteria, this code follow time complexity O(n) of algorithm described as "unsorted greedy" order pair matching (Algorithm 1, page 31) in [Jonsou, V. and Steen, A. (2023)](https://www.diva-portal.org/smash/get/diva2:1765801/FULLTEXT01.pdf).

The algorithm was adapted to multithreading (with locking at order filling level), and O(1) random access for different security or users. 

There are also, *two* main available approaches for order matching including on this code:
 - searches for matches at "getMatchingSizeForSecurity()" (multithread), case USE_CACHED_MATCHING_AT_ADD_ORDER is NOT defined (see bellow)
 - searches for matches at "addOrder()", case macro USE_CACHED_MATCHING_AT_ADD_ORDER is defined (see bellow)

On second approach, the matches values are found at insertion time ("addOrder") are stored in cached.
Threrefore there is no computational effort on calling the critical method "getMatchingSizeForSecurity()" (that works as a simple read-only property, *i.e.*, O(1))


**Remark**: notes on "order" data structure:

The problem interface expects a orders vector at "getAllOrders()". 

There is  design trade-off between:
  - vector access interface on "getAllOrders()" (with O(1) using a internal vector state variable)
  - batch removal of orders on "cancelOrdersForUser()" and "cancelOrdersForSecIdWithMinimumQty()" interfaces

Current implementation uses a internal list of orders data structure in order to remove items with O(1), and therefore the batch orders removal methods (above) can be O(n), otherwise they will be O(n.m). The trade-off is that the current "getAllOrders()" method is O(n) instead of O(1) as expected.


**Remark**: getters and setters

There are two class properties defined only for testing purposes / performance comparision:
 - multiThread() / setMultiThread(): enable/disable multi-thread support
 - verbose() / setVerbose(): enable/disable full verbosity on debug mode (_DEBUG)



## Compilation

### Visual Studio 2022 IDE

Opens the file "CachedOrders.sln"



## Compilation flags

    #define USE_CACHED_MATCHING_AT_ADD_ORDER       - uses the order matching at insertion time (see description above)
    #define EXTENDED_INTERFACE                     - enables extended interfaces for gets orders matched pairs (see description above)
    #define THROW_EXCEPTIONS                       - enables the validation of methods parameters throwing exceptions (DO NOT USE THIS with the unit tests)
    #define EXTENDED_TESTING                       - runs aditional unit tests (more tests) :) 
    #define SHOW_EXECUTION_TIMES                   - shows execution times on console (debug only)
    #define _DEBUG                                 - enables debug messages on console (automatically defined on Visual Studio IDE when selecting "Debug/Release" configuration)
    #define _CRT_SECURE_NO_WARNINGS                - disables some warnings on Visual Studio IDE


## License


Copyright (c) 2024, [Jorge Albuquerque](https://www.linkedin.com/in/jorgealbuquerque/) 

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of Hossein Moein and/or the DataFrame nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Hossein Moein BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
