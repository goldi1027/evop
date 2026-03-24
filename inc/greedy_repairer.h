//
// Created by Bojie Shen on 13/10/2023.
//

#pragma once
#include <random>
#include "utils.h"

enum Greedy_Repair_strategy { GREEDY_CLOSE, GREEDY_PRICE, REGRET_2_CLOSE ,REGRET_2_PRICE, GREEDY_REP_COUNT};
enum Greedy_Charging_Repair_strategy { CLOSE_CHARGING, PRICE_CHARGING};
enum Greedy_Order_Repair_strategy { GREEDY, REGRET_2};
class Greedy_Repairer {

public:
    int screen = 0;

    Greedy_Repairer() {repair_weights.assign(GREEDY_REP_COUNT,1);};

    void init(Graph* g, Query* q, EV_setting* e,int tb ){
        time_bucket = tb;
        search_graph = g;
        query = q;
        ev = e;
        order_visited.resize(g->get_number_of_nodes());
    }

    void rouletteWheel()
    {
        double sum = 0;
        for (const auto& h : repair_weights)
            sum += h;
        if (screen >= 2)
        {
            cout << "repair weights = ";
            for (const auto& h : repair_weights)
                cout << h / sum << ",";
        }
        double r = (double) rand() / RAND_MAX;
        double threshold = repair_weights[0];
        selected_neighbor = 0;
        while (threshold < r * sum)
        {
            selected_neighbor++;
            threshold += repair_weights[selected_neighbor];
        }
    }

    void chooseRepairHeuristicbyALNS()
    {
        rouletteWheel();
        switch (selected_neighbor)
        {
            case 0 :
                repair_strategy = GREEDY_CLOSE;
                order_strategy = GREEDY;
                charging_strategy = CLOSE_CHARGING;
                break;
            case 1 :
                repair_strategy = GREEDY_PRICE;
                order_strategy = GREEDY;
                charging_strategy = PRICE_CHARGING;
                break;
            case 2 :
                repair_strategy = REGRET_2_CLOSE;
                order_strategy = REGRET_2;
                charging_strategy = CLOSE_CHARGING;
                break;
            case 3 :
                repair_strategy = REGRET_2_PRICE;
                order_strategy = REGRET_2;
                charging_strategy = PRICE_CHARGING;
                break;
            default : cerr << "ERROR" << endl; exit(-1);
        }
        if (screen >= 2) {
            cout << "Repair strategy: ";
            switch (repair_strategy) {
                case GREEDY_CLOSE:
                    cout << "Greedy order + Close charging" << endl;
                    break;
                case GREEDY_PRICE:
                    cout << "Greedy order + Price charging" << endl;
                    break;
                case REGRET_2_CLOSE:
                    cout << "Regret 2 + Close charging" << endl;
                    break;
                case REGRET_2_PRICE:
                    cout << "Regret 2 + Price charging" << endl;
                    break;
            }
        }
    }

    void run_repairer(vector<Decision>& current_solution){
        chooseRepairHeuristicbyALNS();
        repair_solution(current_solution);
    }

    void initialise_EDLA(vector<Decision>& current_solution){

        double earliest_departure= query ->time_start;
        for (int i  = 0 ; i < current_solution.size() - 1 ; i ++){
            if ( search_graph->is_order_node(current_solution[i].node->graph_id)){
                earliest_departure = earliest_departure <= query->working_time_start ? query->working_time_start: earliest_departure;
                earliest_departure = max( earliest_departure, search_graph->get_order_start_time(current_solution[i].node->graph_id));
                if ( earliest_departure > search_graph->get_order_end_time(current_solution[i].node->graph_id)){
                    cout<< "error, can not arrive later than service finish. "<<endl;
                }
                earliest_departure += search_graph->get_service_time(current_solution[i].node->graph_id);
            }else if (search_graph->is_charging_station_node(current_solution[i].node->graph_id)){
                earliest_departure = (current_solution[i].charging_end + 1 ) * time_bucket + query->time_start;
            }
            current_solution[i].earliest_departure = earliest_departure;
            earliest_departure += search_graph->get_edge_time(current_solution[i].node->graph_id, current_solution[i+1].node->graph_id);
        }


        double left = query->time_end;
        for (int i  = current_solution.size() - 1 ; i > 0 ; i --){
            if ( search_graph->is_order_node(current_solution[i].node->graph_id)){
                left = left >= query->working_time_end ?  query->working_time_end: left;
                left -= search_graph->get_service_time(current_solution[i].node->graph_id);
                left = min(left, search_graph->get_order_end_time(current_solution[i].node->graph_id));
                if ( left < search_graph->get_order_start_time(current_solution[i].node->graph_id)){
                    cout<< "error, can not arrive early than service time begin. "<<endl;
                }
            }else if (search_graph->is_charging_station_node(current_solution[i].node->graph_id)){
                if(left < current_solution[i].charging_begin*time_bucket+query->time_start){
                    cout<<" error, can not arrive early than charging time. "<<endl;
                }
                left = current_solution[i].charging_begin*time_bucket+query->time_start;
            }
            current_solution[i].latest_arrive= left ;
            left -= search_graph->get_edge_time(current_solution[i-1].node->graph_id, current_solution[i].node->graph_id);
        }
    }


    void update_EDLA(vector<Decision>& current_solution , int position){
        double earliest_departure= current_solution[position].earliest_departure;
        earliest_departure += search_graph->get_edge_time(current_solution[position].node->graph_id, current_solution[position+1].node->graph_id);
        for (int i  = position + 1 ; i < current_solution.size() - 1 ; i ++){
            if ( search_graph->is_order_node(current_solution[i].node->graph_id)){
                earliest_departure = earliest_departure <= query->working_time_start ? query->working_time_start: earliest_departure;
                earliest_departure = max( earliest_departure, search_graph->get_order_start_time(current_solution[i].node->graph_id));
                if ( earliest_departure > search_graph->get_order_end_time(current_solution[i].node->graph_id)){
                    cout<< "error, can not arrive later than service finish. "<<endl;
                }
                earliest_departure += search_graph->get_service_time(current_solution[i].node->graph_id);
            }else if (search_graph->is_charging_station_node(current_solution[i].node->graph_id)){
                earliest_departure = (current_solution[i].charging_end + 1 ) * time_bucket + query->time_start;
            }
            current_solution[i].earliest_departure = earliest_departure;
            earliest_departure += search_graph->get_edge_time(current_solution[i].node->graph_id, current_solution[i+1].node->graph_id);
        }

        double left = current_solution[position + 2].latest_arrive;
        left -= search_graph->get_edge_time(current_solution[position + 1].node->graph_id, current_solution[position + 2].node->graph_id);
        for (int i  = position + 1; i > 0 ; i --){
            if ( search_graph->is_order_node(current_solution[i].node->graph_id)){
                left = left >= query->working_time_end ?  query->working_time_end: left;
                left -= search_graph->get_service_time(current_solution[i].node->graph_id);
                left = min(left, search_graph->get_order_end_time(current_solution[i].node->graph_id));
                if ( left < search_graph->get_order_start_time(current_solution[i].node->graph_id)){
                    cout<< "error, can not arrive early than service time begin. "<<endl;
                }
            }else if (search_graph->is_charging_station_node(current_solution[i].node->graph_id)){
                if(left < current_solution[i].charging_begin*time_bucket+query->time_start){
                    cout<<" error, can not arrive early than charging time. "<<endl;
                }
                left = current_solution[i].charging_begin*time_bucket+query->time_start;
            }
            current_solution[i].latest_arrive= left ;
            left -= search_graph->get_edge_time(current_solution[i-1].node->graph_id, current_solution[i].node->graph_id);
        }
    }

    pair<int,Node*> compute_greedy_insertion(const vector<Decision>& current_solution){
        vector<tuple<double,int,Node*>> rank_list;
        for(auto& o : search_graph->get_order_nodes()){
            if(!order_visited[o]){
                for ( int i = 1 ; i < current_solution.size() ; i ++){
                    double earliest_departure = current_solution[i-1].earliest_departure;
                    double latest_arrive = current_solution[i].latest_arrive;
                    const Node* prev = current_solution[i-1].node;
                    const Node* next = current_solution[i].node;
                    Node* curr = search_graph->get_node_ptr(o);
                    double arrive_time = earliest_departure + search_graph->get_edge_time(prev->graph_id , curr->graph_id);
                    arrive_time = arrive_time <= query->working_time_start ? query->working_time_start : arrive_time;
                    if( arrive_time  < curr->object.order->time_end && arrive_time + search_graph->get_service_time(curr->graph_id) < query->working_time_end ) {
                        double arrive_next_time = max(arrive_time, curr->object.order->time_start) + search_graph->get_service_time(curr->graph_id)
                                                  + search_graph ->get_edge_time(curr->graph_id , next->graph_id);
                        if ( arrive_next_time <  latest_arrive){
//                           search_graph->get_all_discharging_price()
                            double arrive_fwd = current_solution[i - 1].battery_fwd -
                                                search_graph->get_edge_distance(current_solution[i-1].node->graph_id,o) * ev->driving_efficiency;
                            double arrive_bwd = current_solution[i].battery_bwd +
                                                search_graph->get_edge_distance(o,current_solution[i].node->graph_id) * ev->driving_efficiency;
                            if(arrive_fwd < arrive_bwd) continue;
                            double energy_consume = search_graph->get_service_energy(curr->graph_id)
                                                    + search_graph->get_edge_distance(prev->graph_id,curr->graph_id)* ev->driving_efficiency
                                                    +search_graph->get_edge_distance(curr->graph_id,next->graph_id)* ev->driving_efficiency;
                            double score = curr->object.order->profit - energy_consume*search_graph->get_all_time_highest_charging_price();
                            rank_list.push_back(make_tuple(score, i-1, curr));
                        }
                    }
                }
            }
        }
        // add randomness;
        if(rank_list.empty()){
            return make_pair(-1, nullptr);
        }else {
            std::sort(rank_list.begin(), rank_list.end(), [](const auto &lhs, const auto &rhs) {
                return get<0>(lhs) > get<0>(rhs);
            });
            double r = (double) rand() / (double) RAND_MAX;
            int selected_index = pow(r, degree_of_random) * rank_list.size();
            return make_pair(get<1>(rank_list[selected_index]), get<2>(rank_list[selected_index]));
        }
    }



    pair<int,Node*> compute_regret_k_insertion(const vector<Decision>& current_solution, int k_value){
//        double best_score = -1 ;
//        Node* best_order = nullptr;
//        int best_insertion_place = -1;
        vector<tuple<double,int,Node*>> rank_list;
        for(auto& o : search_graph->get_order_nodes()){
            if(!order_visited[o]){
                double best_first = -1;
                int best_first_place = -1;
                vector<double> cost_vector = vector<double>(current_solution.size(),-1) ;
                Node* curr = search_graph->get_node_ptr(o);
                for ( int i = 1 ; i < current_solution.size() ; i ++){
                    double earliest_departure = current_solution[i-1].earliest_departure;
                    double latest_arrive = current_solution[i].latest_arrive;
                    const Node* prev = current_solution[i-1].node;
                    const Node* next = current_solution[i].node;
                    double arrive_time = earliest_departure + search_graph->get_edge_time(prev->graph_id , curr->graph_id);
                    arrive_time = arrive_time <= query->working_time_start ? query->working_time_start : arrive_time;
                    if( arrive_time  < curr->object.order->time_end  && arrive_time + search_graph->get_service_time(curr->graph_id) < query->working_time_end ) {
                        double arrive_next_time = max(arrive_time, curr->object.order->time_start) + search_graph->get_service_time(curr->graph_id)
                                                  + search_graph ->get_edge_time(curr->graph_id , next->graph_id);
                        if ( arrive_next_time <  latest_arrive){
                            double arrive_fwd = current_solution[i - 1].battery_fwd -
                                                search_graph->get_edge_distance(current_solution[i-1].node->graph_id,o) * ev->driving_efficiency;
                            double arrive_bwd = current_solution[i].battery_bwd +
                                                search_graph->get_edge_distance(o,current_solution[i].node->graph_id) * ev->driving_efficiency;
                            if(arrive_fwd < arrive_bwd) continue;
                            double energy_consume = search_graph->get_service_energy(curr->graph_id)
                                                    + search_graph->get_edge_distance(prev->graph_id,curr->graph_id)* ev->driving_efficiency
                                                    +search_graph->get_edge_distance(curr->graph_id,next->graph_id)* ev->driving_efficiency;
                            double cost = curr->object.order->profit - energy_consume*search_graph->get_all_time_highest_charging_price();
                            cost_vector[i] = cost ;
                            if(cost > best_first){
                                best_first = cost;
                                best_first_place = i - 1;
                            }
                        }
                    }
                }
                std::sort(cost_vector.begin(), cost_vector.end(), [](const auto &lhs, const auto &rhs) {
                    return lhs > rhs;
                });
                double regret_value = best_first ;
                for( int i  = 1;  i < k_value; i ++){
                    // from second item to  k ;
                    if(cost_vector[i] == -1 ){
                        // no valid place can be inserted.
                        regret_value  += best_first;
                    }else{
                        regret_value += best_first - cost_vector[i];
                    }
                }
                rank_list.push_back(make_tuple(regret_value, best_first_place, curr));
            }
        }
        // add randomness
        if(rank_list.empty()){
            return make_pair(-1, nullptr);
        }else {
            std::sort(rank_list.begin(), rank_list.end(), [](const auto &lhs, const auto &rhs) {
                return get<0>(lhs) > get<0>(rhs);
            });
            double r = (double) rand() / (double) RAND_MAX;
            int selected_index = pow(r, degree_of_random) * rank_list.size();
            // add randomness;
            return make_pair(get<1>(rank_list[selected_index]), get<2>(rank_list[selected_index]));
        }
    }


    bool insert_order_decision(vector<Decision>& current_solution){
        pair<int, Node*> best_insertion;
        switch (order_strategy) {
            case GREEDY:
                best_insertion = compute_greedy_insertion(current_solution);
                break;
            case REGRET_2:
                best_insertion = compute_regret_k_insertion(current_solution,regret_k_value);
                break;
        }
        if(best_insertion.first == -1) return 0;
        current_solution.insert(current_solution.begin() + best_insertion.first + 1,
                                Decision{best_insertion.second});
        order_visited[best_insertion.second->graph_id]  = true;
        update_EDLA(current_solution,best_insertion.first);
        update_battery_state(current_solution,best_insertion.first);
        return  1;
    }



    void initialise_battery_state(vector<Decision>& current_solution) {
        double battery_fwd = query->battery_start;
        double battery_bwd = query->battery_end;
        for(int i = 0; i < current_solution.size() - 1; i++){
            if(current_solution[i].node->type ==  CS){
                for ( int t = current_solution[i].charging_begin; t <=current_solution[i].charging_end; t ++){
                    if(current_solution[i].charging_plan[t]){
                        battery_fwd += search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                    }else if(current_solution[i].discharging_plan[t]){
                        battery_fwd -= search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                    }
                }
            }else{
                battery_fwd -= search_graph->get_service_energy(current_solution[i].node->graph_id);
            }
            current_solution[i].battery_fwd = battery_fwd;
            battery_fwd -= ev->driving_efficiency * search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
        }
        current_solution[ current_solution.size() - 1].battery_fwd = battery_fwd;


        for(int i = current_solution.size() - 1; i > 0; i--){
            current_solution[i].battery_bwd = battery_bwd;
            if(current_solution[i].node->type ==  CS){
                for ( int t = current_solution[i].charging_begin; t <=current_solution[i].charging_end; t ++){
                    if(current_solution[i].charging_plan[t]){
                        battery_bwd -= search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                    }else if(current_solution[i].discharging_plan[t]){
                        battery_bwd += search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                    }
                }
            }else{
                battery_bwd += search_graph->get_service_energy(current_solution[i].node->graph_id);
            }
            battery_bwd += ev->driving_efficiency * search_graph->get_edge_distance(current_solution[i-1].node->graph_id,current_solution[i].node->graph_id);
        }
        current_solution[0].battery_bwd = battery_bwd;
    }

    void update_battery_state(vector<Decision>& current_solution , int position) {
//        cout << "Before Updating: "<<endl;
//        for ( auto c: current_solution){
//            cout << *c.node << " battery_state: "<<  c.battery_fwd << " "<<c.battery_bwd << endl;
//        }
        double battery_fwd = current_solution[position].battery_fwd;
        double battery_bwd = current_solution[position+2].battery_bwd ;
        for(int i = position; i < current_solution.size() - 1; i++){
            if ( i != position) {
                if (current_solution[i].node->type == CS) {
                    for (int t = current_solution[i].charging_begin; t <= current_solution[i].charging_end; t++) {
                        if (current_solution[i].charging_plan[t]) {
                            battery_fwd += search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                        } else if (current_solution[i].discharging_plan[t]) {
                            battery_fwd -= search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                        }
                    }
                } else {
                    battery_fwd -= search_graph->get_service_energy(current_solution[i].node->graph_id);
                }
            }
            current_solution[i].battery_fwd = battery_fwd;
            battery_fwd -= ev->driving_efficiency * search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
        }
        current_solution[ current_solution.size() - 1].battery_fwd = battery_fwd;

        for(int i = position+2; i > 0; i--){
            current_solution[i].battery_bwd = battery_bwd;
//            if ( i != position + 2 ) {
                if (current_solution[i].node->type == CS) {
                    for (int t = current_solution[i].charging_begin; t <= current_solution[i].charging_end; t++) {
                        if (current_solution[i].charging_plan[t]) {
                            battery_bwd -= search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                        } else if (current_solution[i].discharging_plan[t]) {
                            battery_bwd += search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                        }
                    }
                } else {
                    battery_bwd += search_graph->get_service_energy(current_solution[i].node->graph_id);
                }
//            }
            battery_bwd += ev->driving_efficiency * search_graph->get_edge_distance(current_solution[i-1].node->graph_id,current_solution[i].node->graph_id);
        }
        current_solution[0].battery_bwd = battery_bwd;
//        cout << "Updating: "<<endl;
//        for ( auto c: current_solution){
//            cout << *c.node << " battery_state: "<<  c.battery_fwd << " "<<c.battery_bwd << endl;
//        }
//        bool a = 0;
    }





    bool insert_charging_decision(vector<Decision>& current_solution){
        if ( charging_inserted  >= charging_decision_limit ) return 0;

        vector<int> inserting_index;
        for ( int i = 1; i < current_solution.size(); i ++){
            inserting_index.push_back(i);
        }

        auto rng = std::default_random_engine{};
        std::shuffle(std::begin(inserting_index), std::end(inserting_index), rng);

        int selected_cs = -1;
        int max_position = -1;
        int best_begin_timeslot = 0;
        int best_end_timeslot = 0;
        for ( int position : inserting_index ){
            max_position = position;
            int num_charging_before = 0 ;
            int check_position = max_position - 1;
            // dont allow continue of more than 4 charging decision.
            while(current_solution[check_position].node->type == CS){
                num_charging_before ++ ;
                if ( num_charging_before > continue_charging_decision_limit - 1){
                    continue;
                }
                check_position -- ;
            }

            const Node* prev = current_solution[max_position-1].node;
            const Node* next = current_solution[max_position].node;
            double earliest_departure = current_solution[max_position-1].earliest_departure;
            double latest_arrive = current_solution[max_position].latest_arrive;
            int prev_charging_id = next->type == CS? next->graph_id : -1;
            int next_charging_id = prev->type == CS? prev->graph_id : -1;

            const auto& cs_nodes = search_graph->get_cs_nodes();
            vector<tuple<double,int,int,int>> charging_decision = vector<tuple<double, int,int,int>>(0);
            // select charging station
            switch (charging_strategy) {
                case CLOSE_CHARGING:
                    for ( auto c : cs_nodes){
                        if(c == prev_charging_id || c == next_charging_id) continue;

                        double arrive =  earliest_departure + search_graph->get_edge_time(prev->graph_id,c);
                        if(!search_graph->is_sd_charging_station(c)){
                            arrive = arrive <= query->working_time_start ? query->working_time_start : arrive;
                        }
                        double left = latest_arrive - search_graph->get_edge_time(c,next->graph_id);
                        if(!search_graph->is_sd_charging_station(c)){
                            left = left >= query->working_time_end? query->working_time_end : left;
                        }

                        int begin_timeslot = ceil((arrive - query->time_start)/time_bucket);
                        int end_timeslot = floor((left - query->time_start)/time_bucket) - 1;
                        if ( begin_timeslot > end_timeslot ) continue;
                        double arrive_fwd = current_solution[max_position-1].battery_fwd -
                                            search_graph->get_edge_distance(current_solution[max_position-1].node->graph_id,c) * ev->driving_efficiency;
                        double arrive_bwd = current_solution[max_position].battery_bwd +
                                            search_graph->get_edge_distance(c,current_solution[max_position].node->graph_id) * ev->driving_efficiency;
                        if(arrive_fwd  <  0) continue;
                        if(arrive_bwd +  ev->battery_capacity*charging_margin >= arrive_fwd ){
                            if ( arrive_fwd + search_graph->get_bucket_charging_amount(c) >= ev->battery_capacity) continue;
                        }else{
                            if ( arrive_fwd - search_graph->get_bucket_charging_amount(c) <= 0) continue;
                        }
                        charging_decision.push_back(make_tuple(search_graph->get_edge_distance(prev->graph_id,c) + search_graph->get_edge_distance(c,next->graph_id),c,begin_timeslot,end_timeslot));
                    }
                    break;
                case PRICE_CHARGING:
                    for ( auto c : cs_nodes){
                        if(c == prev_charging_id  || c == next_charging_id) continue;

                        double arrive =  earliest_departure + search_graph->get_edge_time(prev->graph_id,c);
                        if(!search_graph->is_sd_charging_station(c)){
                            arrive = arrive <= query->working_time_start ? query->working_time_start : arrive;
                        }
                        double left = latest_arrive - search_graph->get_edge_time(c,next->graph_id);
                        if(!search_graph->is_sd_charging_station(c)){
                            left = left >= query->working_time_end? query->working_time_end : left;
                        }

                        int begin_timeslot = ceil((arrive - query->time_start)/time_bucket);
                        int end_timeslot = floor((left - query->time_start)/time_bucket) - 1;
                        if ( begin_timeslot > end_timeslot ) continue;
                        double arrive_fwd = current_solution[max_position-1].battery_fwd -
                                search_graph->get_edge_distance(current_solution[max_position-1].node->graph_id,c) * ev->driving_efficiency;

                        double arrive_bwd = current_solution[max_position].battery_bwd +
                                            search_graph->get_edge_distance(c,current_solution[max_position].node->graph_id) * ev->driving_efficiency;
                        // can not go to infeasible charging.
                        if(arrive_fwd  <  0) continue;

                        double price_sum = 0;
                        if(arrive_bwd +  ev->battery_capacity*charging_margin >= arrive_fwd ){
                            if ( arrive_fwd + search_graph->get_bucket_charging_amount(c) >= ev->battery_capacity) continue;
                            //charging
                            double highest_charging =search_graph->get_all_time_highest_charging_price();
                            for( int t = begin_timeslot; t <= end_timeslot; t ++){
                                price_sum += highest_charging - search_graph->get_bucket_charging_price(c,t);
                            }
                        }else{
                            if ( arrive_fwd - search_graph->get_bucket_charging_amount(c) <= 0) continue;
                            //discharging
                            for( int t = begin_timeslot; t <= end_timeslot; t ++){
                                price_sum += search_graph->get_bucket_discharging_price(c,t);
                            }
                        }
                        // maximize the profit, thus take negative value.
                        charging_decision.push_back(make_tuple(-price_sum,c,begin_timeslot,end_timeslot));
                    }
                    break;
            }
            if(!charging_decision.empty()){
                std::sort(charging_decision.begin(), charging_decision.end(),
                          [](const auto &lhs, const auto &rhs) {
                              return get<0>(lhs) < get<0>(rhs);
                          });
                double r = (double) rand() / (double) RAND_MAX;
                int selected_index = pow(r, degree_of_random) * charging_decision.size();
                selected_cs = get<1>(charging_decision[selected_index]);
                best_begin_timeslot = get<2>(charging_decision[selected_index]);
                best_end_timeslot = get<3>(charging_decision[selected_index]);
                break;
            }
        }
        // Select a position;

        if (selected_cs == -1 ) return  0;

        current_solution.insert(current_solution.begin() + (max_position - 1 )  + 1,
                                Decision{search_graph->get_node_ptr(selected_cs)});
        current_solution[max_position].charging_plan = vector<bool>( floor((query->time_end - query->time_start)/time_bucket), false);
        current_solution[max_position].discharging_plan = vector<bool>( floor((query->time_end - query->time_start)/time_bucket), false);
        order_visited[selected_cs]  = true;

        double arrive_fwd = current_solution[max_position - 1].battery_fwd -
                            search_graph->get_edge_distance(current_solution[max_position-1].node->graph_id,selected_cs) * ev->driving_efficiency;

        double arrive_bwd = current_solution[max_position + 1 ].battery_bwd +
                            search_graph->get_edge_distance(selected_cs,current_solution[max_position+1].node->graph_id) * ev->driving_efficiency;

        if(arrive_bwd +  ev->battery_capacity*charging_margin >= arrive_fwd){
            int full_charging =(ev->battery_capacity - arrive_fwd) / search_graph->get_bucket_charging_amount(selected_cs);
            int max_time_slot = min( full_charging,best_end_timeslot - best_begin_timeslot + 1 );
            double r = (double) rand() / (double) RAND_MAX;
            int decrease_value = pow(r, 2) * max_time_slot;
            int charging_slots =  max_time_slot - decrease_value;
//            int charging_slots =  random_integer(1,max_time_slot);
            current_solution[max_position].charging_begin = best_begin_timeslot;
            current_solution[max_position].charging_end = best_begin_timeslot + charging_slots - 1;
            if(current_solution[max_position].charging_end < 0 or current_solution[max_position].charging_end > 100){
                cout << "WTF" << endl;
            }
            if(current_solution[max_position].charging_begin < 0 or current_solution[max_position].charging_begin > 100){
                cout << "WTF" << endl;
            }
            for ( int t = current_solution[max_position].charging_begin;  t <= current_solution[max_position].charging_end ; t ++){
                current_solution[max_position].charging_plan[t] = true;
            }
        }else{
            int full_discharging = arrive_fwd / search_graph->get_bucket_charging_amount(selected_cs);
            int max_time_slot = min( full_discharging,best_end_timeslot - best_begin_timeslot + 1 );
            double r = (double) rand() / (double) RAND_MAX;
            int decrease_value = pow(r, 2) * max_time_slot;
            int charging_slots =  max_time_slot - decrease_value;
//            int charging_slots =  random_integer(1,max_time_slot);
            current_solution[max_position].charging_begin = best_begin_timeslot;
            current_solution[max_position].charging_end = best_begin_timeslot + charging_slots - 1;
            for ( int t = current_solution[max_position].charging_begin;  t <= current_solution[max_position].charging_end ; t ++){
                current_solution[max_position].discharging_plan[t] = true;
            }
        }

        update_EDLA(current_solution,(max_position - 1 ));
        update_battery_state(current_solution,(max_position - 1 ));
        charging_inserted ++;
        return 1;
    }

    void print_battery_state (vector<Decision>& current_solution){
        cout<<"Printing batter state"<<endl;
        for ( int i  =0; i < current_solution.size();i++){
            auto c =  current_solution [i];
            double charging_amount = 0;
            double discharging_amount = 0;
            if( c.node->type == Node_type::CS){
                for ( int t =c.charging_begin; t <=c.charging_end; t ++){
                    if (c.charging_plan[t]) {
                        charging_amount += search_graph->get_bucket_charging_amount(c.node->graph_id);
                    } else if (c.discharging_plan[t]) {
                        discharging_amount -= search_graph->get_bucket_charging_amount(c.node->graph_id);
                    }
                }
            }
//            if(i > 0){
//                cout<< "traveling cost: " << ev->driving_efficiency * search_graph->get_edge_distance(current_solution[i-1].node->graph_id, current_solution[i].node->graph_id ) <<endl;
//            }
//            cout << *c.node << " battery_state: "<<  c.battery_fwd << " "<<c.battery_bwd <<" charging : " << charging_amount
//            << " discharging "<< discharging_amount << endl;
        }
    }

     void repair_solution(vector<Decision>& current_solution) {
        charging_inserted = 0;
        std::fill(order_visited.begin(),order_visited.end(),false);
        for(auto& n : current_solution){
            if(n.node->type == Node_type::ORDER){
                order_visited[n.node->graph_id] = true;
            }
        }
        initialise_EDLA(current_solution);
        initialise_battery_state(current_solution);
        for(;;){
//            print_battery_state(current_solution);
            bool successful_inserted = true;
            if(current_solution[current_solution.size()-1].battery_fwd <  ev->battery_capacity*charging_margin){
                if(! insert_charging_decision(current_solution)){
                   if(! insert_order_decision(current_solution)){
                       successful_inserted = false;
                   }
                }
            } else {
                if ( ! insert_order_decision(current_solution) ){
                    if ( ! insert_charging_decision(current_solution) ) {
                        successful_inserted = false;
                    }
                }
            }
            if(!successful_inserted) break;
        }
         verify_tmp_solution(current_solution);
    }

    bool verify_tmp_solution(const vector<Decision>& current_solution){
        double arrive_time = query->time_start;
        arrive_time += search_graph->get_edge_time(current_solution[0].node->graph_id,current_solution[1].node->graph_id);
        for(int i = 1; i < current_solution.size(); i ++){
            if(current_solution[i].node->type == ORDER){
                arrive_time =  arrive_time < query->working_time_start ? query->working_time_start : arrive_time;
                if(arrive_time > current_solution[i].node->object.order->time_end|| arrive_time + current_solution[i].node->object.order->service_time> query->working_time_end){
                    cout<<"invalid tmp solution"<<endl;
                }
                arrive_time += current_solution[i].node->object.order->service_time + search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
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
                arrive_time = (current_solution[i].charging_end + 1)* time_bucket + query->time_start +
                              search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id);
            }else{
                if(arrive_time > query->time_end){
                    cout<<"exceed final time"<<endl;
                }
            }
        }
    }

    void adjust_weight(bool success, double old_sum_of_costs, double sum_of_costs, int changed_neighbour_size){
        if(success)
            // may needs to devided something
            repair_weights[selected_neighbor] =
                    reaction_factor * (old_sum_of_costs - sum_of_costs)/ changed_neighbour_size
                    + (1 - reaction_factor) * repair_weights[selected_neighbor];
        else
            repair_weights[selected_neighbor] =
                    (1 - decay_factor) * repair_weights[selected_neighbor];
    }

private:
    int time_bucket ;
    Graph* search_graph;
    Query* query;
    EV_setting* ev;

    int regret_k_value = 3;
    int continue_charging_decision_limit = 4;
    int charging_decision_limit = 8;
    //TODO charging_margin default is 0.15
    double charging_margin = 0.15;
    int charging_inserted = 0;

    //TODO dcay reaction factor default is 0.01
    double decay_factor = 0.01;
    double reaction_factor = 0.01;

    vector<double> repair_weights;
    int selected_neighbor;
    Greedy_Repair_strategy  repair_strategy;
    vector<bool> order_visited;
    Greedy_Charging_Repair_strategy charging_strategy;
    Greedy_Order_Repair_strategy order_strategy;
    //todo: set to 5 as default
    int degree_of_random = 5;
};