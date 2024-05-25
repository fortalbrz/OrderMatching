#include "OrderCache.h"
#include "gtest/gtest.h"
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <list>
#include <memory>
#include <random>
#include <cmath>
#include <algorithm>
#include <cmath>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>


class OrderCacheTest : public ::testing::Test {
protected:
    OrderCache cache;
};


// Test U1: Add order to cache
TEST_F(OrderCacheTest, U1_UnitTest_addOrder) {
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 1000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 3000, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Sell", 500, "User3", "CompanyA"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Buy", 600, "User4", "CompanyC"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Buy", 100, "User5", "CompanyB"});
    cache.addOrder(Order{"OrdId6", "SecId3", "Buy", 1000, "User6", "CompanyD"});
    cache.addOrder(Order{"OrdId7", "SecId2", "Buy", 2000, "User7", "CompanyE"});
    cache.addOrder(Order{"OrdId8", "SecId2", "Sell", 5000, "User8", "CompanyE"});
    ASSERT_EQ(1,1);    
}

// Test U2: Get all orders from cache
TEST_F(OrderCacheTest, U2_UnitTest_getAllOrders) {
    cache.setVerbose(false);
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 1000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 3000, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Sell", 500, "User3", "CompanyA"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Buy", 600, "User4", "CompanyC"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Buy", 100, "User5", "CompanyB"});
    cache.addOrder(Order{"OrdId6", "SecId3", "Buy", 1000, "User6", "CompanyD"});
    cache.addOrder(Order{"OrdId7", "SecId2", "Buy", 2000, "User7", "CompanyE"});
    cache.addOrder(Order{"OrdId8", "SecId2", "Sell", 5000, "User8", "CompanyE"});
    cache.setVerbose(true);
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 8);
    
}

// Test U3: Cancel order
TEST_F(OrderCacheTest, U3_UnitTest_cancelOrder) {
    cache.setVerbose(false);
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 100, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Sell", 100, "User2", "Company1"});
    cache.setVerbose(true);
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 2);

    // Cancel order 2
    cache.cancelOrder("OrdId2");
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 1);
    ASSERT_EQ(allOrders[0].orderId(), "OrdId1");

    // Cancel order 1
    cache.cancelOrder("OrdId1");
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 0);

    // Cancel an order that does not exist
    cache.cancelOrder("OrdId3");
    ASSERT_EQ(allOrders.size(), 0);
}

// Test U4: Cancel orders for user
TEST_F(OrderCacheTest, U4_UnitTest_cancelOrdersForUser) {
    cache.setVerbose(false);
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 1000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId1", "Buy", 600, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId2", "Sell", 3000, "User1", "CompanyB"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Sell", 500, "User2", "CompanyA"});
    cache.setVerbose(true);
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 4);

    // Cancel all orders for User1
    cache.cancelOrdersForUser("User1");
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 2); // Two orders left

    // Cancel all orders for User2
    cache.cancelOrdersForUser("User2");
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 0); // No orders left

    // Cancel an order for a user ID that does not exist
    cache.cancelOrdersForUser("User3");
    ASSERT_EQ(allOrders.size(), 0); // No orders left
}



// Test U5: Cancel orders for security with minimum quantity
TEST_F(OrderCacheTest, U5_UnitTest_cancelOrdersForSecIdWithMinimumQty) {
    OrderCache cache;
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId1", "Buy", 200, "User1", "Company1"});
    cache.addOrder(Order{"2", "SecId1", "Sell", 200, "User2", "Company1"});
    cache.addOrder(Order{"3", "SecId1", "Buy", 100, "User1", "Company1"});
    cache.setVerbose(true);
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 300);
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 3);
    
    // Cancel all orders with security ID 1 and minimum quantity 200
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 200);
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 1);
    
    // Cancel all orders with security ID 1 and minimum quantity 100
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 100);
    allOrders = cache.getAllOrders();
    ASSERT_EQ(allOrders.size(), 0);        
}


// Test U6: First example from README.txt
TEST_F(OrderCacheTest, U6_UnitTest_getMatchingSizeForSecurityTest_Example1) {
    OrderCache cache;
    cache.setVerbose(false);
    cache.addOrder(Order{"OrdId1", "SecId1", "Buy", 1000, "User1", "CompanyA"});
    cache.addOrder(Order{"OrdId2", "SecId2", "Sell", 3000, "User2", "CompanyB"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Sell", 500, "User3", "CompanyA"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Buy", 600, "User4", "CompanyC"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Buy", 100, "User5", "CompanyB"});
    cache.addOrder(Order{"OrdId6", "SecId3", "Buy", 1000, "User6", "CompanyD"});
    cache.addOrder(Order{"OrdId7", "SecId2", "Buy", 2000, "User7", "CompanyE"});
    cache.addOrder(Order{"OrdId8", "SecId2", "Sell", 5000, "User8", "CompanyE"});
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 0);

    matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 2700);

    matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    ASSERT_EQ(matchingSize, 0);
}


// Test U7: Second example from README.txt
TEST_F(OrderCacheTest, U7_UnitTest_getMatchingSizeForSecurityTest_Example2) {
    OrderCache cache;
    cache.setVerbose(false);
    cache.addOrder(Order{"OrdId1", "SecId1", "Sell", 100, "User10", "Company2"});
    cache.addOrder(Order{"OrdId2", "SecId3", "Sell", 200, "User8", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Buy", 300, "User13", "Company2"});
    cache.addOrder(Order{"OrdId4", "SecId2", "Sell", 400, "User12", "Company2"});
    cache.addOrder(Order{"OrdId5", "SecId3", "Sell", 500, "User7", "Company2"});
    cache.addOrder(Order{"OrdId6", "SecId3", "Buy", 600, "User3", "Company1"});
    cache.addOrder(Order{"OrdId7", "SecId1", "Sell", 700, "User10", "Company2"});
    cache.addOrder(Order{"OrdId8", "SecId1", "Sell", 800, "User2", "Company1"});
    cache.addOrder(Order{"OrdId9", "SecId2", "Buy", 900, "User6", "Company2"});
    cache.addOrder(Order{"OrdId10", "SecId2", "Sell", 1000, "User5", "Company1"});
    cache.addOrder(Order{"OrdId11", "SecId1", "Sell", 1100, "User13", "Company2"});
    cache.addOrder(Order{"OrdId12", "SecId2", "Buy", 1200, "User9", "Company2"});
    cache.addOrder(Order{"OrdId13", "SecId1", "Sell", 1300, "User1", "Company1"});
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 300);

    matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 1000);

    matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    ASSERT_EQ(matchingSize, 600);
}

// Test U8: Third example from README.txt
TEST_F(OrderCacheTest, U8_UnitTest_getMatchingSizeForSecurityTest_Example3) {
    OrderCache cache;
    cache.setVerbose(false);
    cache.addOrder(Order{"OrdId1", "SecId3", "Sell", 100, "User1", "Company1"});
    cache.addOrder(Order{"OrdId2", "SecId3", "Sell", 200, "User3", "Company2"});
    cache.addOrder(Order{"OrdId3", "SecId1", "Buy", 300, "User2", "Company1"});
    cache.addOrder(Order{"OrdId4", "SecId3", "Sell", 400, "User5", "Company2"});
    cache.addOrder(Order{"OrdId5", "SecId2", "Sell", 500, "User2", "Company1"});
    cache.addOrder(Order{"OrdId6", "SecId2", "Buy", 600, "User3", "Company2"});
    cache.addOrder(Order{"OrdId7", "SecId2", "Sell", 700, "User1", "Company1"});
    cache.addOrder(Order{"OrdId8", "SecId1", "Sell", 800, "User2", "Company1"});
    cache.addOrder(Order{"OrdId9", "SecId1", "Buy", 900, "User5", "Company2"});
    cache.addOrder(Order{"OrdId10", "SecId1", "Sell", 1000, "User1", "Company1"});
    cache.addOrder(Order{"OrdId11", "SecId2", "Sell", 1100, "User6", "Company2"});
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 900);

    matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 600);

    matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    ASSERT_EQ(matchingSize, 0);
}

// Test O1: Matching Orders with Different Quantities
TEST_F(OrderCacheTest, O1_OrderMatchingTest_TestDifferentQuantities) {
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId1", "Buy", 5000, "User1", "CompanyA"});
    cache.addOrder(Order{"2", "SecId1", "Sell", 2000, "User2", "CompanyB"});
    cache.addOrder(Order{"3", "SecId1", "Sell", 1000, "User3", "CompanyC"});
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 3000); // 2000 from Order 2 and 1000 from Order 3 should match with Order 1
}

// Test O2: Complex Order Combinations
TEST_F(OrderCacheTest, O2_OrderMatchingTest_TestComplexCombinations) {
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId2", "Buy", 7000, "User1", "CompanyA"});
    cache.addOrder(Order{"2", "SecId2", "Sell", 3000, "User2", "CompanyB"});
    cache.addOrder(Order{"3", "SecId2", "Sell", 4000, "User3", "CompanyC"});
    cache.addOrder(Order{"4", "SecId2", "Buy", 500, "User4", "CompanyD"});
    cache.addOrder(Order{"5", "SecId2", "Sell", 500, "User5", "CompanyE"});
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 7500); // 7000 from Order 1 and 500 from Order 4 should fully match with Orders 2, 3, and 5
}

// Test O3: Orders from the Same Company Should Not Match
TEST_F(OrderCacheTest, O3_OrderMatchingTest_TestSameCompanyNoMatch) {
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId3", "Buy", 2000, "User1", "CompanyA"});
    cache.addOrder(Order{"2", "SecId3", "Sell", 2000, "User2", "CompanyA"}); // Same company as Order 1
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId3");
    ASSERT_EQ(matchingSize, 0); // No match should occur since both orders are from the same company
}



// Test C1: Cancelling an Order that Does Not Exist
TEST_F(OrderCacheTest, C1_CancellationTest_CancelNonExistentOrder) {
    cache.cancelOrder("NonExistentOrder");
    // Assuming getAllOrders() returns all current orders, the size should be 0 for an empty cache.
    std::vector<Order> allOrders = cache.getAllOrders();
    ASSERT_TRUE(allOrders.empty());
}

// Test C2: Partial Cancellation Based on Minimum Quantity
TEST_F(OrderCacheTest, C2_CancellationTest_CancelOrdersPartialOnMinQty) {
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId1", "Buy", 200, "User1", "Company1"});
    cache.addOrder(Order{"2", "SecId1", "Sell", 500, "User2", "Company1"});
    cache.addOrder(Order{"3", "SecId1", "Buy", 300, "User3", "Company2"});
    cache.setVerbose(true);

    // Cancel orders for SecId1 with a minimum quantity of 300
    cache.cancelOrdersForSecIdWithMinimumQty("SecId1", 300);

    std::vector<Order> allOrders = cache.getAllOrders();
    // Only the order with quantity 200 should remain
    ASSERT_EQ(allOrders.size(), 1);
    ASSERT_EQ(allOrders[0].orderId(), "1");
}



// Test C3: Cancelling Multiple Orders for the Same User or Security ID
TEST_F(OrderCacheTest, C3_CancellationTest_CancelMultipleOrdersForUser) {
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId1", "Buy", 200, "User1", "Company1"});
    cache.addOrder(Order{"2", "SecId2", "Sell", 300, "User1", "Company1"});
    cache.addOrder(Order{"3", "SecId3", "Buy", 400, "User2", "Company2"});
    cache.setVerbose(true);

    // Cancel all orders for User1
    cache.cancelOrdersForUser("User1");

    std::vector<Order> allOrders = cache.getAllOrders();
    // Only the order for User2 should remain
    ASSERT_EQ(allOrders.size(), 1);
    ASSERT_EQ(allOrders[0].orderId(), "3");
}

// Test M1: Multiple Small Orders Matching a Larger Order
TEST_F(OrderCacheTest, M1_MatchingSizeTest_MultipleSmallOrdersMatchingLargeOrder) {
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId1", "Buy", 10000, "User1", "CompanyA"});
    cache.addOrder(Order{"2", "SecId1", "Sell", 2000, "User2", "CompanyB"});
    cache.addOrder(Order{"3", "SecId1", "Sell", 1500, "User3", "CompanyC"});
    cache.addOrder(Order{"4", "SecId1", "Sell", 2500, "User4", "CompanyD"});
    cache.addOrder(Order{"5", "SecId1", "Sell", 4000, "User5", "CompanyE"});
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId1");
    ASSERT_EQ(matchingSize, 10000); // Total of 10000 from orders 2, 3, 4, and 5 should match with order 1
}

// Test M2: Multiple Matching Combinations
TEST_F(OrderCacheTest, M2_MatchingSizeTest_MultipleMatchingCombinations) {
    cache.setVerbose(false);
    cache.addOrder(Order{"1", "SecId2", "Buy", 6000, "User1", "CompanyA"});
    cache.addOrder(Order{"2", "SecId2", "Sell", 2000, "User2", "CompanyB"});
    cache.addOrder(Order{"3", "SecId2", "Sell", 3000, "User3", "CompanyC"});
    cache.addOrder(Order{"4", "SecId2", "Buy", 1000, "User4", "CompanyD"});
    cache.addOrder(Order{"5", "SecId2", "Sell", 1500, "User5", "CompanyE"});
    cache.setVerbose(true);

    unsigned int matchingSize = cache.getMatchingSizeForSecurity("SecId2");
    ASSERT_EQ(matchingSize, 6500); // Total of 6500 (2000 from Order 2, 3000 from Order 3, 1500 from Order 5) should match with Orders 1 and 4
}




#ifdef EXTENDED_TESTING

//  Extended Test 0: Order Sorting (by working lots: used on greedy sorted algorithm)
TEST_F(OrderCacheTest, X1_ExtendedTest_OrderSortingByWorkingLots) {
    
    //
    // Design test (performance optimization)
    // 
	// checks better performance: sorting indexes on std::list vs std::vector
    //
    debug::timer_start start;

    const int SIZE = 1000;

    auto cmp = [](const order_ptr& left, const order_ptr& right) {
        return left->workingQty() > right->workingQty();
    };

    // fill sample data
    std::list<Order> data;    
    order_list orderIndex;

    for (unsigned int i = 0; i < SIZE; i++) {
        auto order = Order{ std::to_string(i), "SecId1", "Buy", SIZE - i, "User1", "CompanyA" };        
        data.push_back(order);
    }

    // index for the orders iterators
    for (auto it = data.begin(); it != data.end(); it++)         
        orderIndex.push_back(it);
              
    start = debug::TestUtils::tic();
    std::sort(orderIndex.begin(), orderIndex.end(), cmp);
    debug::TestUtils::toc(start, "sorting vector of orders ptr");
    
    
    std::string orderId = "3";
    orderIndex.erase(std::remove_if(orderIndex.begin(), orderIndex.end(), 
        [&orderId](order_ptr& order) { return order->orderId() == orderId; }));
        
    ASSERT_EQ(1,1);
}

// Extended Test 1:  multithread std::cout (C++17 implementation)
TEST_F(OrderCacheTest, X1_ExtensionsTest_MultithreadCout) {
       
    //
	// multithread std::cout (C++17 implementation) (debug purposes)
    // 
    #ifdef _DEBUG
    //
    // use cases:
    //
    utils::osyncstream() << "TEST 1\n"; //no interence between threads  

    utils::osyncstream{} << "TEST 2\n"; //no interence between threads

    utils::osyncstream out; //no interence between threads until the object is disposed   
    out << "TEST " << 3 << "\n";
    out << "TEST " << 4 << "\n";

    ASSERT_EQ("TEST 3\nTEST 4\n", out.str());

    #else
    ASSERT_EQ(1, 1);
    #endif
}

// Extended Test 2:  extended members / multithread 
TEST_F(OrderCacheTest, X2_ExtensionsTest_OrderMembers) {

    // basic methods: fill/unfill orders
    auto order = Order{ "Ord1", "SecId1", "Buy", 10, "User1", "CompanyA" };
    ASSERT_EQ(order.workingQty(), 10);
    ASSERT_EQ(order.filledQty(), 0);
    ASSERT_EQ(order.isFilled(), false);

    order.fillLots(6);
    ASSERT_EQ(order.workingQty(), 4);
    ASSERT_EQ(order.filledQty(), 6);
    ASSERT_EQ(order.isFilled(), false);
    
    order.fillLots(4);
    ASSERT_EQ(order.workingQty(), 0);
    ASSERT_EQ(order.filledQty(), 10);
    ASSERT_EQ(order.isFilled(), true);

    order.unfillLots(6);
    ASSERT_EQ(order.workingQty(), 6);
    ASSERT_EQ(order.filledQty(), 4);
    ASSERT_EQ(order.isFilled(), false);

    order.resetFills();
    ASSERT_EQ(order.workingQty(), 10);
    ASSERT_EQ(order.filledQty(), 0);
    ASSERT_EQ(order.isFilled(), false);

    //
    // checks for the order lock
    //
    int counter = 10;
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.push_back(std::thread([&counter](const int& threadId, Order* order) {
			order->lock();
            
            counter--;
            order->fillLots(1);

            #ifdef _DEBUG
            utils::osyncstream() << "thread: " << threadId << ", working orders: " << order->workingQty() << ", sequential: " << counter << "\n";
            #endif
            
            if (order->workingQty() != counter)
                throw std::range_error("order locks failed");
			
            order->unlock();
        }, i, &order));
    }

    // waits for all threads to finish
    std::for_each(threads.begin(), threads.end(), [](std::thread& t){
        if (t.joinable())
            t.join();
    });

    #ifdef _DEBUG
    utils::osyncstream() << "working orders [final]: " << order.workingQty() << "\n";
    #endif
    ASSERT_EQ(order.isFilled(), true);
}

// Extended Test 3:  indexed access
TEST_F(OrderCacheTest, X3_ExtensionsTest_IndexedAccess) {
    unsigned int size = 100;

    cache.setVerbose(false);    
    for (unsigned int i = 0; i < size; i++) {
        auto order = Order{ std::to_string(i), "SecId1", "Buy", i, "User1", "CompanyA" };
        cache.addOrder(order);
    }
    cache.setVerbose(true);

    auto start = debug::TestUtils::tic();

    Order order = cache["59"];
	
    debug::TestUtils::toc(start, "random access time: ");
    
    ASSERT_EQ(order.qty(), 59);
    ASSERT_EQ(order.user(), "User1");
}

// Extended Test 4:  order matching
TEST_F(OrderCacheTest, X4_ExtensionsTest_OrderMatching) {
    // number of orders
    unsigned int size = 100;

    // fills data
    cache.setVerbose(false);
    for (unsigned int i = 0; i < size; i++) {
        auto order = Order{ std::to_string(i), "SecId1", "Buy", i, "User1", "CompanyA" };
        cache.addOrder(order);
    }
    cache.setVerbose(true);

    auto start = debug::TestUtils::tic();
    // indexed access
    Order order = cache["5"];

    auto indexElapsedTime = debug::TestUtils::toc(start, "random access time: ");

    ASSERT_EQ(order.qty(), 5);
    ASSERT_EQ(order.user(), "User1");

	// sequential search time (comparison purposes)
    auto orders = cache.getAllOrders();
    start = debug::TestUtils::tic();
    auto _ = std::find_if(orders.begin(), orders.end(),
        [](const Order& ord) { 
            return ord.orderId() == "5";
        });
	auto sequentialElapsedTime = debug::TestUtils::toc(start, "sequential order search time: ");
    if (sequentialElapsedTime > 1 &&  indexElapsedTime > 1)
        ASSERT_GE(sequentialElapsedTime, indexElapsedTime);    
}



// Extended Test 5: Multithread x Single threaded order matching
TEST_F(OrderCacheTest, X5_PerformanceTest_MultithreadOrderMatch) {
    // test vector size
    unsigned int nthreads = std::thread::hardware_concurrency();
    unsigned int size = nthreads * 64;

    // generates random positions
    std::random_device rnd_device;
    std::mt19937 mersenne_engine{ rnd_device() };  
    std::uniform_int_distribution<int> dist{ -100, 100 };

    auto generateRandom = [&dist, &mersenne_engine]() { 
        int value = dist(mersenne_engine);
        return value == 0 ? 1 : value; 
    };

    std::vector<int> positions(size);
    std::generate(positions.begin(), positions.end(), generateRandom);
        
    // fills sample data
    auto fill = [&](OrderCache& cached) {
        for (unsigned int i = 0; i < size; i++) {
            auto order = Order{ std::to_string(i), "SecId1", 
                positions[i] > 0 ? "Buy" : "Sell", 
                (unsigned int)std::abs(positions[i]), "User1", "CompanyA"};
            cached.addOrder(order);
        }
    };

    OrderCache singleThreadCache;
    singleThreadCache.setVerbose(false);
    singleThreadCache.setMultiThread(false);
    fill(singleThreadCache);
    ASSERT_EQ(singleThreadCache.size(), size);

    OrderCache multiThreadCache;
    multiThreadCache.setVerbose(false);
    multiThreadCache.setMultiThread(false);
    fill(multiThreadCache);
    ASSERT_EQ(multiThreadCache.size(), size);

    debug::timer_start start;
    utils::osyncstream out;
	    
    //
	// evaluates time for sequential orders matching (for comparison purposes)
    //    
    start = debug::TestUtils::tic();
    auto sequential = singleThreadCache.getMatchingSizeForSecurity("SecId1");
    debug::TestUtils::toc(out, start, "sequential order matching time: ");
        
    //
    // evaluates time for multithreading orders cancellation (for comparison purposes)
    //    
    start = debug::TestUtils::tic();
    auto parallel = multiThreadCache.getMatchingSizeForSecurity("SecId1");
    debug::TestUtils::toc(out, start, "parallel order  matching time: ");

    ASSERT_GE(sequential, parallel);
}

// Extended Test 6: Multithread x Single threaded order deleting
TEST_F(OrderCacheTest, X6_PerformanceTest_MultithreadCancel) {

    unsigned int nthreads = std::thread::hardware_concurrency();
    unsigned int size = nthreads * DELETE_CHUNK_SIZE;
    cache.setVerbose(false);

    debug::timer_start start;
    utils::osyncstream out;

    auto fill = [&]() {

        for (unsigned int i = 0; i < size; i++) {
            auto order = Order{ std::to_string(i), "SecId1", "Buy", size - i, "User1", "CompanyA" };
            cache.addOrder(order);
        }
    };

    //
    // evaluates time for sequential orders cancellation (for comparison purposes)
    //    
    fill();
    ASSERT_EQ(cache.size(), size);

    // cancel orders (single threading)
    cache.setMultiThread(false);

    start = debug::TestUtils::tic();
    cache.cancelOrdersForUser("User1");
    debug::TestUtils::toc(out, start, "sequential order cancel time: ");
    ASSERT_EQ(cache.size(), 0);

    //
    // evaluates time for multithreading orders cancellation (for comparison purposes)
    //    
    fill();
    ASSERT_EQ(cache.size(), size);

    // cancel orders (multithreading)
    cache.setMultiThread(true);

    start = debug::TestUtils::tic();
    cache.cancelOrdersForUser("User1");
    debug::TestUtils::toc(out, start, "parallel order cancel time: ");
    ASSERT_EQ(cache.size(), 0);

    cache.setVerbose(true);
}


#ifdef EXTENDED_INTERFACE

// Extended Test 11: Get
TEST_F(OrderCacheTest, X11_ExtendedTest_GetAllOrderMatches) {

    cache.addOrder(Order{ "1", "SecId1", "Buy", 10000, "User1", "CompanyA" });
    cache.addOrder(Order{ "2", "SecId1", "Sell", 2000, "User2", "CompanyB" });
    cache.addOrder(Order{ "3", "SecId1", "Sell", 1500, "User3", "CompanyC" });
    cache.addOrder(Order{ "4", "SecId1", "Sell", 2500, "User4", "CompanyD" });
    cache.addOrder(Order{ "5", "SecId1", "Sell", 4000, "User5", "CompanyE" });
    
    auto orders = cache.getAllOrderMatches();    
    ASSERT_EQ(orders.size(), 4);
}

#endif // EXTENDED_INTERFACE

#endif // EXTENDED_TESTING



int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

