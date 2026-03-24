//
// Created by Bojie Shen on 20/9/2023.
//

#pragma once
#include <random>
#include "utils.h"

enum Repair_strategy { RANDOM_REP,MAX_PROFIT_REP, CLOSE_REP, REP_COUNT};

class Repairer {

    public:
        int screen = 0;

        Repairer() {repair_weights.assign(REP_COUNT,1);};

        void init(Graph* g, Query* q, EV_setting* e,int tb ){
            time_bucket = tb;
            search_graph = g;
            query = q;
            ev = e;
            node_priority.clear();
            for( int i= 1; i < g->get_number_of_nodes()-1; i ++){
                node_priority.push_back(g->get_node_ptr(i));
            }
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
                cout<< "Repair strategy: ";
                switch(repair_strategy){
                    case RANDOM_REP:
                        cout << "Random repair strategy" <<endl;
                    case MAX_PROFIT_REP:
                        cout << "Max profit repair strategy" <<endl;
                        break;
                    case CLOSE_REP:
                        cout << "Close repair strategy" <<endl;
                        break;
                }

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
                case 0 : repair_strategy = RANDOM_REP; break;
                case 1 : repair_strategy = MAX_PROFIT_REP; break;
                case 2 : repair_strategy = CLOSE_REP; break;
                default : cerr << "ERROR" << endl; exit(-1);
            }
        }

        vector<Decision> run_repairer(vector<Decision>& current_solution){
            chooseRepairHeuristicbyALNS();
//            return  repair_solution(current_solution);
            return repair_solution_all_pair_insert(current_solution);
        }

        double get_previous_time_bucket(double current_time){
            double finished_time = (floor(( current_time - query->time_start)/time_bucket) - 1) * (time_bucket) ;
            assert(finished_time > 0);
            return finished_time + query->time_start;
        }

        double get_next_time_bucket(double current_time){
            double finished_time = (ceil(( current_time - query->time_start)/time_bucket) + 1) * (time_bucket) ;
            return finished_time + query->time_start;
        }

        double get_next_n_time_bucket(double current_time, int num_of_time_slot){
            double finished_time = (ceil(( current_time - query->time_start)/time_bucket) + num_of_time_slot) * (time_bucket) ;
            return finished_time + query->time_start;
        }

        int get_num_time_bucket_in_between(double begin_time, double end_time ){
            int begin_time_bucket = ceil(( begin_time - query->time_start)/time_bucket);
            int finished_time_bucket = floor(( end_time - query->time_start)/time_bucket);
            return max(finished_time_bucket-begin_time_bucket,0);
        }

        double increase_service_time(Node* n, double arrive_time, int time_slot){
            if (n-> type == Node_type::ORDER){
                arrive_time = max(arrive_time,n->object.order->time_start);
                arrive_time += n->object.order->service_time;
            }else if(n -> type == Node_type::CS){
                arrive_time = get_next_n_time_bucket(arrive_time,time_slot);
            }
            return arrive_time;
        }

        double estimate_profit(Node* node, int begin_bucket, int end_bucket){
            if(node->type == Node_type::ORDER){
                return node->object.order->profit;
            }else if (node->type == Node_type::CS){
                double profit_d = 0;
                double profit_c = 0;
                for ( int t = begin_bucket; t < end_bucket; t ++){
                    profit_d += search_graph->get_bucket_discharging_price(node->graph_id, t);
                    profit_c += search_graph->get_max_charging_price(t) - search_graph->get_bucket_charging_price(node->graph_id, t);
                }
                return max(profit_c,profit_d);
            }
        }

        void update_prioirty(Node* curr_node, Node* next_node, double arrive_time, double latest_arrive ){
            int begin_bucket = ceil((arrive_time - query->time_start) /time_bucket);
            int end_bucket = floor((latest_arrive - query->time_start) / time_bucket);
            auto rng = std::default_random_engine {};
            switch (repair_strategy) {
                case RANDOM_REP:
                    if(curr_node->type == Node_type::SOURCE){
                        std::shuffle(std::begin(node_priority), std::end(node_priority), rng);
                    }
                    break;
                case MAX_PROFIT_REP:
                    sort(node_priority.begin(), node_priority.end(), [this, &begin_bucket, &end_bucket](const auto &lhs, const auto &rhs) {
                        return estimate_profit(lhs,begin_bucket,end_bucket) > estimate_profit(rhs,begin_bucket,end_bucket) ;
                    });
                    break;
                case CLOSE_REP:
                    sort(node_priority.begin(), node_priority.end(), [this, &curr_node, &next_node](const auto &lhs, const auto &rhs) {
                        double d1 =  search_graph->get_edge_distance(curr_node->graph_id,lhs->graph_id) +
                                search_graph->get_edge_distance(lhs->graph_id, next_node->graph_id);

                        double d2 =  search_graph->get_edge_distance(curr_node->graph_id,rhs->graph_id) +
                                     search_graph->get_edge_distance(rhs->graph_id, next_node->graph_id);

                        return d1 < d2 ;
                    });
            }

        }


        vector<Decision> repair_solution(vector<Decision>& current_solution){
            std::fill(order_visited.begin(),order_visited.end(),false);
            for(auto n : current_solution){
                if(n.node->type == Node_type::ORDER){
                    order_visited[n.node->graph_id] = true;
                }
            }

            double left = query->time_end;
            for (int i  = current_solution.size() - 1 ; i > 0 ; i --){
                if ( search_graph->is_order_node(current_solution[i].node->graph_id)){
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


            vector<Decision> refined_solution = vector<Decision>{Decision{search_graph->get_node_ptr(0)}};
            double arrive_time = search_graph->get_query_start_time();
            for (int i = 0; i < current_solution.size() - 1; i ++){
                if(current_solution[i].node->type != Node_type::CS){
                    int next_node_id = -1;
                    for( int j = i + 1 ; j < current_solution.size(); j ++){
                        if(current_solution[j].node->type != Node_type::CS){
                            next_node_id = j ;
                            break;
                        }
                    }
                    assert(next_node_id != -1);

                    Node* next_node = current_solution[next_node_id].node;
                    double latest_arrive = current_solution[next_node_id].latest_arrive;
                    update_prioirty(refined_solution.back().node,next_node,arrive_time,latest_arrive);
                    int charging_slot = 0;
                    bool copy_charing = false;
                    for ( int k = i + 1; k < next_node_id; k ++){
                        assert(current_solution[k].node->type == Node_type::CS);
                        // these must be charging.
                        arrive_time =  (current_solution[k].charging_end + 1)*time_bucket + query->time_start;
                        refined_solution.push_back(Decision{search_graph->get_node_ptr(current_solution[k].node->graph_id)});
                        copy_charing = true;
                        refined_solution[refined_solution.size() - 1].charging_begin = current_solution[k].charging_begin;
                        refined_solution[refined_solution.size() - 1].charging_end = current_solution[k].charging_end;
                    }
                    // insert new action;
                    int curr_size = refined_solution.size();
                    for ( auto n : node_priority){
                        double tmp_arrive = 0;
                        if(refined_solution.size() != curr_size ||  !copy_charing){
                            tmp_arrive  = increase_service_time(refined_solution.back().node,arrive_time,charging_slot);
                        }else{
                            tmp_arrive = arrive_time;
                        }
                        if(n->type == Node_type::ORDER){
                            if(order_visited[n->graph_id]) continue;

                            tmp_arrive += search_graph->get_edge_time(refined_solution.back().node->graph_id,
                                                                                         n->graph_id);
                            if(tmp_arrive > n->object.order->time_end) continue;

                            if(max(n->object.order->time_start, tmp_arrive) +
                            n->object.order->service_time + search_graph->get_edge_time(n->graph_id,next_node->graph_id) > latest_arrive) continue;

                            arrive_time = tmp_arrive;
                            order_visited[n->graph_id] = true;
                            refined_solution.push_back(Decision{search_graph->get_node_ptr( n->graph_id)});
                        }else if ( n->type == Node_type ::CS){
                            if(refined_solution.back().node->graph_id == n ->graph_id)  continue;
                            tmp_arrive += search_graph->get_edge_time(refined_solution.back().node->graph_id,
                                                                      n->graph_id);
                            int tmp_charging_slot = 1 ;
                            if(random_charging) {
                                int maximal_charging = floor((latest_arrive - query->time_start) / time_bucket)
                                                       - ceil((arrive_time - query->time_start) / time_bucket);
                                if (maximal_charging < 1) continue;

                                int maximal_full_charge = ev->battery_capacity / n->object.cs->bucket_charging_amount;
                                tmp_charging_slot = random_integer(1,min(maximal_charging, maximal_full_charge));
                            }
                            if(get_next_n_time_bucket(tmp_arrive,tmp_charging_slot) +
                            search_graph->get_edge_time(n->graph_id,next_node->graph_id) > latest_arrive ) continue;

                            charging_slot = tmp_charging_slot;
                            arrive_time = tmp_arrive;
                            refined_solution.push_back(Decision{search_graph->get_node_ptr( n->graph_id)});
                            refined_solution[refined_solution.size()-1].charging_begin =ceil(( tmp_arrive - query->time_start)/time_bucket);
                            refined_solution[refined_solution.size()-1].charging_end = refined_solution[refined_solution.size()-1].charging_begin
                                    + tmp_charging_slot - 1;

                        }
                    }
                    if(refined_solution.size() != curr_size ||  !copy_charing) {
                        arrive_time = increase_service_time(refined_solution.back().node, arrive_time, charging_slot);
                    }
                    arrive_time += search_graph->get_edge_time(refined_solution.back().node->graph_id,
                                                              next_node->graph_id);
                    refined_solution.push_back(Decision{next_node});
                }

            }
            verify_tmp_solution(refined_solution);
            return refined_solution;

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




    double increase_service_time(Decision* n, double arrive_time){
        if (n->node-> type == Node_type::ORDER){
            arrive_time = max(arrive_time,n->node->object.order->time_start);
            arrive_time += n->node->object.order->service_time;
        }else if(n->node->type == Node_type::CS){
            arrive_time = ( n->charging_end + 1) * time_bucket + query->time_start;
        }
        return arrive_time;
    }


    vector<Decision> repair_solution_all_pair_insert(vector<Decision>& current_solution) {
        std::fill(order_visited.begin(), order_visited.end(), false);
        for (auto n: current_solution) {
            if (n.node->type == Node_type::ORDER) {
                order_visited[n.node->graph_id] = true;
            }
        }

        double left = query->time_end;
        for (int i = current_solution.size() - 1; i > 0; i--) {
            if (search_graph->is_order_node(current_solution[i].node->graph_id)) {
                left -= search_graph->get_service_time(current_solution[i].node->graph_id);
                left = min(left, search_graph->get_order_end_time(current_solution[i].node->graph_id));
                if (left < search_graph->get_order_start_time(current_solution[i].node->graph_id)) {
                    cout << "error, can not arrive early than service time begin. " << endl;
                }
            } else if (search_graph->is_charging_station_node(current_solution[i].node->graph_id)) {
                if (left < current_solution[i].charging_begin * time_bucket + query->time_start) {
                    cout << " error, can not arrive early than charging time. " << endl;
                }
                left = current_solution[i].charging_begin * time_bucket + query->time_start;
            }
            current_solution[i].latest_arrive = left;
            left -= search_graph->get_edge_time(current_solution[i - 1].node->graph_id,
                                                current_solution[i].node->graph_id);
        }


        vector<Decision> refined_solution = vector<Decision>{Decision{search_graph->get_node_ptr(0)}};
        double arrive_time = search_graph->get_query_start_time();
        for (int i = 0; i < current_solution.size() - 1; i++) {
            Node *next_node = current_solution[i + 1].node;
            double latest_arrive = current_solution[i + 1].latest_arrive;
            update_prioirty(refined_solution.back().node, next_node, arrive_time, latest_arrive);
            for (auto& n: node_priority) {
                double tmp_arrive = increase_service_time( &refined_solution.back(),arrive_time);
                if (n->type == Node_type::ORDER) {
                    if (order_visited[n->graph_id]) continue;
                    tmp_arrive += search_graph->get_edge_time(refined_solution.back().node->graph_id,
                                                              n->graph_id);
                    if (tmp_arrive > n->object.order->time_end) continue;

                    if (max(n->object.order->time_start, tmp_arrive) +
                        n->object.order->service_time + search_graph->get_edge_time(n->graph_id, next_node->graph_id) >
                        latest_arrive)continue;

                    arrive_time = tmp_arrive;
                    order_visited[n->graph_id] = true;
                    refined_solution.push_back(Decision{search_graph->get_node_ptr(n->graph_id)});
                } else if (n->type == Node_type::CS) {
                    if (refined_solution.back().node->graph_id == n->graph_id) continue;
                    tmp_arrive += search_graph->get_edge_time(refined_solution.back().node->graph_id,
                                                              n->graph_id);
                    int tmp_charging_slot = 1;
                    if (random_charging) {
                        int maximal_charging = floor((latest_arrive - query->time_start) / time_bucket)
                                               - ceil((arrive_time - query->time_start) / time_bucket);
                        if (maximal_charging < 1) continue;

                        int maximal_full_charge = ev->battery_capacity / n->object.cs->bucket_charging_amount;
                        tmp_charging_slot = random_integer(1, min(maximal_charging, maximal_full_charge));
                    }

                    if (get_next_n_time_bucket(tmp_arrive, tmp_charging_slot) +
                        search_graph->get_edge_time(n->graph_id, next_node->graph_id) > latest_arrive)
                        continue;

                    arrive_time = tmp_arrive;
                    refined_solution.push_back(Decision{search_graph->get_node_ptr(n->graph_id)});
                    refined_solution[refined_solution.size() - 1].charging_begin = ceil(
                            (tmp_arrive - query->time_start) / time_bucket);
                    refined_solution[refined_solution.size() - 1].charging_end =
                            refined_solution[refined_solution.size() - 1].charging_begin + tmp_charging_slot -1;
                }
            }
            arrive_time = increase_service_time(&refined_solution.back(), arrive_time);
            arrive_time += search_graph->get_edge_time(refined_solution.back().node->graph_id,
                                                       next_node->graph_id);
            refined_solution.push_back(Decision{next_node});
            if(current_solution[i + 1].node->type == CS){
                refined_solution[refined_solution.size()-1].charging_begin = current_solution[i + 1].charging_begin;
                refined_solution[refined_solution.size()-1].charging_end = current_solution[i + 1].charging_end;
            }
        }

        verify_tmp_solution(refined_solution);
        return refined_solution;
    }

    private:
        int time_bucket ;
        Graph* search_graph;
        Query* query;
        EV_setting* ev;
        vector<Node*> node_priority;
        vector<bool> order_visited;

        double decay_factor = 0.01;
        double reaction_factor = 0.01;

        vector<double> repair_weights;
        int selected_neighbor;
        vector<double> value;
        Repair_strategy  repair_strategy;

        bool random_charging = true;

};