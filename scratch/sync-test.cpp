#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <iostream>

int main() {

    std::list<std::thread> threads;

    std::atomic<unsigned int> finished_round(0);
    std::atomic<unsigned int> round(0);

    std::mutex round_mutex;
    std::condition_variable round_cv;
    std::condition_variable continue_cv;

    for (auto a : {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}) {
        threads.push_back(std::thread([a, &round, &round_mutex, &round_cv, &continue_cv, &finished_round] () {
                std::cout << "This is thread " << a << "\n" << std::flush;

                std::cout << a << "-" << 1 << "\n" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(50*a));

                // Done round 1; signal the main thread
                finished_round++;
                continue_cv.notify_one();

                // Wait for main thread to signal the next round
                std::unique_lock<std::mutex> lk(round_mutex);
                round_cv.wait(lk, [&round,&a]{
                    std::cout << "round_cv wakeup(" << a << "), round=" << round << "\n" << std::flush;
                    return round >= 1; });
                lk.unlock();

                std::cout << a << "-" << 2 << "\n" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(50*(16-a)));

                // Done round 2: signal the main thread

                finished_round++;
                continue_cv.notify_one();

                // Wait for main thread to signal the next round
                lk.lock();
                round_cv.wait(lk, [&round]{ return round >= 2; });
                lk.unlock();

                for (int i = 1; i < 10; i++) {
                    std::cout << a << "-" << 3 << "\n" << std::flush;
                }
        }));
    }

    std::unique_lock<std::mutex> lk(round_mutex);
    continue_cv.wait(lk, [&]{ return finished_round >= threads.size(); });
    // Start round 2
    finished_round = 0;
    round++;
    std::cout << "\nRound 2\n" << std::flush;
    round_cv.notify_all();
    lk.unlock();

    // Round 3:
    lk.lock();
    continue_cv.wait(lk, [&]{ return finished_round >= threads.size(); });
    finished_round = 0;
    round++;
    round_cv.notify_all();
    lk.unlock();

    for (auto &t : threads) {
        t.join();
    }
}
