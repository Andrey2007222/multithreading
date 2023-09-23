#include <iostream>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <iostream>
#include <fstream>

long long simulate(int goal, std::default_random_engine& rng) {
    int pos = 0;
    long long stepCount = 0;

    
    int facing = +1;

    auto zeroto3 = std::uniform_int_distribution<>(0, 3);
    while (pos < goal) {
        stepCount++;
        
        if (pos == 0) {
            facing = +1;
            pos = 1;
            continue;
        }

        

        int val = zeroto3(rng);
        if (val <= 1) {
            
            pos += facing;
        }
        else if (val == 2) {
            
            stepCount++;
            pos += facing;
        }
        else {
            
            stepCount++;
            facing = -facing;
            pos += facing;
        }
    }
    return stepCount;
}

const int INITIAL_GOAL = 3;
const int FINAL_GOAL = 10000;

const int THREAD_COUNT = 8;
const int SAMPLE_COUNT = 10000;

std::mutex testNo_check_mutex;
std::mutex output_mutex;
std::mutex next_sim_mutex;
std::mutex all_waiting_mutex;

std::condition_variable next_sim;
std::condition_variable all_waiting;

void simulate_task(std::atomic<int>* testNo, std::atomic<int>* goal, std::atomic<int>* threadsWaiting, std::atomic<long long>* sum) {

    int seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine rng(seed);

    while (true) {

        testNo_check_mutex.lock();
        if (testNo->load() >= SAMPLE_COUNT) {
            testNo_check_mutex.unlock();

            std::unique_lock<std::mutex> cond_lock(next_sim_mutex);
            threadsWaiting->fetch_add(1);
            all_waiting.notify_all();
            next_sim.wait(cond_lock);

            if (goal->load() > FINAL_GOAL) {
                return;
            }

        }
        else {
            testNo->fetch_add(1);
            testNo_check_mutex.unlock();
        }


        long long result = simulate(goal->load(), rng);

        sum->fetch_add(result);
        
    }
}

int main()
{
    std::atomic<int> testNo = 0;
    std::atomic<int> goal = INITIAL_GOAL;
    std::atomic<int> threadsWaiting = 0;

    std::atomic<long long> sum = 0;

    std::ofstream resultFile;
    resultFile.open("--- Removed for privacy ---");
    if (!resultFile.is_open()) {
        std::cout << "Failed to open file." << std::endl;
        return -1;
    }

    std::thread Threadpool[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        Threadpool[i] = std::thread(simulate_task, &testNo, &goal, &threadsWaiting, &sum);
    }

    while (goal.load() <= FINAL_GOAL) {
        std::unique_lock<std::mutex> cond_lock(next_sim_mutex);

       
        while (threadsWaiting.load() < THREAD_COUNT) {
            all_waiting.wait(cond_lock);
        }

        resultFile << goal.load() << ";" << sum.load() / SAMPLE_COUNT << "\n";
        double percentage = ((double)goal.load() * 100) / FINAL_GOAL;
        std::cout << "\rProgress: " << goal.load() << " of " << FINAL_GOAL << " (" << percentage << "%)        ";

        
        testNo.store(0);
        goal.fetch_add(1);
        sum.store(0);

        threadsWaiting = 0;

        next_sim.notify_all();
    }

    std::cout << "done!" << std::endl;

    for (int i = 0; i < THREAD_COUNT; i++) {
        Threadpool[i].join();
    }

    resultFile.close();
    std::cout << "And this was hopefully not a massive waste of time!\n";
    return 0;
}