//
// Created by Bojie Shen on 19/9/2023.
//
#pragma once
#include <random>
#include "utils.h"

enum Destroy_strategy {RANDOM, SHAW_REMOVAL, WORST_PROFIT, CHARGING_CLOSENESS,DESTORY_COUNT};
//enum Destroy_strategy {RANDOM, MIN_PROFIT_ORDER, MIN_WINDOW_ORDER, RANDOM_CHARGING, DESTORY_COUNT};



class Destructor{

public:
    int screen = 0;
    Destructor() {destroy_weights.assign(DESTORY_COUNT,1);};
    void init(Graph* g, Query* q, EV_setting* e,int tb ){
        time_bucket = tb;
        search_graph = g;
        query = q;
        ev = e;
    }


    void random_destroy(vector<Decision>& current_solution){
        neighbour_size  = 0;
        int max_destroy = floor(current_solution.size() * destroy_higher_threshold);
        int min_destroy = floor(current_solution.size() * destroy_lower_threshold);
        if( current_solution.size() > 2  && min_destroy > 0 ){
            neighbour_size = random_integer(min_destroy, max_destroy);
            for( int i = 0; i < neighbour_size ; i ++){
                int index = random_integer(1, current_solution.size()-2);
                current_solution.erase(current_solution.begin() + index);
            }
        }
        assert(current_solution.front().node->type == Node_type::SOURCE);
        assert(current_solution.back().node->type == Node_type::DESTINATION);
    }


    void min_order_destroy(vector<Decision>& current_solution){
        neighbour_size  = 0;
        vector<Node*> order = vector<Node*>();
        for(auto& n : current_solution){
            if(n.node->type == Node_type::ORDER){
                order.push_back(n.node);
            }
        }
        int max_destroy = floor(order.size() * destroy_higher_threshold);
        int min_destroy = floor(order.size()  * destroy_lower_threshold);
        if( min_destroy > 0 ){
            neighbour_size = random_integer(min_destroy, max_destroy);
            sort(order.begin(), order.end(), [](const auto &lhs, const auto &rhs) {
                return lhs->object.order->profit < rhs->object.order->profit;
            });
            order.erase(order.begin() + neighbour_size, order.end());
        }
        for (auto& o : order){
            current_solution.erase(std::remove_if(current_solution.begin(), current_solution.end(),
                                               [&o](auto& element){ return element.node == o;}  ),
                                   current_solution.end());
        }
        assert(current_solution.front().node->type == Node_type::SOURCE);
        assert(current_solution.back().node->type == Node_type::DESTINATION);
    }

    void min_window_destroy(vector<Decision>& current_solution){
        neighbour_size  = 0;
        vector<Node*> order = vector<Node*>();
        for(auto& n : current_solution){
            if(n.node->type == Node_type::ORDER){
                order.push_back(n.node);
            }
        }
        int max_destroy = floor(order.size() * destroy_higher_threshold);
        int min_destroy = floor(order.size()  * destroy_lower_threshold);
        if( min_destroy > 0 ){
            neighbour_size = random_integer(min_destroy, max_destroy);
            sort(order.begin(), order.end(), [](const auto &lhs, const auto &rhs) {
                double lhs_window_size = lhs->object.order->time_end - lhs->object.order->time_start;
                double rhs_window_size = rhs->object.order->time_end - rhs->object.order->time_start;
                return lhs_window_size < rhs_window_size;
            });
            order.erase(order.begin() + neighbour_size, order.end());
        }
        for (auto& o : order){
            current_solution.erase(std::remove_if(current_solution.begin(), current_solution.end(),
                                                  [&o](auto& element){ return element.node == o;}  ),
                                   current_solution.end());
        }
        assert(current_solution.front().node->type == Node_type::SOURCE);
        assert(current_solution.back().node->type == Node_type::DESTINATION);
    }






    void random_charging(vector<Decision>& current_solution){
        neighbour_size = 0;
        vector<Node*> charging = vector<Node*>();
        for(auto& n : current_solution){
            if(n.node->type == Node_type::CS){
                charging.push_back(n.node);
            }
        }
        int max_destroy = floor(charging.size() * destroy_higher_threshold);
        int min_destroy = floor(charging.size()  * destroy_lower_threshold);
        if( min_destroy > 0 ){
            neighbour_size = random_integer(min_destroy, max_destroy);
            auto rng = std::default_random_engine{};
            std::shuffle(std::begin(charging), std::end(charging), rng);
            charging.erase(charging.begin() + neighbour_size, charging.end());
        }
        for (auto& c : charging){
            current_solution.erase(std::remove_if(current_solution.begin(), current_solution.end(),
                                                  [&c](auto& element){ return element.node == c;}  ),
                                   current_solution.end());
        }
        assert(current_solution.front().node->type == Node_type::SOURCE);
        assert(current_solution.back().node->type == Node_type::DESTINATION);
    }




    double get_begin_time(const Decision& d1){
        if(d1.node->type == Node_type::CS){
            return d1.charging_begin * time_bucket + query->time_start;
        }else if(d1.node->type == Node_type::ORDER){
            return d1.node->object.order->time_start;
        }
        return 0;
    }
    double get_end_time(const Decision& d1){
        if(d1.node->type == Node_type::CS){
            return (d1.charging_end + 1 ) * time_bucket + query->time_start;
        }else if(d1.node->type == Node_type::ORDER){
            return d1.node->object.order->time_end;
        }
        return 0;
    }
    double compute_relateness(const Decision& d1, const Decision& d2){
       double accumulate_time = search_graph->get_accumulate_time(d1.node->graph_id, d2.node->graph_id);
       double time_window_difference = abs(get_begin_time(d1) - get_begin_time(d2)) + abs(get_end_time(d1) - get_end_time(d2));
       return accumulate_time + time_window_difference;
    }


    void shaw_removal(vector<Decision>& current_solution){
        if(current_solution.size() > 2){
            int remove_index = random_integer(1,current_solution.size()-1);
            vector<Decision> remove_list = {current_solution[remove_index]};
            vector<Decision*> candidate_list;
            for( int i = 1; i < current_solution.size()-1;i++){
                if(i != remove_index){
                    candidate_list.push_back(&current_solution[i]);
                }
            }
            int max_destroy = floor(candidate_list.size() * destroy_higher_threshold);
            int min_destroy = floor(candidate_list.size()  * destroy_lower_threshold);
            neighbour_size = random_integer(min_destroy, max_destroy);
            for( int i = 0; i < neighbour_size; i++){
                const Decision& selected = remove_list[(rand()%remove_list.size())];
                sort(candidate_list.begin(), candidate_list.end(), [&selected, this](const auto &lhs, const auto &rhs) {
                    double lhs_value= compute_relateness(selected,*lhs);
                    double rhs_value= compute_relateness(selected,*rhs);
                    return lhs_value < rhs_value;
                });
                double r = (double)rand() / (double)RAND_MAX;
                remove_index = pow(r,degree_of_random) * candidate_list.size();
                remove_list.push_back(*candidate_list[remove_index]);
                candidate_list.erase(candidate_list.begin() + remove_index);
            }

            for (auto& r : remove_list){
                current_solution.erase(std::remove(current_solution.begin(), current_solution.end(),
                                                      r), current_solution.end());
            }
        }
    }

    double compute_profit(const Decision& d1){
        if(d1.node->type== Node_type::ORDER){
            return d1.node->object.order->profit;
        }else if (d1.node->type == Node_type::CS){
            double profit = 0;
            for(int t = d1.charging_begin; t <= d1.charging_end; t++){
                if(d1.charging_plan[t]) {
                    profit += search_graph->get_all_discharging_price(t);
                    profit -= search_graph->get_bucket_charging_price(d1.node->graph_id, t);
                }
                if(d1.discharging_plan[t]){
                    profit += search_graph->get_bucket_discharging_price(d1.node->graph_id,t);
                }
            }
            return profit;
        }
        return 0;
    }


    void worst_profit(vector<Decision>& current_solution){
        if(current_solution.size() > 2){
            int remove_index = random_integer(1,current_solution.size()-1);
            // have to copy the removed element
            vector<Decision> remove_list = {current_solution[remove_index]};

            vector<Decision*> candidate_list;
            for( int i = 1; i < current_solution.size()-1;i++){
                if(i != remove_index){
                    candidate_list.push_back(&current_solution[i]);
                }
            }
            int max_destroy = floor(candidate_list.size() * destroy_higher_threshold);
            int min_destroy = floor(candidate_list.size()  * destroy_lower_threshold);
            neighbour_size = random_integer(min_destroy, max_destroy);
            for( int i = 0; i < neighbour_size; i++){
                sort(candidate_list.begin(), candidate_list.end(), [this](const auto &lhs, const auto &rhs) {
                    double lhs_value = compute_profit(*lhs);
                    double rhs_value = compute_profit(*rhs);
                    return lhs_value < rhs_value;
                });
                double r = (double)rand() / (double)RAND_MAX;
                remove_index = pow(r,degree_of_random) * candidate_list.size();
                remove_list.push_back(*candidate_list[remove_index]);
                candidate_list.erase(candidate_list.begin() + remove_index);
            }

            for (auto& r : remove_list){
                current_solution.erase(std::remove(current_solution.begin(), current_solution.end(),
                                                   r  ),
                                       current_solution.end());
            }
        }
    }


    double compute_distance(const Decision& c1, const Decision& d2){
        auto c1_dep = search_graph->get_departure_location(*c1.node);
        auto d2_dep = search_graph->get_departure_location(*d2.node);
        auto d2_arr = search_graph->get_arrival_location(*d2.node);
        double dist = haversineDistance(get<0>(c1_dep),
                            get<1>(c1_dep),
                            get<0>(d2_dep),
                            get<1>(d2_dep)
        );
        dist += haversineDistance(get<0>(c1_dep),
                                  get<1>(c1_dep),
                                  get<0>(d2_arr),
                                  get<1>(d2_arr)
        );
        return dist;

    }

    void charging_closeness(vector<Decision>& current_solution){
//        CHARGING_CLOSENESS
        if(current_solution.size() > 2){
            vector<Decision*> candidate_list;
            vector<Decision*> charging_list;
            vector<Decision> remove_list;
            for( int i = 1; i < current_solution.size()-1; i++ ) {
                candidate_list.push_back(&current_solution[i]);
                if(current_solution[i].node->type == Node_type::CS){
                    charging_list.push_back(&current_solution[i]);
                }
            }
            if(charging_list.empty()) return;

            int max_destroy = floor(candidate_list.size() * destroy_higher_threshold);
            int min_destroy = floor(candidate_list.size() * destroy_lower_threshold);
            neighbour_size = random_integer(min_destroy, max_destroy);
            int element_removed  = 0;
            while (element_removed != neighbour_size) {
                Decision* charging_station = charging_list[rand()%charging_list.size()];
                candidate_list.erase(std::remove(candidate_list.begin(), candidate_list.end(),
                                                 charging_station ),
                                     candidate_list.end());
                charging_list.erase(std::remove(charging_list.begin(), charging_list.end(),
                                                 charging_station ),
                                    charging_list.end());
                remove_list.push_back(*charging_station);
                element_removed++;
                if(element_removed == neighbour_size){
                    break;
                }

                sort(candidate_list.begin(), candidate_list.end(), [this, &charging_station](const auto &lhs, const auto &rhs) {
                    double lhs_value = compute_distance(*charging_station,*lhs);
                    double rhs_value = compute_distance(*charging_station,*rhs);
                    return lhs_value < rhs_value;
                });
                int remove_size = 0;
                if(charging_list.empty()){
                    remove_size = (neighbour_size - element_removed);
                }else{
                    remove_size = random_integer(1, (neighbour_size - element_removed));
                }


                for( int i  = 0 ; i < remove_size; i ++){
                    double r = (double)rand() / (double)RAND_MAX;
                    int remove_index = pow(r,degree_of_random) * candidate_list.size();
                    remove_list.push_back(*candidate_list[remove_index]);
                    candidate_list.erase(candidate_list.begin() + remove_index);
                    element_removed++;
                }

            }

            for (auto& r : remove_list){
                current_solution.erase(std::remove(current_solution.begin(), current_solution.end(),
                                                   r  ),
                                       current_solution.end());
            }
        }
    }



    void single_charging_closeness(vector<Decision>& current_solution){
//        CHARGING_CLOSENESS
        if(current_solution.size() > 2){
            vector<Decision*> candidate_list;
            vector<Decision*> charging_list;
            vector<Decision> remove_list;
            for( int i = 1; i < current_solution.size()-1; i++ ) {
                candidate_list.push_back(&current_solution[i]);
                if(current_solution[i].node->type == Node_type::CS){
                    charging_list.push_back(&current_solution[i]);
                }
            }
            if(charging_list.empty()) return;

            int max_destroy = floor(candidate_list.size() * destroy_higher_threshold);
            int min_destroy = floor(candidate_list.size() * destroy_lower_threshold);
            neighbour_size = random_integer(min_destroy, max_destroy);
            Decision* charging_station = charging_list[rand()%charging_list.size()];
            candidate_list.erase(std::remove(candidate_list.begin(), candidate_list.end(),
                                             charging_station ),
                                 candidate_list.end());
            charging_list.erase(std::remove(charging_list.begin(), charging_list.end(),
                                            charging_station ),
                                charging_list.end());
            remove_list.push_back(*charging_station);

            for ( int i = 0; i< neighbour_size - 1; i ++){
                sort(candidate_list.begin(), candidate_list.end(), [this, &charging_station](const auto &lhs, const auto &rhs) {
                    double lhs_value = compute_distance(*charging_station,*lhs);
                    double rhs_value = compute_distance(*charging_station,*rhs);
                    return lhs_value < rhs_value;
                });
                double r = (double)rand() / (double)RAND_MAX;
                int remove_index = pow(r,degree_of_random) * candidate_list.size();
                remove_list.push_back(*candidate_list[remove_index]);
                candidate_list.erase(candidate_list.begin() + remove_index);
            }
            for (auto& r : remove_list){
                current_solution.erase(std::remove(current_solution.begin(), current_solution.end(),
                                                   r  ),
                                       current_solution.end());
            }
        }
    }
    void run_destructor(vector<Decision>& current_solution){
        chooseDestroyHeuristicbyALNS();
        switch(destroy_strategy){
            case RANDOM:
                random_destroy(current_solution);
                break;
            case SHAW_REMOVAL:
                shaw_removal(current_solution);
                break;
            case WORST_PROFIT:
                worst_profit(current_solution);
                break;
            case CHARGING_CLOSENESS:
                single_charging_closeness(current_solution);
                break;
        }
    }

    void chooseDestroyHeuristicbyALNS()
    {
        rouletteWheel();
        switch (selected_neighbor)
        {
            case 0 : destroy_strategy = RANDOM; break;
            case 1 : destroy_strategy = SHAW_REMOVAL; break;
            case 2 : destroy_strategy = WORST_PROFIT; break;
            case 3 : destroy_strategy = CHARGING_CLOSENESS; break;
            default : cerr << "ERROR" << endl; exit(-1);
        }
        if (screen >= 2)
        {
            cout<< "Destroy strategy: ";
            switch(destroy_strategy){
                case RANDOM:
                    cout << "Random strategy" <<endl;
                    break;
                case SHAW_REMOVAL:
                    cout << "Shaw removal strategy" <<endl;
                    break;
                case WORST_PROFIT:
                    cout << "Worst profit strategy" <<endl;
                    break;
                case CHARGING_CLOSENESS:
                    cout << "Charging closeness strategy" <<endl;
                    break;
            }
        }
    }

    void rouletteWheel()
    {
        double sum = 0;
        for (const auto& h : destroy_weights)
            sum += h;
        if (screen >= 2)
        {
            cout << "destroy weights = ";
            for (const auto& h : destroy_weights)
                cout << h / sum << ",";
        }
        double r = (double) rand() / RAND_MAX;
        double threshold = destroy_weights[0];
        selected_neighbor = 0;
        while (threshold < r * sum)
        {
            selected_neighbor++;
            threshold += destroy_weights[selected_neighbor];
        }
    }

    int get_neighbour_size(){
        return neighbour_size;
    }


    void adjust_weight(bool success, double old_sum_of_costs, double sum_of_costs, int changed_neighbour_size){
        if(success)
            // may needs to devided something
            destroy_weights[selected_neighbor] =
                    reaction_factor * (old_sum_of_costs - sum_of_costs)/ changed_neighbour_size
                    + (1 - reaction_factor) * destroy_weights[selected_neighbor];
        else
            destroy_weights[selected_neighbor] =
                    (1 - decay_factor) * destroy_weights[selected_neighbor];
    }


private:
    Graph* search_graph;
    Query* query;
    EV_setting* ev;
    int time_bucket;
    double decay_factor = 0.01;
    double reaction_factor = 0.01;

    double destroy_lower_threshold = 0.2;
    double destroy_higher_threshold = 0.6;

    vector<double> destroy_weights;
    int selected_neighbor;
    Destroy_strategy  destroy_strategy;
    int neighbour_size = 0;

    int degree_of_random = 5;
};


