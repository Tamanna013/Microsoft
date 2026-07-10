#include <gtest/gtest.h>
#include "nimbus/common/bounded_blocking_queue.h"
#include <thread>
#include <future>
#include <vector>
#include <atomic>

using namespace nimbus::common;

TEST(BoundedBlockingQueueTest, SingleThreadCorrectness) {
    BoundedBlockingQueue<int> q(2);
    EXPECT_TRUE(q.Push(1).IsOk());
    EXPECT_TRUE(q.Push(2).IsOk());
    EXPECT_EQ(q.SizeApprox(), 2);

    auto item = q.Pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 1);
    item = q.Pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 2);
}

TEST(BoundedBlockingQueueTest, BlockOnFull) {
    BoundedBlockingQueue<int> q(1);
    q.Push(1); // queue is full

    std::promise<void> p;
    auto f = p.get_future();
    std::thread t([&]() {
        p.set_value();
        q.Push(2);
    });

    f.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(q.SizeApprox(), 1); // thread is still blocked

    q.Pop(); // this should unblock the thread
    t.join();
    EXPECT_EQ(q.SizeApprox(), 1); // the thread pushed 2
}

TEST(BoundedBlockingQueueTest, BlockOnEmpty) {
    BoundedBlockingQueue<int> q(1);

    std::promise<void> p;
    auto f = p.get_future();
    std::atomic<bool> popped(false);
    std::thread t([&]() {
        p.set_value();
        q.Pop();
        popped = true;
    });

    f.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(popped);

    q.Push(1);
    t.join();
    EXPECT_TRUE(popped);
}

TEST(BoundedBlockingQueueTest, ShutdownWithBlockedPush) {
    BoundedBlockingQueue<int> q(1);
    q.Push(1);

    std::thread t([&]() {
        auto res = q.Push(2);
        EXPECT_FALSE(res.IsOk());
        EXPECT_EQ(res.Error(), decltype(q)::QueueError::ShutDown);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.Shutdown();
    t.join();
}

TEST(BoundedBlockingQueueTest, ShutdownWithBlockedPop) {
    BoundedBlockingQueue<int> q(1);

    std::thread t([&]() {
        auto item = q.Pop();
        EXPECT_FALSE(item.has_value());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    q.Shutdown();
    t.join();
}

TEST(BoundedBlockingQueueTest, ShutdownWithBufferedItems) {
    BoundedBlockingQueue<int> q(2);
    q.Push(1);
    q.Push(2);
    q.Shutdown();

    auto item = q.Pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 1);

    item = q.Pop();
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, 2);

    item = q.Pop();
    EXPECT_FALSE(item.has_value());
}

TEST(BoundedBlockingQueueTest, MultiProducerMultiConsumer) {
    BoundedBlockingQueue<int> q(100);
    const int producers_count = 8;
    const int consumers_count = 4;
    const int items_per_producer = 1000;

    std::vector<std::thread> producers;
    for (int i = 0; i < producers_count; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                q.Push(i * items_per_producer + j);
            }
        });
    }

    std::atomic<int> received{0};
    std::vector<std::thread> consumers;
    for (int i = 0; i < consumers_count; ++i) {
        consumers.emplace_back([&]() {
            while (auto item = q.Pop()) {
                received++;
            }
        });
    }

    for (auto& t : producers) t.join();
    q.Shutdown();
    for (auto& t : consumers) t.join();

    EXPECT_EQ(received.load(), producers_count * items_per_producer);
}
