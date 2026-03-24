//
// Created by Bojie Shen on 18/9/2023.
//
#include "LNS.h"
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <string>
#include <iostream>
using namespace std;
int time_bucket = 20;

int
main(int   argc,
     char *argv[])
{
    const char *binary_name = argv[0];

    if (argc != 6) {
        std::cerr
                << std::endl
                << "USAGE: " << binary_name
                << "<datafile.csv> <queryfile.csv> <instance> <timelimit> " << std::endl
                << std::endl;

        return EXIT_FAILURE;
    }
    const std::string datafile(argv[1]);
    const std::string queryfile(argv[2]);
    const std::string instance(argv[3]);
    const double charging_factor = stod(argv[4]);
    const double order_factor = stod(argv[5]);
    const double limit = 300;


    std::ifstream file(queryfile); // Replace with your CSV file's name

    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return 0;
    }

    std::string line;
    bool first_line_skipped = false;
    int query_count = 0 ;
    LNS  lns = LNS() ;
    while (getline(file, line)) {
        if (!first_line_skipped) {
            first_line_skipped = true;
            continue; // Skip the first line
        }
        std::vector<std::string> values;
        std::istringstream ss(line);
        std::string value;
        while (getline(ss, value, ',')) { // Assuming comma-separated values
            values.push_back(value);
        }

        Query q = {stod(values[0]),stod(values[1]),stod(values[2]),stod(values[3]),
                   stod(values[4]),stod(values[5]),
                   stod(values[6]),stod(values[7]),
                   stod(values[8]),stod(values[9])};
        EV_setting ev ={ stod(values[10]), stod(values[11]), stod(values[12])};
        ev.driving_efficiency = ev.battery_capacity/ev.driving_range;

        //TODO: change here from loading melbourne to ny
        //Graph g = Graph(datafile, charging_factor, charging_factor);
        Graph g = Graph(datafile, -1);

        g.initialise_graph(q,ev,time_bucket,1, order_factor,true);


        lns.init_search(&g,&q,&ev,time_bucket);
        std::ofstream plan_output("dataset/result/dynamic/" + instance + "_" + to_string(static_cast<float>(charging_factor)) +"_" + to_string(static_cast<float>(order_factor)) + ".lns");
        std::ofstream stats_output("dataset/result/dynamic/" + instance + "_"  + to_string(static_cast<float>(charging_factor))+ "_" + to_string(static_cast<float>(order_factor)) + ".csv");
        lns.set_screen(0);
        lns.set_runtime_limit(limit);
//        lns.run_LNS_search();
        lns.run_LNS_search_greedy_repairer();
        lns.save_lns_state(stats_output);
        lns.save_solution(plan_output);
        int max_revisit = lns.get_max_cs_visit();
        query_count ++;


        cout << lns.get_order_profit() << " " << lns.get_final_profit() << endl;
        //string out = "dataset/lns_battery_" + to_string(static_cast<float>(charging_factor))+"_" + to_string(static_cast<float>(order_factor))   + ".csv";
        string out = "dataset/evop_lns_nyc.csv";
        std::ofstream myFile(out, std::ofstream::out | std::ofstream::app);
        myFile<<std::fixed<<setprecision(8)<< instance  << "," << lns.get_order_profit() << "," << lns.get_final_profit() << "\n";
        myFile.close();

//        cout << out << endl;
//        string visit_out = "dataset/result/small/lns/" + instance   + "_revisit.csv";
//        std::ofstream out_file(visit_out);
//        out_file << std::fixed<<setprecision(8) << instance << "," << max_revisit << "\n";
//        out_file.close();


    }
    file.close();
    return 0;
}
