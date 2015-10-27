/// Lock test

#include <eris/Simulation.hpp>
#include <eris/firm/QFirm.hpp>
#include <eris/market/QMarket.hpp>
#include <eris/interopt/QFStepper.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/intraopt/MUPD.hpp>
#include <eris/intraopt/FixedIncome.hpp>
#include <iostream>
#include <list>
#include <vector>
#include <chrono>


using namespace eris;
using namespace eris::consumer;

std::vector<SharedMember<Quadratic>> c;
std::vector<int> order;
std::mutex orderlock;

// Different sleep timings are multiples of 1 to 20 times this value (in milliseconds)
constexpr int SLEEP_SCALE = 10;

void sleep_ms(int x) {
    std::this_thread::sleep_for(std::chrono::milliseconds(x));
}

void record_finished(int i) {
    orderlock.lock();
    order.push_back(i);
    orderlock.unlock();
}

void thr_code1() {
    std::cout << "                                                  1 write-locking   0...\n" << std::flush;
    auto wlock = c[0]->writeLock();
    std::cout << "                                                  1 write-locked    0\n" << std::flush;
    sleep_ms(20*SLEEP_SCALE);
    std::cout << "                                                  1 write-releasing 0          *3, 4*\n" << std::flush;

    record_finished(1);
}
void thr_code2() {
    sleep_ms(1*SLEEP_SCALE);
    std::cout << "                                                  2 write-locking   0...\n" << std::flush;
    auto wlock = c[0]->writeLock();
    std::cout << "                                                  2 write-locked    0\n" << std::flush;
    std::cout << "                                                  2 write-releasing 0          *4, 5*\n" << std::flush;

    record_finished(2);
}
void thr_code3() {
    sleep_ms(1*SLEEP_SCALE);
    std::cout << "                                                  3 write-locking   3...\n" << std::flush;
    auto wlock = c[3]->writeLock();
    std::cout << "                                                  3 write-locked    3\n" << std::flush;
    sleep_ms(20*SLEEP_SCALE);
    std::cout << "                                                  3 write-releasing 3          *5, 6*\n" << std::flush;

    record_finished(3);
}
void thr_code4() {
    sleep_ms(2*SLEEP_SCALE);
    std::cout << "                                                  4 write-locking   0--4...\n" << std::flush;
    auto wlock = c[0]->writeLock(c[1], c[2], c[3], c[4]);
    std::cout << "                                                  4 write-locked    0--4\n" << std::flush;
    std::cout << "                                                  4 write-releasing 0--4       *7, 7*\n" << std::flush;

    record_finished(4);
}
void thr_code5() {
    sleep_ms(3*SLEEP_SCALE);
    std::cout << "                                                  5 write-locking   2...\n" << std::flush;
    auto wlock = c[2]->writeLock();
    std::cout << "                                                  5 write-locked    2\n" << std::flush;
    std::cout << "                                                  5 write-releasing 2          *2, 3*\n" << std::flush;

    record_finished(5);
}
void thr_code6() {
    sleep_ms(1*SLEEP_SCALE);
    std::cout << "                                                  6 read-locking    1--6...\n" << std::flush;
    auto rlock = c[1]->readLock(c[2], c[3], c[4], c[5], c[6]);
    std::cout << "                                                  6 read-locked     1--6\n" << std::flush;
    sleep_ms(10*SLEEP_SCALE);
    std::cout << "                                                  6 read-releasing  1--6       *6, 2*\n" << std::flush;

    record_finished(6);
}
void thr_code7() {
    sleep_ms(2*SLEEP_SCALE);
    std::cout << "                                                  7 read-locking    2...\n" << std::flush;
    auto rlock = c[2]->readLock();
    std::cout << "                                                  7 read-locked     2\n" << std::flush;
    sleep_ms(8*SLEEP_SCALE);
    std::cout << "                                                  7 read-releasing  2          *1, 1*\n" << std::flush;

    record_finished(7);
}
void thr_code8() {
    sleep_ms(4*SLEEP_SCALE);
    std::cout << "                                                  8 read-locking    2...\n" << std::flush;
    auto rlock = c[2]->readLock();
    std::cout << "                                                  8 read-locked     2\n" << std::flush;
    sleep_ms(2*SLEEP_SCALE);
    std::cout << "                                                  8 read-releasing  2          *0, 0*\n" << std::flush;

    record_finished(8);
}

    

int main() {

    auto sim = Simulation::create();
    sim->maxThreads(10);

    for (int i = 0; i < 10; i++) {
        c.push_back(sim->spawn<Quadratic>());
    }

    int seq1 = 0, seq2 = 0;
    int tests = 10;
    for (int i = 0; i < tests; i++) {
        std::vector<std::thread> threads;
        threads.push_back(std::thread(&thr_code1));
        threads.push_back(std::thread(&thr_code2));
        threads.push_back(std::thread(&thr_code3));
        threads.push_back(std::thread(&thr_code4));
        threads.push_back(std::thread(&thr_code5));
        threads.push_back(std::thread(&thr_code6));
        threads.push_back(std::thread(&thr_code7));
        threads.push_back(std::thread(&thr_code8));

        for (auto &t : threads) t.join();

        if (order == std::vector<int>({ 8, 7, 5, 1, 2, 3, 6, 4 })) {
            std::cout << "Lock test passed (8-7-5-1-2-3-6-4)\n";
            seq1++;
        }
        else if (order == std::vector<int>({ 8, 7, 6, 5, 1, 2, 3, 4 })) {
            std::cout << "Lock test passed (8-7-6-5-1-2-3-4)\n";
            seq2++;
        }
        else {
            std::cout << "Lock test FAILED (incorrect ordering: ";
            bool first = true;
            for (auto &i : order) {
                if (first) first = false;
                else std::cout << "-";
                std::cout << i;
            }
            std::cout << ")\n";
            break;
        }
        order.clear();
    }

    if (seq1 + seq2 < tests) {
        std::cout << "One or more tests FAILED!\n";
        return 1;
    }
    else {
        std::cout << "All tests passed (" << seq1 << "/" << tests << " sequence 1, " << seq2 << "/" << tests << " sequence 2)\n";
        return 0;
    }
}
