#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <zconf.h>
#include <random>
#include <fstream>
#include "mcs_lock.h"
#include "k42_mcs_lock.h"
#include "clh_lock.h"


//global counter
int counter = 0;
std::mutex g_lock;
std::atomic_bool start_flag{false};
using namespace mcs_space;
using namespace k42_mcs_space;
using namespace clh_space;

void write_file(float time){
    std::ofstream outfile;
    outfile.open("a.txt", std::ios::out | std::ios::app);
    outfile<<time<<std::endl;
    outfile.close();

}

struct clh_node{
    std::atomic_bool succ_must_wait{false};
};

std::vector<clh_node*> thread_clh_node_ptrs;

class k42_clh_lock {
    clh_node dummy;
    std::atomic<clh_node*> tail {&dummy};
    std::atomic<clh_node*> head;

public:
    void acquire(int threadId){
        clh_node* p = thread_clh_node_ptrs[threadId];
        p->succ_must_wait.store(true);
        clh_node* pred = tail.exchange(p, std::memory_order_acquire);
        while (pred->succ_must_wait.load());
        head.store(p);
        thread_clh_node_ptrs[threadId] = pred;
    }
    void release(){
        head.load()->succ_must_wait.store(false, std::memory_order_release);
    }
};

class TAS_Lock {
private:
    std::atomic_bool Locked{false};
    const int base = 1000;
    const int limit = 1000000;
    const int multiplier = 2;
public:
    void acquire(){
        while (Locked.exchange(true, std::memory_order_acquire)) ;
    }
    void release(){
        Locked.store(false, std::memory_order_release);
    }
    void acuqire_with_backoff(){
        int delay = base;
        while (true){
            if (Locked.exchange(true, std::memory_order_acquire)){
                for(int i = 0; i < base; i++){}
                delay = std::min(delay*multiplier, limit);
            } else {
                break;
            }
        }
    }

};

class Ticket_Lock {
    std::atomic_int next_ticket{0};
    std::atomic_int now_serving{0};
    const int base = 20000;
public:
    void acquire(){
        int my_ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
        while (true){
            int ns = now_serving.load(std::memory_order_acquire);
            if (ns == my_ticket) {
                break;
            }
        }
    }
    void acquire_with_backoff(){
        int my_ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
        while (true){
            int ns = now_serving.load(std::memory_order_acquire);
            if (ns == my_ticket) {
                break;
            }
            for(int i = 0; i < base * (my_ticket - ns); i++){}
        }
    }
    void release(){
        const auto t = now_serving.load(std::memory_order_relaxed) + 1;
        now_serving.store(t, std::memory_order_release);
    }
};

void with_mutex(int i) {
    while (!start_flag);

    g_lock.lock();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    g_lock.unlock();
}

void with_tas_lock(int i, TAS_Lock& lock){
    while (!start_flag);

    lock.acquire();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    lock.release();
}

void tas_with_backoff(int i, TAS_Lock& lock) {
    while (!start_flag);

    lock.acuqire_with_backoff();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    lock.release();
}

void with_ticket_lock(int i, Ticket_Lock& lock) {
    while (!start_flag);

    lock.acquire();
    for (int n = 0; n < i; n++) {
        counter += 1;
    }
    lock.release();
}

void ticket_with_backoff(int i, Ticket_Lock& lock) {
    while (!start_flag);

    lock.acquire_with_backoff();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    lock.release();
}

void with_mcs_lock(int i, mcs_space::MCS_Lock& mcsLock) {
    while (!start_flag);

    mcsLock.acquire();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    mcsLock.release();
}

void mcs_k42(int i, k42_mcs_space::k42_mcs_lock& lock){
    while (!start_flag);

    lock.acquire();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    lock.release();
}

void with_clh(int i, clh_space::clh_lock& lock){
    while (!start_flag);

    lock.acquire();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    lock.release();
}

void with_k42_clh(int i, int threadId, k42_clh_lock& lock){
    while (!start_flag);

    lock.acquire(threadId);
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    lock.release();
}

//1: a C++ mutex
void c_mutex(int num_threads, int increment, bool affinity) {

    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(with_mutex, increment));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_2[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }

    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "1. C++ Mutex" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//2: a naive TAS lock
void tas_lock(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;
        TAS_Lock tasLock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(with_tas_lock, increment, std::ref(tasLock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "2. Naive TAS Lock" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//3: a TAS lock with exponential backoff
void tas_lock_with_backoff(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;
        TAS_Lock tasLock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(tas_with_backoff, increment, std::ref(tasLock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_3[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "3. TAS Lock with Backoff" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//4. a naive ticket lock
void ticket_lock(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;
        Ticket_Lock lock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(with_ticket_lock, increment, std::ref(lock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_4[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "4. Naive Ticket Lock" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//5. a ticket lock with proportional backoff
void ticket_lock_with_backoff(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;
        Ticket_Lock lock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(ticket_with_backoff, increment, std::ref(lock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_4[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "5. Ticket Lock with proportional backoff" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//6. a MCS lock
void mcs_lock(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;

        mcs_space::MCS_Lock lock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(with_mcs_lock, increment, std::ref(lock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_4[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "6. MCS Lock" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//7. a K42 MCS lock
void k42_mcs(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;

        k42_mcs_space::k42_mcs_lock lock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(mcs_k42, increment, std::ref(lock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_4[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "7. K42 MCS Lock" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//8. a CLH lock
void clh(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;

        clh_space::clh_lock lock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            threads.push_back(std::thread(with_clh, increment, std::ref(lock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_4[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "8. CLH Lock" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}

//9. a K42 CLH lock
void k42_clh(int num_threads, int increment, bool affinity) {
    auto start = std::chrono::high_resolution_clock::now();
    {
        counter = 0;

        k42_clh_lock lock;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n) {
            thread_clh_node_ptrs.push_back(new clh_node());
            threads.emplace_back(std::thread(with_k42_clh, increment, n, std::ref(lock)));
//        if(affinity){
//            cpu_set_t cpuset;
//            CPU_ZERO(&cpuset);
//            CPU_SET(n % 32, &cpuset);
//            int rc = pthread_setaffinity_np(threads_4[n].native_handle(), sizeof(cpu_set_t), &cpuset);
//            if (rc != 0){
//                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
//            }
//        }
        }
        start_flag = true;
        for (auto &thread : threads) {
            thread.join();
        }
    }
    start_flag = false;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime = end - start;
    std::cout << "9.K42 CLH Lock" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime.count() << "s" << std::endl;
    write_file(num_threads * increment / totalTime.count());
    std::cout << "Affinity: " << affinity << std::endl;
}



int main(int argc, const char **argv) {
    int num_threads = 5;
    int increment = 50000;

    // get arguments from terminal (if exists)
    // argument format: ./parcount "-t 10" "-i 20000"
    if (argc > 1) {
        for (int m = 0; m < argc; m++) {
            std::string arg = std::string(argv[m]);
            size_t t_found = arg.find("-t");
            size_t i_found = arg.find("-i");
            if (t_found != std::string::npos) {
                num_threads = std::stoi(arg.erase(0, 2));
            }
            if (i_found != std::string::npos) {
                increment = std::stoi(arg.erase(0, 2));
            }
        }
    }

    std::cout << "Number of threads: " << num_threads << ", Increment: " << increment << std::endl;

    bool affinity = false;

    std::cout << "-------------------------------"<< std::endl;
    c_mutex(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    tas_lock(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    tas_lock_with_backoff(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    ticket_lock(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    ticket_lock_with_backoff(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    mcs_lock(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    k42_mcs(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    clh(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;
    k42_clh(num_threads, increment, affinity);
    std::cout << "-------------------------------"<< std::endl;

    return 0;
}