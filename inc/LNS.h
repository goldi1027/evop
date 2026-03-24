//
// Created by Bojie Shen on 18/9/2023.
//

#include "graph.h"
#include <queue>
#include "cpool.h"
#include "evaluator.h"
#include "set"
#include "destructor.h"
#include "pp_repairer.h"
#include "greedy_repairer.h"
#include "utils.h"

struct Iteration_state{
    unsigned num_of_iteration;
    double best_value;
    double runtime;
    double MIP_solving_time;
};
class LNS {
    public:
        LNS() { }
        ~LNS()
        {}

        void init_search(Graph* g, Query* q, EV_setting* e, int tb){
            search_graph = g;
            query = q;
            ev = e ;
            time_bucket = tb;
            evaluator.init(g,q,e,tb);
            repairer.init(g,q,e,tb);
            g_repairer.init(g,q,e,tb);
            destructor.init(g,q,e,tb);
            num_of_iterations = 0;
            curr_best_value = 0;
            curr_best_solution.clear();
        }

        void generate_init_solution(){
            // change to call repair to initial the solution.
            curr_best_solution.push_back(Decision{search_graph->get_node_ptr(0)});
//            double arrive_time = query->time_start;
//            double battery_left = query->battery_start;
//            vector<Node*> order;
//            for(int i = 0; i < search_graph->get_number_of_nodes(); i++){
//                if(search_graph->is_order_node(i)){
//                    order.push_back(search_graph->get_node_ptr(i));
//                }
//            }
//            sort(order.begin(), order.end(), [](const auto &lhs, const auto &rhs) {
//                return lhs->object.order->profit > rhs->object.order->profit;
//            });
//            double curr_profit = 0;
//            for (auto n : order){
//                double tmp_time = arrive_time;
//                if(curr_best_solution.back().node->type == Node_type::ORDER){
//                    tmp_time = max(tmp_time,curr_best_solution.back().node->object.order->time_start);
//                    tmp_time += curr_best_solution.back().node->object.order->service_time;
//                }
//                tmp_time += search_graph->get_edge_time(curr_best_solution.back().node->graph_id,
//                                                          n->graph_id);
//                if(tmp_time > n->object.order->time_end) continue;
//
//                if(max(n->object.order->time_start, tmp_time) +
//                   n->object.order->service_time + search_graph->get_edge_time(n->graph_id, search_graph->get_d_nodes()) > search_graph->get_query_end_time()) continue;
//                arrive_time = tmp_time;
//                battery_left -=  search_graph->get_edge_distance( curr_best_solution.back().node->graph_id,n->graph_id)
//                                   * ev->driving_efficiency + search_graph->get_service_energy(n->graph_id);
//                curr_profit += n ->object.order->profit;
//                curr_best_solution.push_back(Decision{search_graph->get_node_ptr( n->graph_id),0,0,0 });
//
//            }
//            battery_left -=search_graph->get_edge_distance( curr_best_solution.back().node->graph_id,search_graph->get_d_nodes());
//            if(battery_left >= query->battery_end){
//                curr_best_value = curr_profit;
//            }
            curr_best_solution.push_back(Decision{search_graph->get_node_ptr(search_graph->get_d_nodes()),0,0,0});
        }

        void validate_evaluator(){
//            vector<int> solution = {0,19,18,26,5,9,10,32,33,27,16,25,1,2,4,6,8,20,11,12,3,21,28,34};
//            vector<int> solution = {0,18,19,26,9,5,33,10,27,33,16,25,30,23,2,4,6,8,20,29,11,3,21,33,28,32,34};
//            vector<int> solution = {0,18,19,26,9,5,33,10,27,33,16,25,30,23,2,4,6,8,20,29,11,3,21,33,28,32,34};
//            vector<int> solution = {0,33,16, 1, 33, 2,32,13,11,3,21,33,28,33,34};
//            vector<int> solution = {0, 32, 2, 13, 29, 11, 3, 21, 28, 34};
//            vector<int> solution = {0, 32, 33, 2, 13, 29, 11, 3, 21, 33, 28, 32, 33, 34};
//            vector<int> solution = { 0,10,33,27,16,25,30,1,2,4,6,8,20,29,11,3,21,28,34 };
            vector<int> solution = { 0,33,32,1,8,23,16,10,24,27,17,29,2,28,20,31,33,34};
                    vector<Decision> curr_solution;
            double profit = 0;
            for (int i = 0; i < solution.size(); i ++){
                curr_solution.push_back(Decision{search_graph->get_node_ptr(solution[i])});
                if(search_graph->is_order_node(solution[i])){
                    profit += search_graph->get_profit(solution[i]);
                }
            }
            auto result = evaluator.solve_optional_charging(curr_solution);
//            vector<bool> visit_solution_var;
//            vector<double> time_solution_var, battery_solution_var;
//            vector<vector<bool>> charging_solution_var, discharging_solution_var;
//            evaluator.get_variable_value(curr_solution,visit_solution_var,time_solution_var,battery_solution_var,charging_solution_var,discharging_solution_var);
//            cout<<"try resolving"<<endl;
//            auto result2 = evaluator.solve_optional_charging2(curr_solution,visit_solution_var,time_solution_var,battery_solution_var,charging_solution_var,discharging_solution_var);
            auto a  = evaluator.get_current_solution(curr_solution);
            validate_time_battery(a);
            curr_best_solution = a ;
            curr_best_value =result;
            save_solution(cout);
            cout <<"current profit "<< profit <<endl;
            cout <<"fixing price "<< result <<endl;
        }

        void set_screen(int n){
            screen = n;
            repairer.screen = n;
            g_repairer.screen = n;
            destructor.screen = n;
        }

        void set_runtime_limit(double limit){
            runtime_limit = limit;
        }


        void run_LNS_search(){
            iter_info.clear();
            auto start = chrono::steady_clock::now();
            double solving_time = 0;
            generate_init_solution();
            while (num_of_iterations != 10000){
                double curr_time =  (double)std::chrono::duration_cast<std::chrono::milliseconds>(chrono::steady_clock::now() - start ).count() /1000;
                if( curr_time > runtime_limit) {
                    iter_info.push_back(Iteration_state{num_of_iterations ,curr_best_value,curr_time,solving_time});
                    return;
                }
                vector<Decision> curr_solution =  curr_best_solution;
                destructor.run_destructor(curr_solution);
                curr_solution = repairer.run_repairer(curr_solution);
                assert(curr_solution.front().node->type == Node_type::SOURCE);
                assert(curr_solution.back().node->type == Node_type::DESTINATION);
                double curr_profit = 0;
                double profit_upper_bound = 0;
                int count_charger = 0;
                for(int i = 0; i < curr_solution.size()-1;){
                    if(curr_solution[i].node->type == ORDER){
                        profit_upper_bound  += curr_solution[i].node->object.order->profit;
                        curr_profit += curr_solution[i].node->object.order->profit;
                        i++;
                    }else{
                        if(curr_solution[i].node->type == CS){
                            count_charger++;
                            set<int> charging_station_visited;
                            for( int j = i; j < curr_solution.size(); j++){
                                if( curr_solution[j].node->type != CS ){
                                    int charging_begin = curr_solution[i].charging_begin;
                                    int charging_end = curr_solution[j-1].charging_end;
                                    for( int t = charging_begin; t <= charging_end; t++){
                                        double max_p = 0 ;
                                        for(auto& cs : charging_station_visited){
                                            max_p = max(max_p,search_graph->get_bucket_discharging_price(cs,t));
                                        }
                                        profit_upper_bound += max_p;
                                    }
                                    i = j-1;
                                    break;
                                }else{
                                    charging_station_visited.insert(curr_solution[j].node->graph_id);
                                }
                            }
                        }
                        i++;

                    }
                }
//                cout<<count_charger<<endl;
                if(profit_upper_bound < curr_best_value){
                    continue;
                }


                auto solve_start  = chrono::steady_clock::now();
                auto result = evaluator.solve_optional_charging(curr_solution);
                solving_time += (double)std::chrono::duration_cast<std::chrono::milliseconds>(chrono::steady_clock::now() - solve_start ).count() /1000;
                if(result != -1){
                    curr_profit = result;
                    if (curr_profit > curr_best_value ){
                        int changed_neighbour_size = abs(result - curr_best_solution.size());
                        destructor.adjust_weight(true,curr_profit,curr_best_value,changed_neighbour_size);
                        repairer.adjust_weight(true,curr_profit,curr_best_value,changed_neighbour_size);
                        curr_best_value = curr_profit;
                        curr_best_solution = evaluator.get_current_solution(curr_solution);
                        double runtime = (double)std::chrono::duration_cast<std::chrono::milliseconds>(chrono::steady_clock::now() - start ).count() /1000;
                        validate_time_battery(curr_best_solution);
                        iter_info.push_back(Iteration_state{num_of_iterations ,curr_best_value,runtime,solving_time});
//                        verify_tmp_solution(curr_best_solution);
//                        print_solution(curr_best_solution);
//                        cout<<"Found improvement, iteration number: "<<
//                        num_of_iterations<< ", curr best value: "<< curr_best_value
//                        << ", runtime: "<<runtime
//                        << ", solving runtime: "<< solving_time << endl;
                    }else{
                        destructor.adjust_weight(false,0,0,0);
                        repairer.adjust_weight(false,0,0,0);
                    }
                }else{
                    destructor.adjust_weight(false,0,0,0);
                    repairer.adjust_weight(false,0,0,0);
                }
                evaluator.reset_solver();
                num_of_iterations ++;
            }
        }


    void run_LNS_search_greedy_repairer(){
        iter_info.clear();
        auto start = chrono::steady_clock::now();
        double solving_time = 0;
        generate_init_solution();
        while (num_of_iterations != 100000){
//            cout<< num_of_iterations<<endl;
            double curr_time =  (double)std::chrono::duration_cast<std::chrono::milliseconds>(chrono::steady_clock::now() - start ).count() /1000;
            if( curr_time > runtime_limit) {
                iter_info.push_back(Iteration_state{num_of_iterations ,curr_best_value,curr_time,solving_time});
                return;
            }
            vector<Decision> curr_solution =  curr_best_solution;

            if(screen >= 2  ) {
                cout << "Printing current plan" << endl;
                for (auto &c: curr_solution) {
                    cout << *c.node << endl;
                }
            }
            destructor.run_destructor(curr_solution);

            if(screen >= 2  ) {
                cout << "Printing destroyed plan" << endl;
                for (auto &c: curr_solution) {
                    cout << *c.node << endl;
                }
            }
            g_repairer.run_repairer(curr_solution);

            if(screen >= 2  ) {
                cout << "Printing fixed plan" << endl;
                for (auto &c: curr_solution) {
                    cout << *c.node << endl;
                }
            }
            assert(curr_solution.front().node->type == Node_type::SOURCE);
            assert(curr_solution.back().node->type == Node_type::DESTINATION);
            double curr_profit = 0;
            double profit_upper_bound = 0;
            int count_charger = 0;
            for(int i = 0; i < curr_solution.size()-1;){
                if(curr_solution[i].node->type == ORDER){
                    profit_upper_bound  += curr_solution[i].node->object.order->profit;
                    curr_profit += curr_solution[i].node->object.order->profit;
                    i++;
                }else{
                    if(curr_solution[i].node->type == CS){
                        count_charger++;
                        set<int> charging_station_visited;
                        for( int j = i; j < curr_solution.size(); j++){
                            if( curr_solution[j].node->type != CS ){
                                int charging_begin = curr_solution[i].charging_begin;
                                int charging_end = curr_solution[j-1].charging_end;
                                for( int t = charging_begin; t <= charging_end; t++){
                                    double max_p = 0 ;
                                    for(auto& cs : charging_station_visited){
                                        max_p = max(max_p,search_graph->get_bucket_discharging_price(cs,t));
                                    }
                                    profit_upper_bound += max_p;
                                }
                                i = j-1;
                                break;
                            }else{
                                charging_station_visited.insert(curr_solution[j].node->graph_id);
                            }
                        }
                    }
                    i++;

                }
            }
//                cout<<count_charger<<endl;
            if(profit_upper_bound < curr_best_value){
                continue;
            }

            auto solve_start  = chrono::steady_clock::now();
            auto result = evaluator.solve_optional_charging(curr_solution);
            solving_time += (double)std::chrono::duration_cast<std::chrono::milliseconds>(chrono::steady_clock::now() - solve_start ).count() /1000;
            if(result != -1){
                curr_profit = result;
                if (curr_profit > curr_best_value ){
                    int changed_neighbour_size = abs(result - curr_best_solution.size());
                    destructor.adjust_weight(true,curr_profit,curr_best_value,changed_neighbour_size);
                    g_repairer.adjust_weight(true,curr_profit,curr_best_value,changed_neighbour_size);
                    curr_best_value = curr_profit;
                    curr_best_solution = evaluator.get_current_solution(curr_solution);
                    double runtime = (double)std::chrono::duration_cast<std::chrono::milliseconds>(chrono::steady_clock::now() - start ).count() /1000;
                    validate_time_battery(curr_best_solution);
                    verify_solved_solution();
                    iter_info.push_back(Iteration_state{num_of_iterations ,curr_best_value,runtime,solving_time});
//                        verify_tmp_solution(curr_best_solution);
//                        print_solution(curr_best_solution);
                        cout<<"Found improvement, iteration number: "<<
                        num_of_iterations<< ", curr best value: "<< curr_best_value
                        << ", runtime: "<<runtime
                        << ", solving runtime: "<< solving_time << endl;
                }else{
                    destructor.adjust_weight(false,0,0,0);
                    g_repairer.adjust_weight(false,0,0,0);
                }
            }else{
                destructor.adjust_weight(false,0,0,0);
                g_repairer.adjust_weight(false,0,0,0);
            }
            evaluator.reset_solver();
            num_of_iterations ++;
        }
    }

    bool verify_solved_solution(){
            for(int i = 0; i < curr_best_solution.size()-1; i++){
                auto curr = curr_best_solution[i];
                auto next = curr_best_solution[i+1];
                if(curr.node->type == ORDER){
                    if(curr.arrival_time< query->working_time_start - Epsilon || curr.arrival_time + curr.node->object.order->service_time > query->working_time_end+ Epsilon){
                        cout<<"invalid Order"<<endl;
                    }
                    if(curr.arrival_time + curr.node->object.order->service_time +
                    search_graph->get_edge_time(curr.node->graph_id,next.node->graph_id) > next.arrival_time + Epsilon){
                        cout<<"unreachable order"<<endl;
                    }
                    if(curr.battery_level < 0 - Epsilon  || curr.battery_level> ev->battery_capacity+ Epsilon){
                        cout<<"exceed battery capacity"<<endl;
                    }

                    if(curr.battery_level - search_graph->get_service_energy(curr.node->graph_id) -
                        ev->driving_efficiency*search_graph->get_edge_distance(curr.node->graph_id,next.node->graph_id) < next.battery_level - Epsilon
                            ){
                        cout<<"battery consumption error"<<endl;
                    }
                }
                if(curr.node->type == CS){
                    if(!search_graph->is_sd_charging_station(curr.node->graph_id)){
                        if(curr.arrival_time < query->working_time_start - Epsilon|| curr.arrival_time + curr.node->object.order->service_time > query->working_time_end+ Epsilon){
                            cout<<"invalid charging"<<endl;
                        }
                    }
                    if(curr.arrival_time > curr.charging_begin * time_bucket + query->time_start + Epsilon){
                        cout<<"charging arrive late "<<endl;
                    }
                    if((curr.charging_end + 1) * time_bucket + query->time_start +
                    search_graph->get_edge_time(curr.node->graph_id,next.node->graph_id) > next.arrival_time + Epsilon){
                        cout<<"charging too late "<<endl;
                    }

                    if(curr.battery_level < 0- Epsilon  || curr.battery_level> ev->battery_capacity+ Epsilon){
                        cout<<"exceed battery capacity"<<endl;
                    }

                    double charging_amount = 0;
                    for(int t = 0; t < curr.charging_plan.size(); t++){
                        if(curr.charging_plan[t]){
                            charging_amount+=curr.node->object.cs->bucket_charging_amount;
                        }
                        if(curr.discharging_plan[t]){
                            charging_amount-=curr.node->object.cs->bucket_charging_amount;
                        }
                    }
                    double a = curr.battery_level + charging_amount -
                               ev->driving_efficiency*search_graph->get_edge_distance(curr.node->graph_id,next.node->graph_id);
                    double b = next.battery_level - Epsilon;
                    if(a < b ){

                        cout<<"battery consumption error2"<<endl;
                    }

                }


            }
        }

    void validate_time_battery(const vector<Decision>& current_solution) {
        double arrive_time = query->time_start;
        double battery_state = query->battery_start;
        arrive_time += search_graph->get_edge_time(current_solution[0].node->graph_id,current_solution[1].node->graph_id);
        battery_state -= search_graph->get_edge_distance(current_solution[0].node->graph_id,current_solution[1].node->graph_id) * ev->driving_efficiency;
        if (battery_state < current_solution[1].battery_level - 0.001){
            cout <<"Error, battery less than minimal "<<endl;
        }else{
            // can waste energy.
            battery_state = current_solution[1].battery_level;
        }
        for(int i = 1; i < current_solution.size(); i ++){
//            cout<<"Arrive Node: "<< current_solution[i].node->graph_id <<endl;
//            cout << " Battery_state: " << battery_state <<endl;
            if(battery_state < 0 ){cout<<"insufficient battery"<<endl;}
            if(current_solution[i].node->type == ORDER){
                arrive_time =  arrive_time < query->working_time_start ? query->working_time_start : arrive_time;
                if(arrive_time > current_solution[i].node->object.order->time_end || arrive_time + current_solution[i].node->object.order->service_time> query->working_time_end){
                    cout<<"invalid tmp solution"<<endl;
                }
                arrive_time += current_solution[i].node->object.order->service_time + search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
                battery_state -= current_solution[i].node->object.order->service_energy;
                battery_state -= search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id) * ev->driving_efficiency;
            }else if(current_solution[i].node->type == CS){
                if(!search_graph->is_sd_charging_station(current_solution[i].node->graph_id)){
                    arrive_time =  arrive_time < query->working_time_start ? query->working_time_start : arrive_time;
                    if((current_solution[i].charging_end + 1)* time_bucket + query->time_start > query->working_time_end){
                        cout<<"invalid tmp solution"<<endl;
                    }
                    if((current_solution[i].charging_begin)* time_bucket + query->time_start < query->working_time_start){
                        cout<<"invalid tmp solution"<<endl;
                    }
                }
                if(arrive_time > current_solution[i].charging_begin * time_bucket+ query->time_start){
                    cout<<"invalid tmp solution"<<endl;
                }
                arrive_time = (current_solution[i].charging_end+1) * time_bucket + query->time_start +
                              search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
                double tmp_battery = battery_state;
                double charging_amount = 0;
                for(int t = 0; t < current_solution[i].charging_plan.size(); t++){
                    if(current_solution[i].charging_plan[t]){
                        battery_state +=current_solution[i].node->object.cs->bucket_charging_amount;
                        charging_amount+=current_solution[i].node->object.cs->bucket_charging_amount;
                    }
                    if(current_solution[i].discharging_plan[t]){
                        battery_state -=current_solution[i].node->object.cs->bucket_charging_amount;
                        charging_amount-=current_solution[i].node->object.cs->bucket_charging_amount;
                    }
                }

                if(battery_state > ev->battery_capacity+Epsilon){
                    cout<<" charging exceed battery amount"<<endl;
                }
                if(battery_state < 0-Epsilon){
                    cout<<" charging less than 0 "<<endl;
                }
                battery_state -= ev->driving_efficiency * search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
            }else{
                if(arrive_time > query->time_end){
                    cout<<"exceed final time"<<endl;
                }
            }
            if( i < current_solution.size() -1 ){
                if (battery_state < current_solution[i+1].battery_level - 0.001){
                    cout <<"Error, battery less than minimal "<<endl;
                }else{
                    // can waste energy.
                    battery_state = current_solution[i+1].battery_level;
                }
            }
        }
    }

    bool verify_tmp_solution(const vector<Decision>& current_solution){
        double arrive_time = query->time_start;
        arrive_time += search_graph->get_edge_time(current_solution[0].node->graph_id,current_solution[1].node->graph_id);
        for(int i = 1; i < current_solution.size(); i ++){
            if(current_solution[i].node->type == ORDER){
                if(arrive_time > current_solution[i].node->object.order->time_end){
                    cout<<"invalid tmp solution"<<endl;
                }
                arrive_time += current_solution[i].node->object.order->service_time + search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
            }else if(current_solution[i].node->type == CS){
                if(arrive_time > current_solution[i].charging_begin * time_bucket+ query->time_start){
                    cout<<"invalid tmp solution"<<endl;
                }
                arrive_time = (current_solution[i].charging_end+1) * time_bucket + query->time_start +
                              search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
            }else{
                if(arrive_time > query->time_end){
                    cout<<"exceed final time"<<endl;
                }
            }
        }
    }

    void save_lns_state(std::ostream& os){
        os<<"#iternation,best_value,runtime,MIP_time"<<endl;
            if(curr_best_value ==0 ){
                os<<-1<<","<<-1<<","<<-1<<","<<-1 <<endl;
            }else{
                for(auto& a : this->iter_info){
                    os<<a.num_of_iteration<<","<<a.best_value<<","<<a.runtime<<","<<a.MIP_solving_time <<endl;
                }
            }

        }






    void save_solution( std::ostream& os){
            if(curr_best_value == 0 ){
                os << "LNS Solution Not Found."<<endl;
            }else {
                double o_profit = 0;
                double profit = 0;
                os << "LNS Solution:" << endl;
                os << endl;
                os << *query << endl;
                os << *ev << endl;
                os << "Printing solution:" << endl;
                os << "Departure from the source: " << search_graph->get_s_nodes() << ", departure time: "
                   << search_graph->get_query_start_time() << endl;
                os << "Battery level :" << query->battery_start << endl;
                os << endl;

                auto &previous = curr_best_solution[0];
                for (auto &d: curr_best_solution) {
                    if (d.node->type == SOURCE) {
                        previous = d;
                        continue;
                    }
                    os <<std::setprecision(8) <<"Traveling from node: " << previous.node->graph_id << " to " << d.node->graph_id
                       << ", travel time: " << search_graph->get_edge_time(previous.node->graph_id, d.node->graph_id)
                       << " Energy consume: "
                       << search_graph->get_edge_distance(previous.node->graph_id, d.node->graph_id) *
                          ev->driving_efficiency << endl;
                    os << "Arrival at node: " << d.node->graph_id << " arrival time: " << d.arrival_time << endl;
                    os << *d.node << endl;
                    os << "Battery level: " << d.battery_level << endl;
                    if (d.node->type == ORDER) {
                        profit += search_graph->get_profit(d.node->graph_id);
                        o_profit += search_graph->get_profit(d.node->graph_id);
                    }
                    if (search_graph->is_charging_station_node(d.node->graph_id)) {
                        for (int t = d.charging_begin; t <= d.charging_end; t++) {
                            if (d.charging_plan[t]) {
                                os << "Charging: " << search_graph->get_query_start_time() + t * time_bucket
                                   << " to " << search_graph->get_query_start_time() + (t + 1) * time_bucket <<
                                   " energy charged: " << search_graph->get_bucket_charging_amount(d.node->graph_id)
                                   <<
                                   " charging cost: "
                                   << search_graph->get_bucket_charging_price(d.node->graph_id, t) << std::endl;
                                profit -= search_graph->get_bucket_charging_price(d.node->graph_id, t);
                            }
                            if (d.discharging_plan[t]) {
                                os << "Discharging: " << search_graph->get_query_start_time() + t * time_bucket
                                   << " to "
                                   << search_graph->get_query_start_time() + (t + 1) * time_bucket <<
                                   " energy discharged: "
                                   << search_graph->get_bucket_charging_amount(d.node->graph_id) <<
                                   " discharging cost: "
                                   << search_graph->get_bucket_discharging_price(d.node->graph_id, t) << std::endl;
                                profit += search_graph->get_bucket_discharging_price(d.node->graph_id, t);
                            }
                        }
                    }
                    previous = d;
                    os << " " << endl;
                }
                os << "Sum of Profit: " << profit << endl;
                order_profit = o_profit;
            }
        }


    int get_max_cs_visit(){
        std::unordered_map<int, int> frequencyMap;
        int lastElement = 999;

        // Count the frequency of each element, considering consecutive elements as one
        for (auto &d: curr_best_solution) {
            if(search_graph->is_charging_station_node(d.node->graph_id)){
                if (d.node->graph_id != lastElement) {
                    lastElement = d.node->graph_id;
                    frequencyMap[lastElement]++;
                }
            }

        }

        // Find the element with the highest frequency
        int mostFrequentElement = 999;
        int highestFrequency = 0;

        for (const auto& pair : frequencyMap) {
            if (pair.second > highestFrequency) {
                highestFrequency = pair.second;
                mostFrequentElement = pair.first;
            }
        }

        return highestFrequency;

        }
    void print_solution(const vector<Decision>& current_solution){
            cout<<" "<<endl;
            cout<<" "<<endl;
            cout<<"Printing solution "<<endl;
            for (auto& d : current_solution){
                cout << *d.node <<endl;
                cout << "Node Id: " << d.node->graph_id << ", Battery level: " <<  d.battery_level <<
                     ", Arrive time: " << d.arrival_time<< endl;
                if(search_graph->is_charging_station_node(d.node->graph_id)) {
                    for( int t = d.charging_begin; t <= d.charging_end; t ++){
                        if(d.charging_plan[t]) {
                            //charging
                            cout << "Charging: " << search_graph->get_query_start_time() + t * time_bucket << " to "
                                 << search_graph->get_query_start_time() + (t + 1) * time_bucket <<
                                 " energy charged: " << search_graph->get_bucket_charging_amount(d.node->graph_id)
                                 <<
                                 " charging cost: " << search_graph->get_bucket_charging_price(d.node->graph_id, t)
                                 << std::endl;
                        }
                        if(d.discharging_plan[t]) {
                            cout << "Discharging: " << search_graph->get_query_start_time() + t*time_bucket <<" to " << search_graph->get_query_start_time() + (t+1)*time_bucket<<
                                 " energy discharged: "<< search_graph->get_bucket_charging_amount(d.node->graph_id) <<
                                 " discharging cost: "<< search_graph->get_bucket_discharging_price(d.node->graph_id,t) << std::endl;

                        }
                    }
                }
                cout << " " <<endl;
            }
            cout<<"Finished printing solution "<<endl;
            cout<<" "<<endl;
            cout<<" "<<endl;
            cout<<" "<<endl;
        }



    double get_order_profit(){
            return order_profit;
        }

    double get_final_profit(){
            return curr_best_value;
        }
    private:


        int time_bucket;
        unsigned num_of_iterations;
        Graph* search_graph;
        Query* query;
        EV_setting* ev;
        Evaluator evaluator;
        Destructor destructor;
        Repairer repairer;
        Greedy_Repairer g_repairer;
        vector<Decision> curr_best_solution;
        vector<Iteration_state> iter_info;
        double curr_best_value;
        double order_profit;

        double runtime_limit;
        int screen;
};