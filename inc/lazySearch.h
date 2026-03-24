//
// Created by Bojie Shen on 5/9/2023.
//

#pragma once
#include "graph.h"
#include <queue>
#include "cpool.h"
#include "evaluator.h"
#include "set"
struct SearchNode {
    unsigned int search_node_id;
    double f_value;
    double g_value;
    double h_value;
    double maximal_battery;
    double EAT;
    int node_id;
    int charging_station_visit;
    SearchNode* parent_node;
    vector<int> order_visit;
    bool operator<(const SearchNode& other) const {
        return f_value < other.f_value;
    }
};


class LazySearch {
    template<typename T, typename Compare = std::less<T> >
    struct PointerComp
    {
        bool operator()(const T* x,
                        const T* y) const
        {
            return Compare()(*x, *y);
        }
    };

    typedef std::priority_queue<SearchNode*, std::vector<SearchNode*>,
            PointerComp<SearchNode> > pq;
    public:


        LazySearch() {
            node_pool = new warthog::mem::cpool(sizeof(SearchNode));
        }
        ~LazySearch()
        {
            if (node_pool)
            {
                delete node_pool;
            }
        }

        void init_search(Graph* g, Query* q, EV_setting* e){
            search_graph = g;
            query = q;
            ev = e ;
            evaluator.init(g,q,e,time_bucket);
            open_list = pq();
            node_pool->reclaim();
            curr_best_results = 0;
            best_results = nullptr;
            node_generated = 0;
        }

        double compute_maximal_energy( double earliest_dep, double lastest_arrive, int dep_id, int arr_id,
                                      vector<int>& charging_stations, int max_charging_station, double max_battery){
            int current_id = dep_id;
            for(int i  = 0 ; i <= max_charging_station; i++){
                // from succ to max_charging_station;
                earliest_dep = earliest_dep + search_graph->get_edge_time(current_id,charging_stations[i]);
                max_battery -= ev->driving_efficiency *
                               search_graph->get_edge_distance(current_id,charging_stations[i]);

                // charge for one ;
                if( i != max_charging_station) {
                    earliest_dep = get_next_time_bucket(earliest_dep);
                    max_battery += search_graph->get_node_ptr(
                            charging_stations[i])->object.cs->bucket_charging_amount;
                }
                current_id = charging_stations[i];
            }
            current_id = arr_id;
            for(int i  = charging_stations.size() -1 ; i >= max_charging_station; i--){
                // from succ to max_charging_station;
                lastest_arrive -= search_graph->get_edge_time(current_id,charging_stations[i]);
                max_battery -= ev->driving_efficiency *
                               search_graph->get_edge_distance(current_id,charging_stations[i]);

                // charge for one ;
                if( i != max_charging_station) {
                    lastest_arrive = get_previous_time_bucket(lastest_arrive);
                    max_battery += search_graph->get_node_ptr(
                            charging_stations[i])->object.cs->bucket_charging_amount;
                }
                current_id = charging_stations[i];
            }
            max_battery += get_num_time_bucket(earliest_dep,lastest_arrive)
                    * search_graph->get_node_ptr(charging_stations[max_charging_station])
                    ->object.cs->bucket_charging_amount;
            if(max_battery > ev->battery_capacity){
                max_battery = ev->battery_capacity;
            }
            return  max_battery;
        }



        std::tuple<bool, double> validate_battery_constraint (SearchNode* curr, int succ){
            Node* succ_p = search_graph->get_node_ptr(succ);
            if(curr->maximal_battery - ev->driving_efficiency *
                                                    search_graph->get_edge_distance(curr->node_id,
                                                                                    succ) - search_graph->get_service_energy(succ) < 0 )
            {
                return std::make_tuple(false, 0);
            }
//            cout <<" passing the first validation"<<endl;
            // compute max energy to succ;
            SearchNode *previous = curr;
            int index = 0;
            int max_charging_station = 0;
            double max_bucket_charging = succ_p->type == Node_type::CS ? succ_p->object.cs->bucket_charging_amount : 0  ;
            vector<int> charging_stations = succ_p->type == Node_type::CS ? vector<int>(succ) : vector<int>();
            while (search_graph->get_node_ptr(previous->node_id)->type  == Node_type::CS) {
                if(max_bucket_charging < search_graph->get_node_ptr(previous->node_id)->object.cs->bucket_charging_amount){
                    max_bucket_charging = search_graph->get_node_ptr(previous->node_id)->object.cs->bucket_charging_amount;
                    max_charging_station = index;
                }
                charging_stations.push_back(previous->node_id);
                previous = previous->parent_node;
                index++;
            }
            double max_battery = previous->maximal_battery;
            if(charging_stations.empty()){
                max_battery -= ev->driving_efficiency *
                                                          search_graph->get_edge_distance(
                                                                  previous->node_id, succ)
                              + search_graph->get_service_energy(succ);
            }else{
                reverse(charging_stations.begin(), charging_stations.end());
                double earliest_arrive = previous->EAT;
                double lastest_dep = succ_p->type == Node_type::ORDER ?
                                      search_graph->get_order_end_time(succ) : search_graph->get_query_end_time();
                max_battery = compute_maximal_energy(earliest_arrive, lastest_dep,
                                                               previous->node_id, succ,
                                                               charging_stations, max_charging_station,max_battery);
                max_battery -= search_graph->get_service_energy(succ);
            }

            if(max_battery < 0){
                return std::make_tuple(false, 0);
            }else{
                if ( succ_p->type != Node_type::DESTINATION ){
                    if (max_battery - ev->driving_efficiency *
                                              search_graph->get_edge_distance(succ,
                                                                              search_graph->get_d_nodes()) <
                        query->battery_end) {
                        // no enough energy to reach destination
                        bool reachable = false;
                        for (auto c: search_graph->get_cs_nodes()) {
                            if (max_battery - ev->driving_efficiency *
                                                      search_graph->get_edge_distance(succ, c) > 0) {
                                reachable = true;
                            }
                        }
                        if (!reachable) return std::make_tuple(false, 0);
                    }
                }

                return std::make_tuple(true, max_battery);
            }
        }

        int get_num_time_bucket(double current_time, double finished_time){
            return floor(( finished_time - query->time_start)/time_bucket) - ceil(( current_time - query->time_start)/time_bucket);
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

        int get_num_time_bucket_in_between(double begin_time, double end_time ){
            int begin_time_bucket = ceil(( begin_time - query->time_start)/time_bucket);
            int finished_time_bucket = floor(( end_time - query->time_start)/time_bucket);
            return max(finished_time_bucket-begin_time_bucket,0);
        }

        void generate_destination_node(SearchNode* curr, Node* curr_p, int succ){
            double arrival_time = search_graph->get_edge_time(curr->node_id, succ);
            if ( curr_p->type == Node_type::CS){
                // at least charging one time_bucket.
                arrival_time += get_next_time_bucket(curr->EAT);
            } else if (curr_p->type == Node_type::ORDER){
                arrival_time += max(curr->EAT,curr_p->object.order->time_start) + curr_p->object.order->service_time;
            } else{
                arrival_time += curr->EAT;
            }
            if(arrival_time > query->time_end ){
                return;
            }
            auto result = validate_battery_constraint (curr, succ);
            if(! get<0>(result)){
                return;
            }
            auto* successor = new (node_pool->allocate())
                    SearchNode{node_generated,0, curr->g_value,0, get<1>(result) ,
                            search_graph->get_query_end_time(),succ,0,
                               curr};
//            vector<int> solution = vector<int>();
//            SearchNode* curr_node = successor;
//            while (curr_node != nullptr) {
//                solution.push_back(curr_node->node_id);
//                curr_node = curr_node->parent_node;
//            }
//
//            vector<int> check_solution = {0,18,19,26,5,9,10,33,27,16,25,1,4,2,6,8,13,15,11,3,21,28,34};
//            bool found = true;
//            for(int i = 0 ; i < check_solution.size(); i ++){
//                if(check_solution[i] != solution[check_solution.size()-1 -i]){
//                    found = false;
//                }
//            }
//            if(found){
//                cout<<" I have found !!!!!!!!!!!!!!!!!!!!!" <<endl;
//                cout<<" I have found !!!!!!!!!!!!!!!!!!!!!" <<endl;
//                cout<<" I have found !!!!!!!!!!!!!!!!!!!!!" <<endl;
//                cout<<" I have found !!!!!!!!!!!!!!!!!!!!!" <<endl;
//            }

//            if(successor -> g_value > 355){
//                terminate = true;
//            }
            for( auto i : curr->order_visit){
                successor->order_visit.push_back(i);
            }
            successor->h_value = evaluate_heuristic(successor);
            successor->f_value = successor->h_value + successor->g_value;
            node_generated ++;
            open_list.push(successor);
        }


        void generate_successor_node(SearchNode* curr, Node* curr_p, Node* succ_p, int succ){
            int visit_charging = curr->charging_station_visit;
            if  (succ_p->type == Node_type::CS){
                if(visit_charging + 1 > number_of_continuous_visit){
                    return;
                }
            }
            if  (succ_p->type == Node_type::ORDER) {
                visit_charging  =  0;
            }else{
                visit_charging  += 1;
            }

            double arrival_time = search_graph->get_edge_time(curr->node_id, succ);
            if (curr_p->type == Node_type::CS) {
                // at least charging one time_bucket.
                arrival_time += get_next_time_bucket(curr->EAT);
            } else if (curr_p->type == Node_type::ORDER){
                // order
                arrival_time += max(curr->EAT, curr_p->object.order->time_start) +
                                curr_p->object.order->service_time;
            }else{
                // source
                arrival_time += curr->EAT;
            }

            // prune based on travel time;
            if (curr_p->type == Node_type::ORDER) {
                // order
                if (arrival_time > succ_p->object.order->time_end) return;
                if (max(arrival_time, succ_p->object.order->time_start) + succ_p->object.order->service_time
                    + search_graph->get_edge_time(succ, search_graph->get_d_nodes()) > query->time_end) {
                    return;
                }
            } else {
                // charging station
                if (arrival_time + search_graph->get_edge_time(succ, search_graph->get_d_nodes()) >
                    query->time_end) {
                    return;
                }
            }

//            cout<<"Passing time constraint: "<< succ<<endl;
            auto result = validate_battery_constraint(curr, succ);
            if( !get<0>(result)){
                return;
            }else{
                auto *successor = new(node_pool->allocate())
                        SearchNode{node_generated, 0,curr->g_value,0, get<1>(result), arrival_time, succ,
                                   visit_charging, curr};
                node_generated ++;
                if (succ_p->type == Node_type::ORDER) {
                    successor->g_value += succ_p->object.order->profit;
                    successor->order_visit.push_back(succ);
                }
                successor->h_value = evaluate_heuristic(successor);
                successor->f_value = successor->h_value + successor->g_value;
                for( auto i : curr->order_visit){
                    successor->order_visit.push_back(i);
                }
                validate_successor(successor);
                open_list.push(successor);
            }
        }

        std::tuple<int, int> get_time_bucket_in_between(double begin_time, double end_time ){
            int begin_time_bucket = ceil((begin_time - query->time_start) / time_bucket);
            int finished_time_bucket = floor((end_time - query->time_start) / time_bucket);
            return std::make_tuple(begin_time_bucket,finished_time_bucket);
        }


        double evaluate_heuristic(SearchNode* successor){
            double h_value = 0;
            if( successor->node_id != search_graph->get_d_nodes()) {
                double undecided_start = successor->EAT;
                double undecided_end = query->time_end;
                vector<int> order_nodes = search_graph->get_order_nodes();
                for (auto on: order_nodes) {
                    if (undecided_start < search_graph->get_order_start_time(on) &&
                        search_graph->get_order_end_time(on) < undecided_end) {
                        h_value += search_graph->get_profit(on);
                    }
                }
                auto time_period = get_time_bucket_in_between(undecided_start, undecided_end);
                if (get<0>(time_period) < get<1>(time_period)) {
                    for (int i = get<0>(time_period); i <= get<1>(time_period); i++) {
                        h_value += search_graph->get_all_discharging_price(i);
                    }
                }
            }
            SearchNode *pre_non_charging_node = successor;
            SearchNode *curr_node = successor;
            vector<int> charging_station_between = vector<int>();

            while (curr_node != nullptr) {
                if (search_graph->is_charging_station_node(curr_node->node_id)) {
                    charging_station_between.push_back(curr_node->node_id);
                } else {
                    if (charging_station_between.empty()) {
                        pre_non_charging_node = curr_node;
                    } else {
                        auto time = get_time_bucket_in_between(curr_node->EAT, pre_non_charging_node->EAT);
                        if (get<0>(time) < get<1>(time)) {
                            for (int t = get<0>(time); t <= get<1>(time); t++) {
                                double max_price = 0;
                                for (auto c: charging_station_between) {
                                    max_price = max(max_price, search_graph->get_bucket_discharging_price(c, t));
                                }
                                h_value += max_price;
                            }
                        }
                        pre_non_charging_node = curr_node;
                        charging_station_between.clear();
                    }
                }
                curr_node = curr_node->parent_node;
            }
            return h_value;
        }

        void validate_successor(SearchNode* successor){
            vector<int> solution = vector<int>();
            SearchNode* curr_node = successor;
            while (curr_node != nullptr) {
                solution.push_back(curr_node->node_id);
                curr_node = curr_node->parent_node;
            }

            std::set<int> order_node;
            for (int i = 0; i < solution.size(); i++){
                if(search_graph->is_order_node(solution[i])){
                    if (order_node.find(solution[i]) != order_node.end()) {
                        cout<<"invalid successor "<<endl;
                    } else{
                        order_node.insert(solution[i]);
                    }
                }
            }
            for (int i = 0; i < solution.size()-1; i++){
                if(search_graph->is_charging_station_node(solution[i])  &&
                        search_graph->is_charging_station_node(solution[i+1])){
                    cout<<"invalid successor "<<endl;
                }
            }
        }


        void search(){
//            vector<int> plan2  = { 0, 18, 19, 26, 5, 9, 10, 33, 27, 16, 25, 1, 4, 2, 6, 8, 13, 15, 11, 3, 21, 28, 34};
//            vector<int> plan2  = { 31, 33,  1, 4, 2, 6, 8, 13, 15, 11, 3, 21, 28, 34};
//            double r1 = evaluator.simplified_evaluator(plan2);
//            double r2 = evaluator.evaluate_solution(plan2);
//            cout<< r1 << " " << r2<<endl;
////            validate_plan(plan);
//            return ;
            SearchNode* start = new (node_pool->allocate())
                    SearchNode{0,0,0,0,query->battery_start,
                               query->time_start,search_graph->get_s_nodes(),
                               0 ,nullptr, vector<int>()
            };
            node_generated++;
            open_list.push(start);
            int node_count = 0;
            while(!open_list.empty()){
                if(terminate){
                    break;
                }
                SearchNode* curr  = open_list.top(); open_list.pop();
                Node* curr_p = search_graph->get_node_ptr( curr->node_id );
                node_count++;
                if(node_count % 100 == 0){
                    cout<<"Expanding " << node_count <<" number of nodes."<<endl;
                    cout<<"Current best: "<< curr_best_results << endl;
                    cout<<"Current best f: "<< curr->f_value<< endl;
                }
                if(curr_p->type ==  Node_type::DESTINATION){
                    // reach destination;
                    vector<int> action_sequence = vector<int>();
                    SearchNode* prev = curr;
                    while( prev->parent_node != nullptr ) {
                        action_sequence.push_back(prev->node_id);
                        prev = prev->parent_node;
                    }
                    action_sequence.push_back(0);
                    reverse(action_sequence.begin(), action_sequence.end());
                    double increase = evaluator.evaluate_solution(action_sequence);
                    if(increase != -1){
                        curr->g_value += increase;
                        if(curr_best_results < curr->g_value ){
                            curr_best_results = curr->g_value;
                            best_results = curr;
                        }
                    }
                }else {
                    const vector<int> &successors = search_graph->get_successors();
                    for (int succ: successors) {
                        if (succ == curr->node_id) continue;
                        bool already_visited = false;
                        if (search_graph->is_order_node(succ)) {
                            for (int visited: curr->order_visit) {
                                // skip if already visited;
                                if (succ == visited){
                                    already_visited = true;
                                    break;
                                }
                            }
                        }
                        if(already_visited) continue;
                        Node *succ_p = search_graph->get_node_ptr(succ);
                        if (succ == search_graph->get_d_nodes()) {
                            // reach destination
                            generate_destination_node(curr, curr_p, succ);
                        } else {
                            generate_successor_node(curr, curr_p, succ_p, succ);
                        }
                    }
                }
            }
        }



    private:

        // priority queue sort in descending order.
        int time_bucket = 20;
        int number_of_continuous_visit = 1;
        unsigned node_generated;
        warthog::mem::cpool* node_pool;
        pq open_list;
        Graph* search_graph;
        Query* query;
        EV_setting* ev;
        Evaluator evaluator;
        double curr_best_results;
        bool terminate = false;
        SearchNode* best_results;
};