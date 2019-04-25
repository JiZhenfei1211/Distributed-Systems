#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <array>
#include <fstream>
#include <time.h>


#define PRNG_BUFSZ 8
std::mutex g_lock_print;
const int default_num_of_philosophers = 5;

class syn_controller {
    std::mutex mutex;
    std::condition_variable g_state_flag;

public:
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        g_state_flag.wait(lock);
    }

    void notify_all() {
        g_state_flag.notify_all();
    }
};

class bottle {
    int id;
    int owner;
    bool dirty;
    std::mutex mutex;
    syn_controller signal;
public:
    bottle(int const bottle_id, int const owner_id) :
            id(bottle_id), owner(owner_id), dirty(true) {

    }

    void request(int const ownerId) {

        while (owner != ownerId) {
            if (dirty) {
                std::lock_guard<std::mutex> lk(mutex);
                dirty = false;
                this->owner = ownerId;
            } else {
                signal.wait();
            }
        }
        dirty = false;
    }

    void finish_drinking() {
        std::lock_guard<std::mutex> lk(mutex);
        dirty = true;
        signal.notify_all();
    }
};

class philosopher {
private:
    int id;
    int const drinking_session;
    int session_count = 0;
    std::thread p_thread;
    std::vector<bottle *> bottles;
    syn_controller &controller;
    struct random_data* rand_state;
public:
    philosopher(int const philosopher_id, std::vector<bottle *> bottles, int const sessions, syn_controller &controller,
    struct random_data* rand_state) :
            id(philosopher_id), bottles(bottles), drinking_session(sessions), controller(controller),
            rand_state(rand_state), p_thread(&philosopher::start_session, this) {
    }

    ~philosopher() {
        p_thread.join();
    }


    void think() {
        int t;
        random_r(rand_state, &t);
        usleep(t%1000000);
        print(" thinking");
    }

    void drink() {
        for (auto b : bottles) {
            b->request(id);
        }

        print(" drinking");
        int t;
        random_r(rand_state, &t);
        usleep(t%1000000);
        for (auto b : bottles) {
            b->finish_drinking();
        }
    }

    void print(std::string const &text) {
        std::lock_guard<std::mutex> print_lock(g_lock_print);
        std::cout << "philosopher " << id << text << std::endl;

    }

    void add_bottle(bottle *bt) {
        bottles.push_back(bt);
    }

    int get_philosopher_id() {
        return id;
    }

    void start_session() {
        controller.wait();
        do {
            think();
            drink();
            session_count += 1;
        } while (session_count < drinking_session);
    }
};

class manager {
    syn_controller controller;
    int const drinking_session;
    std::vector<philosopher *> philosophers;
    struct random_data* rand_states;
    char* state_bufs;
public:
    manager(int const drinking_session) : drinking_session(drinking_session) {

    }

    ~manager() {
        for (philosopher *p: philosophers) {
            delete (p);
        }
        free(rand_states);
        free(state_bufs);
    }

    void setup(std::string filename) {
        int num_of_philosophers = 0, id_1 = 0, id_2 = 0;

        std::ifstream file;
        file.open(filename);
        file >> num_of_philosophers;

        rand_states = (struct random_data*)calloc(num_of_philosophers, sizeof(struct random_data));
        state_bufs = (char*) calloc(num_of_philosophers, PRNG_BUFSZ);

        for (int i = 0; i < num_of_philosophers; i++) {
            initstate_r(time(NULL), &state_bufs[i], PRNG_BUFSZ, &rand_states[i]);
            std::vector<bottle *> bottles;
            philosopher *p = new philosopher(i + 1, bottles, drinking_session, controller, &rand_states[i]);
            philosophers.push_back(p);
        }

        int bottle_id = 1;
        while (file >> id_1 >> id_2) {
            bottle *b = new bottle(bottle_id, std::min(id_1, id_2));
            for (philosopher *p: philosophers) {
                if (p->get_philosopher_id() == id_1 || p->get_philosopher_id() == id_2) {
                    p->add_bottle(b);
                }
            }
            bottle_id++;
        }
        file.close();
    }

    void start() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        controller.notify_all();
    }
};

void drinking_problem(int const drinking_session, std::string filename) {
    std::cout << "Drinking Problem Begin." << std::endl;

    {
        manager m(drinking_session);
        m.setup(filename);
        m.start();
    }

    std::cout << "Drinking Done." << std::endl;
}

void default_five_philosopher_cycle(int const drinking_session) {
    struct random_data* rand_states;
    char* state_bufs;
    rand_states = (struct random_data*)calloc(default_num_of_philosophers, sizeof(struct random_data));
    state_bufs = (char*) calloc(default_num_of_philosophers, PRNG_BUFSZ);
    for (int i = 0; i < default_num_of_philosophers; i++) {
        initstate_r(time(NULL), &state_bufs[i], PRNG_BUFSZ, &rand_states[i]);
    }
    std::cout << "Default Drinking Problem Begin." << std::endl;
    {
        syn_controller controller;
        std::array<bottle, default_num_of_philosophers> bottles{
                {
                        {1, 1},
                        {2, 2},
                        {3, 3},
                        {4, 4},
                        {5, 1},
                }
        };
        std::vector<bottle *> bottles_1{&bottles[0], &bottles[4]};
        std::vector<bottle *> bottles_2{&bottles[0], &bottles[1]};
        std::vector<bottle *> bottles_3{&bottles[1], &bottles[2]};
        std::vector<bottle *> bottles_4{&bottles[2], &bottles[3]};
        std::vector<bottle *> bottles_5{&bottles[3], &bottles[4]};
        std::array<philosopher, default_num_of_philosophers> philosophers{
                {
                        {1, bottles_1, drinking_session, controller, &rand_states[0]},
                        {2, bottles_2, drinking_session, controller, &rand_states[1]},
                        {3, bottles_3, drinking_session, controller, &rand_states[2]},
                        {4, bottles_4, drinking_session, controller, &rand_states[3]},
                        {5, bottles_5, drinking_session, controller, &rand_states[4]},
                }
        };

        std::this_thread::sleep_for(std::chrono::seconds(3));
        controller.notify_all();
    }
    std::cout << "Default Drinking Problem Done." << std::endl;
    free(rand_states);
    free(state_bufs);
}


int main(int argc, char **argv) {
    int opt = -1;
    int drinking_session = 20;
    bool use_cycle = true;
      
    while ((opt = getopt(argc, argv, "s:")) != -1) {
        switch (opt) {
            case 's':
                drinking_session = atoi(optarg);
                break;
        }
    }
    if(argv[optind] != NULL) use_cycle = false; 

    if (drinking_session == 0){
        drinking_session = 20;
    }
    
    if(!use_cycle){
        std::string filename = "";
        std::cout << "Please enter the filename." << std::endl;
        std::cin>>filename;
        drinking_problem(drinking_session, filename);
    } else {
        default_five_philosopher_cycle(drinking_session);
    }


    return 0;
}
