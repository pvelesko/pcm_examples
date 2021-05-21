#include <cpucounters.h>
#include <math.h>
#include <random>
#include <omp.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#define N 10000000
using namespace pcm;

class Counter
{
public:
    std::string name;
    int umask;
    int event_selector;

    Counter(std::string _name, int _umask, int _event_selector) : name(_name),
                                                                  umask(_umask),
                                                                  event_selector(_event_selector){};

    PCM::CustomCoreEventDescription getCustomCoreEventDescription()
    {
        PCM::CustomCoreEventDescription res;
        res.event_number = event_selector;
        res.umask_value = umask;
        return res;
    }
};

void target_f(double *a, double *b, double *c, int i)
{
    /* Standart SAXPY over N elements with 10% chance
       of accessing data randomly */
    if (rand() % 10 == 0)
    {
        for (int j = 0; j < N; j++)
            c[i] = a[rand() % N] + 1.234f * b[i] / c[i];
    }
    else
    {
        for (int j = 0; j < N; j++)
            c[i] = a[i] + 1.234f * b[i];
    }
}

class CounterProgrammer
{
private:
    int call_count;

    CounterProgrammer() { call_count = 0; };

public:
    CounterProgrammer(CounterProgrammer const &) = delete;
    void operator=(CounterProgrammer const &) = delete;

    static CounterProgrammer &instance()
    {
        static CounterProgrammer inst;
        return inst;
    }

    std::vector<PCM::CustomCoreEventDescription> program(std::vector<Counter> counters, PCM *pcm_instance)
    {
        pcm_instance->resetPMU();
        std::vector<PCM::CustomCoreEventDescription> chosen_counters;
        std::cout << "Current Call: " << call_count << std::endl;
        for (int i = 0; i < 4; i++)
        {
            int idx = (call_count + i) % counters.size();
            std::cout << counters[idx].name << std::endl;
            chosen_counters.push_back(counters[idx].getCustomCoreEventDescription());
        }

        auto prg_res = pcm_instance->program(pcm::PCM::CUSTOM_CORE_EVENTS, chosen_counters.data());
        if (prg_res != PCM::Success)
        {
            std::cout << "Failed to program counters" << std::endl;
            exit(0);
        }

        call_count += 1;
        return chosen_counters;
    };
};

void dump_data(
    std::vector<Counter> target_counters,
    std::vector<std::tuple<SystemCounterState, SystemCounterState, std::vector<Counter>>> states,
    std::vector<double> t)
{
    std::ofstream myfile;
    myfile.open("counters.csv");
    std::string header("time,");
    for (auto &counter : target_counters)
        header += counter.name + ",";
    header = header.substr(0, header.size() - 1); // remove trailing comma
    myfile << header << std::endl;

    for (int i = 0; i < states.size(); i++)
    {
        std::string str;

        auto before = std::get<0>(states[i]);
        auto after = std::get<1>(states[i]);
        auto programmed_counters = std::get<2>(states[i]);

        for (auto &target_counter : target_counters)
        {
            int count = 0;
            for (int i = 0; i < 4; i++)
            {
                if (target_counter.name == programmed_counters[i].name)
                    count = getNumberOfCustomEvents(i, before, after);
            }
            str = str + std::to_string(count) + ",";
        }
        str = str.substr(0, str.size() - 1); // remove trailing comma
        myfile << std::setprecision(9) << t[i] << "," << str << std::endl;
    }
    myfile.close();
}

int main()
{
    PCM *pcm_instance = PCM::getInstance();
    std::vector<std::tuple<SystemCounterState, SystemCounterState, std::vector<Counter>>> states;
    std::vector<double>
        times;

    std::vector<Counter> target_counters;
    target_counters.push_back(Counter("LONGEST_LAT_CACHE.MISS", 0x41, 0x2E));
    target_counters.push_back(Counter("L2_RQSTS.MISS", 0x3F, 0x24));
    target_counters.push_back(Counter("RESOURCE_STALLS.ANY", 0x01, 0xA2));
    target_counters.push_back(Counter("FP_ARITH_INST_RETIRED.SCALAR_DOUBLE", 0x01, 0xC7));
    target_counters.push_back(Counter("FP_ARITH_INST_RETIRED.128B_PACKED_DOUBLE", 0x04, 0xC7));
    target_counters.push_back(Counter("FP_ARITH_INST_RETIRED.256B_PACKED_DOUBLE", 0x10, 0xC7));
    //target_counters.push_back(Counter("FP_ARITH_INST_RETIRED.512B_PACKED_DOUBLE", 0x40, 0xC7));
    target_counters.push_back(Counter("ARITH.DIVIDER_ACTIVE", 0x01, 0x14));

    double *a = (double *)malloc(N * sizeof(double));
    double *b = (double *)malloc(N * sizeof(double));
    double *c = (double *)malloc(N * sizeof(double));

    for (int i = 0; i < 100; i++)
    {
        CounterProgrammer *prg;
        auto programmed_counters = prg->instance().program(target_counters, pcm_instance);

        auto before = getSystemCounterState();
        auto t = omp_get_wtime();
        target_f(a, b, c, i);
        t = omp_get_wtime() - t;
        times.push_back(t);
        auto after = getSystemCounterState();

        auto x = std::tuple<SystemCounterState, SystemCounterState, std::vector<Counter>>(before, after, target_counters);
        states.push_back(x);

        std::cout << t << std::endl;
    }

    dump_data(target_counters, states, times);
}