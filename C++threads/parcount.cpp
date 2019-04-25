#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <cstring>
#include <pthread.h>

//global counter
extern int counter = 0;
//atomic counter for phase 4
extern std::atomic_int counter_2(0);
std::mutex g_lock;

void without_sync(int i) {
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
}

void with_mutex(int i, int thread_id) {
    g_lock.lock();
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
    g_lock.unlock();
}

void with_lock_guard(int i, int thread_id) {
    std::lock_guard<std::mutex> locker(g_lock);
    for (int n = 1; n <= i; n++) {
        counter += 1;
    }
}

void with_fetch_add(int i, int thread_id) {
    for (int n = 0; n < i; n++) {
        counter_2.fetch_add(1);
    }
}

void with_local_counter(int i, int &local_counter, int thread_id) {
    for (int n = 1; n <= i; n++) {
        local_counter += 1;
    }
}

//phase 1: with no synchronization.
void phase_1(int num_threads, int increment, bool affinity) {
    std::vector<std::thread> threads_1;
    auto start = std::chrono::high_resolution_clock::now();
    for (int n = 0; n < num_threads; ++n) {
        threads_1.push_back(std::thread(without_sync, increment));
        if(affinity){
            cpu_set_t cpuset; 
            CPU_ZERO(&cpuset);
            CPU_SET(n % 32, &cpuset); 
            int rc = pthread_setaffinity_np(threads_1[n].native_handle(), sizeof(cpu_set_t), &cpuset);
            if (rc != 0){
                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
            }
        }
    }
    for (auto &thread : threads_1) {
        thread.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime_1 = end - start;
    std::cout << "Phase: 1" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime_1.count() << "s" << std::endl;
    std::cout << "Affinity: " << affinity << std::endl;
    
}

//phase 2: with mutex
void phase_2(int num_threads, int increment, bool affinity) {
    counter = 0;
    std::vector<std::thread> threads_2;
    auto start = std::chrono::high_resolution_clock::now();
    for (int n = 0; n < num_threads; ++n) {
        threads_2.push_back(std::thread(with_mutex, increment, n));
        if(affinity){
            cpu_set_t cpuset; 
            CPU_ZERO(&cpuset);
            CPU_SET(n % 32, &cpuset); 
            int rc = pthread_setaffinity_np(threads_2[n].native_handle(), sizeof(cpu_set_t), &cpuset);
            if (rc != 0){
                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
            }
        }
    }
    for (auto &thread : threads_2) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime_2 = end - start;
    std::cout << "Phase: 2" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime_2.count() << "s" << std::endl;
    std::cout << "Affinity: " << affinity << std::endl;
}

//phase 3: with lock_guard
void phase_3(int num_threads, int increment, bool affinity) {
    counter = 0;
    std::vector<std::thread> threads_3;
    auto start = std::chrono::high_resolution_clock::now();
    for (int n = 0; n < num_threads; ++n) {
        threads_3.push_back(std::thread(with_lock_guard, increment, n));
        if(affinity){
            cpu_set_t cpuset; 
            CPU_ZERO(&cpuset);
            CPU_SET(n % 32, &cpuset); 
            int rc = pthread_setaffinity_np(threads_3[n].native_handle(), sizeof(cpu_set_t), &cpuset);
            if (rc != 0){
                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
            }
        }
    }
    for (auto &thread : threads_3) {
        thread.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime_3 = end - start;
    std::cout << "Phase: 3" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime_3.count() << "s" << std::endl;
    std::cout << "Affinity: " << affinity << std::endl;
}

//phase 4: with atomic_int
void phase_4(int num_threads, int increment, bool affinity) {
    std::vector<std::thread> threads_4;
    auto start = std::chrono::high_resolution_clock::now();
    for (int n = 0; n < num_threads; ++n) {
        threads_4.push_back(std::thread(with_fetch_add, increment, n));
        if(affinity){
            cpu_set_t cpuset; 
            CPU_ZERO(&cpuset);
            CPU_SET(n % 32, &cpuset); 
            int rc = pthread_setaffinity_np(threads_4[n].native_handle(), sizeof(cpu_set_t), &cpuset);
            if (rc != 0){
                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
            }
        }
    }
    for (auto &thread : threads_4) {
        thread.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime_4 = end - start;
   std::cout << "Phase: 4" <<std::endl;
    std::cout << "Counter: " << counter_2 << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime_4.count() << "s" << std::endl;
    std::cout << "Affinity: " << affinity << std::endl;
}

//phase 5: with local counter
void phase_5(int num_threads, int increment, bool affinity) {
    counter = 0;
    std::vector<std::thread> threads_5;
    int local_counter[num_threads];
    memset(local_counter, 0, num_threads * sizeof(int));
    auto start = std::chrono::high_resolution_clock::now();
    for (int n = 0; n < num_threads; ++n) {
        threads_5.push_back(std::thread(with_local_counter, increment, std::ref(local_counter[n]), n));
        if(affinity){
            cpu_set_t cpuset; 
            CPU_ZERO(&cpuset);
            CPU_SET(n % 32, &cpuset); 
            int rc = pthread_setaffinity_np(threads_5[n].native_handle(), sizeof(cpu_set_t), &cpuset);
            if (rc != 0){
                std::cout<< "Error calling phread_setaffinity_np: "<< rc << "\n";
            }
        }
    }
    for (auto &thread : threads_5) {
        thread.join();
    }
    for (auto lcounter : local_counter) {
        counter += lcounter;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalTime_5 = end - start;
    std::cout << "Phase: 5" <<std::endl;
    std::cout << "Counter: " << counter << std::endl;
    std::cout << "Operations per Second: " << num_threads * increment / totalTime_5.count() << "s" << std::endl;
    std::cout << "Affinity: " << affinity << std::endl;
}


int main(int argc, const char **argv) {
    int num_threads = 4;
    int increment = 10000;
    
    //get arguments from terminal (if exists)
    //argument format: ./parcount "-t 10" "-i 20000"
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

    bool affinity = true; 

    phase_1(num_threads, increment, affinity);

    phase_2(num_threads, increment, affinity);

    phase_3(num_threads, increment, affinity);

    phase_4(num_threads, increment, affinity);

    phase_5(num_threads, increment, affinity);

    return 0;
}