//
// Created by Bojie Shen on 14/9/2023.
//
#pragma once
#include "gurobi_c++.h"
#include <map>
int bigM = 100000;
class Evaluator {
public:

    Evaluator() {
        env.start();
        // set silent mode
        env.set(GRB_IntParam_OutputFlag, 0);
        env.set(GRB_IntParam_LogToConsole, 0);
    }

    void init(Graph* g, Query* q, EV_setting* e, int tb ){
        ev = e;
        time_bucket = tb;
        query = q;
        search_graph = g;
    }

    string itos(int i) {stringstream s; s << i; return s.str(); }


    void print_solution(const vector<int>&  action_sequence, const vector<int>& V_c_mapper, int number_of_time_bucket,
                        GRBVar *battery_vars, GRBVar *time_vars, GRBVar ** charging_vars,GRBVar ** discharging_vars ){
        for(int i = 0; i < action_sequence.size() ; i++ ){
            cout << "Node Id: " << i << ", Battery level: " << battery_vars[i].get(GRB_DoubleAttr_X) << ", Travel time: " << time_vars[i].get(GRB_DoubleAttr_X) << endl;
            if(search_graph->is_charging_station_node(action_sequence[i])) {
                for (int  t = 0; t < number_of_time_bucket; t++ ){
                    if(charging_vars[V_c_mapper[i]][t].get(GRB_DoubleAttr_X) > 0.5){
                        cout << "Charging: " << search_graph->get_query_start_time() + t*time_bucket <<" to " << search_graph->get_query_start_time() +(t+1)*time_bucket <<
                             " energy charged: "<< search_graph->get_bucket_charging_amount(action_sequence[i]) <<
                             " charging cost: "<< search_graph->get_bucket_charging_price(action_sequence[i],t) << std::endl;
                    }
                }
                cout<< std::endl;
                for (int  t = 0; t < number_of_time_bucket; t++ ){
                    if(discharging_vars[V_c_mapper[i]][t].get(GRB_DoubleAttr_X) > 0.5){
                        cout << "Discharging: " << search_graph->get_query_start_time() + t*time_bucket <<" to " << search_graph->get_query_start_time() + (t+1)*time_bucket<<
                             " energy discharged: "<< search_graph->get_bucket_charging_amount(action_sequence[i]) <<
                             " discharging cost: "<< search_graph->get_bucket_discharging_price(action_sequence[i],t) << std::endl;
                    }
                }
            }
        }
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

    std::tuple<int, int> get_time_bucket_in_between(double begin_time, double end_time ){
        int begin_time_bucket = ceil((begin_time - query->time_start) / time_bucket);
        int finished_time_bucket = floor((end_time - query->time_start) / time_bucket);
        return std::make_tuple(begin_time_bucket,finished_time_bucket);
    }

    double simplified_evaluator (vector<int>& action_sequence){
        // This simplified model should work, but need more testings.
        struct decision{
            int node_id;
            double arrive;
            double left;
            double time_to_next_charger;
        };
        vector<decision> decision_graph = vector<decision>(action_sequence.size());
        double arrive = query ->time_start;
        int preivous_charging_station  = -1;
        double travel_time = 0;
        for (int i  = 0 ; i < action_sequence.size() - 1 ; i ++){
            decision_graph[i].node_id = action_sequence[i];
            decision_graph[i].arrive = arrive;
            if ( search_graph->is_order_node(action_sequence[i])){
                arrive = max(arrive, search_graph->get_order_start_time(action_sequence[i]));
                if ( arrive > search_graph->get_order_end_time(action_sequence[i])){
                    cout<< "found arrive time error "<<endl;
                    return -1;
                }
                arrive += search_graph->get_service_time(action_sequence[i]);
                travel_time+= search_graph->get_service_time(action_sequence[i]);
            }else if (search_graph->is_charging_station_node(action_sequence[i])){
                if(preivous_charging_station != -1 ){
                    decision_graph[preivous_charging_station].time_to_next_charger = travel_time;
                }
                preivous_charging_station = i;
                travel_time = 0;
                arrive = get_next_time_bucket(arrive);
            }
            travel_time+= search_graph->get_edge_time(action_sequence[i], action_sequence[i+1]);
            arrive += search_graph->get_edge_time(action_sequence[i], action_sequence[i+1]);
        }
        decision_graph[action_sequence.size()-1].node_id = action_sequence[action_sequence.size()-1];
        decision_graph[action_sequence.size()-1].arrive = arrive;



        double left = query->time_end;
        for (int i  = action_sequence.size() - 1 ; i > 0 ; i --){
            decision_graph[i].left = left ;
            if ( search_graph->is_order_node(action_sequence[i])){
                left -= search_graph->get_service_time(action_sequence[i]);
                left = min(left, search_graph->get_order_end_time(action_sequence[i]));
                if ( left < search_graph->get_order_start_time(action_sequence[i])){
                    cout<< "found left time error "<<endl;
                    return -1;
                }
            }else if (search_graph->is_charging_station_node(action_sequence[i])){
                left = get_previous_time_bucket(left);
            }
            left -= search_graph->get_edge_time(action_sequence[i-1], action_sequence[i]);
        }
        decision_graph[0].left = left;
        // assume that action sequence only contains  o -> c -> o -> c -> o;

        int num_of_charger = 0;
        vector<int> V_c_mapper = vector<int>(action_sequence.size(),-1);
        vector<int> V_c;
        for (int i  = 0 ; i < action_sequence.size(); i ++){
            if ( search_graph->is_charging_station_node(action_sequence[i])){
                V_c_mapper[i] = num_of_charger;
                V_c.push_back(i);
                num_of_charger ++;
            }
        }

        if (num_of_charger == 0 ){
            return 0;
        }
        GRBModel model = GRBModel(env);
        int number_of_time_bucket = floor((query->time_end - query->time_start)/time_bucket);


        // create charging variable;
        GRBVar ** charging_vars = NULL;
        charging_vars = new GRBVar*[num_of_charger];
        for (int i = 0; i < num_of_charger; i++)
            charging_vars[i] = new GRBVar[number_of_time_bucket];
        for (int i = 0 ; i < num_of_charger; i++){
            for (int j = 0 ; j < number_of_time_bucket; j++) {
                charging_vars[i][j] = model.addVar(0.0, 1.0, 0,
                                                   GRB_BINARY, "c_"+itos(i)+"_"+itos(j));
            }
        }

        // create discharging variable;
        GRBVar ** discharging_vars = NULL;
        discharging_vars = new GRBVar*[num_of_charger];
        for (int i = 0; i < num_of_charger; i++)
            discharging_vars[i] = new GRBVar[number_of_time_bucket];
        for (int i = 0 ; i < num_of_charger; i++){
            for (int j = 0 ; j < number_of_time_bucket; j++) {
                discharging_vars[i][j] = model.addVar(0.0, 1.0, 0,
                                                      GRB_BINARY, "d_"+itos(i)+"_"+itos(j));
            }
        }

        for ( int i = 0; i < action_sequence.size(); i++){
            if(search_graph->is_charging_station_node(action_sequence[i])) {
                auto time = get_time_bucket_in_between(decision_graph[i].arrive,decision_graph[i].left);
                if(get<0>(time) < get<1>(time)){
                    for (int t = 0 ; t < get<0>(time); t++){
                        charging_vars[V_c_mapper[i]][t].set(GRB_DoubleAttr_UB, 0);
                        discharging_vars[V_c_mapper[i]][t].set(GRB_DoubleAttr_UB, 0);
                    }
                    for (int t = get<1>(time) ; t < number_of_time_bucket; t++){
                        charging_vars[V_c_mapper[i]][t].set(GRB_DoubleAttr_UB, 0);
                        discharging_vars[V_c_mapper[i]][t].set(GRB_DoubleAttr_UB, 0);
                    }
                }
            }
        }


        GRBVar *battery_vars = new GRBVar[action_sequence.size()];
        for ( int i = 0; i < action_sequence.size(); i++){
            battery_vars[i] = model.addVar(0, ev->battery_capacity, 0,
                                           GRB_CONTINUOUS, "b_"+itos(i));
        }
        // implementation of Eq:(11).
        battery_vars[0].set(GRB_DoubleAttr_UB, query->battery_start);
        battery_vars[0].set(GRB_DoubleAttr_LB, query->battery_start);
        // implementation of Eq:(12).
        model.addConstr(battery_vars[action_sequence.size()-1] >= query->battery_end, "destination_battery_constraint");



        for (int t = 0 ; t < number_of_time_bucket; t++) {
            GRBLinExpr exp = 0;
            for (int i = 0 ; i < num_of_charger; i++){
                exp += charging_vars[i][t] + discharging_vars[i][t];
            }
            model.addConstr(exp <= 1, "not_charging_discharging_"+itos(t));
        }

        for (int i = 0 ; i < num_of_charger; i++){
            GRBLinExpr exp = 0;
            for (int t = 0 ; t < number_of_time_bucket; t++) {
                exp += charging_vars[i][t] + discharging_vars[i][t];
            }
            model.addConstr(exp >=1, "must_charge_one_"+itos(i));
        }

        // make sure each traversal between edges has enough of battery.
        // implementation of Eq:(13).
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            if(search_graph->is_charging_station_node(action_sequence[i])) {

                GRBLinExpr lhs = battery_vars[i+1];
                GRBLinExpr rhs = battery_vars[i] - search_graph->get_edge_distance(action_sequence[i],action_sequence[i+1])* ev->driving_efficiency;
                for (int t = 0 ; t < number_of_time_bucket; t ++){
                    rhs += charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]);
                    rhs -= discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]);
                }
                model.addConstr(lhs  <= rhs, "cs_energy_consume_"+itos(i)+"_"+itos(i+1));

                GRBLinExpr charging_exp = 0;
                GRBLinExpr discharging_exp = 0;
                for (int t = 0 ; t < number_of_time_bucket; t ++){
                    charging_exp += charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]) ;
                    discharging_exp += discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]) ;
                }
                model.addConstr(charging_exp <= ev->battery_capacity - battery_vars[i], "maximal_charging_"+itos(i));
                model.addConstr(discharging_exp <= battery_vars[i], "maximal_discharging_"+itos(i));

            }else{
                GRBLinExpr lhs = battery_vars[i+1];
                GRBLinExpr rhs = battery_vars[i] - (search_graph->get_edge_distance(action_sequence[i],action_sequence[i+1])* ev->driving_efficiency +
                                                    search_graph->get_service_energy(action_sequence[i]));
                model.addConstr(lhs  <= rhs, "energy_consume_"+itos(i)+"_"+itos(i+1));

            }
        }
        GRBVar **bool_charging_vars =  new GRBVar*[num_of_charger];
        for (int i = 0; i < num_of_charger; i++)
            bool_charging_vars[i] = new GRBVar[number_of_time_bucket];
        for (int i = 0 ; i < num_of_charger; i++){
            for (int j = 0 ; j < number_of_time_bucket; j++) {
                bool_charging_vars[i][j] = model.addVar(0.0, 1.0, 0,
                                                        GRB_BINARY, "c_"+itos(i)+"_"+itos(j));
            }
        }

        if(num_of_charger > 1) {
            for(int i = 0; i < num_of_charger ; i++ ) {
                for (int t = 0; t < number_of_time_bucket; t++) {
                    model.addConstr(bool_charging_vars[i][t] == charging_vars[i][t] +
                                                                discharging_vars[i][t], "visited_charging" + itos(i));
                }
            }

            for (int i = 0; i < num_of_charger - 1; i ++){
                for (int t = 0; t < number_of_time_bucket; t++) {
                    int tt = ceil( (query->time_start + (t + 1) * time_bucket
                                    + decision_graph[V_c[i]].time_to_next_charger) / time_bucket);
                    tt = min(number_of_time_bucket,tt);
                    GRBLinExpr expr = 0;
                    for (int t2 = 0; t2 < tt; t2++) {
                        expr += charging_vars[i+1][t2] + discharging_vars[i+1][t2];
                    }
                    model.addGenConstrIndicator(bool_charging_vars[i][t],true, expr, GRB_EQUAL, 0);
                }
            }
        }
        // objective function:
        GRBLinExpr objective = 0;
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            if(search_graph->is_charging_station_node(action_sequence[i])) {
                for (int t = 0; t < number_of_time_bucket; t++) {
                    objective -= charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_price(action_sequence[i],t) ;
                    objective += discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_discharging_price(action_sequence[i],t);
                }
            }
        }

        model.setObjective(objective , GRB_MAXIMIZE);
        model.optimize();

        if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL) {
            return model.get(GRB_DoubleAttr_ObjVal);
        }else{
            return -1;
        }

    }

    // this function takes current solution, and decide whether the solution is feasible or not.
    // if it is feasible, then decide the optimal charging / discharging.
    double solve_optional_charging( const vector<Decision>& current_solution){
        //  Let's say we have a solution like this: S--------O1-------C1---------O2---------C2---------D
        //  This solver will first transfer the solution to this graph problem:
        //  S--------O1-------C1---------O2---------C2---------D
        //           |                   |                     |
        //           |-------------------|---------------------|
        // This basically says, although currently we decided to charge/discharge at C1 and C2, it is possible that
        // the optimal solution may doesn't need to go to C1 or C2.
        // I will try to carry this toy example in the rest of example.

        // V_c records the index of charging decision, in this case it will be <2,4>.
        // V_os records the index of source node and order node, in this case it will be <0,1,3>.
        // V_c_mapper maps the charging decision, to the decision variable of MIP model.
        // In this case, we only need to create two charging decision variable, so it maps 2 -> 0, 4 -> 1.
        vector<int> V_c = vector<int>();
        vector<int> V_os = vector<int>();
        V_c_mapper = vector<int>(current_solution.size(),-1);
        for (int i  = 0 ; i < current_solution.size(); i ++){
            if ( search_graph->is_charging_station_node(current_solution[i].node->graph_id)){
                V_c.push_back(i);
                V_c_mapper[i] = V_c.size()-1;
            }else if ( search_graph->is_order_node(current_solution[i].node->graph_id)) {
                V_os.push_back(i);
            }
        }
        V_os.push_back(0);


        // if there is no charging decision, we simply check whether the battery is sufficient to execute these order.
        V_c_size = V_c.size();
        if(V_c.empty()){
            double battery = query->battery_start;
            for ( int i = 0; i < current_solution.size() - 1 ; i++){
                battery -= search_graph->get_service_energy(current_solution[i].node->graph_id);
                battery -= search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id)*ev->driving_efficiency;
            }
            if(battery < query->battery_end ){
                return -1 ;
            }else {
                return 0;
            }
        }


        //  S--------O1-------C1---------O2---------C2---------D
        //           |                   |                     |
        //           |-------------------|---------------------|
        // solution graph records outgoing edges of each node. e.g., the outgoing node of S, is <1> (i.e., O1).
        // incoming_edge records incoming edges of each node. e.g., the incoming node of S, is < > (i.e., empty).
        solution_graph = vector<vector<int>>(current_solution.size());
        vector<vector<int>> incoming_edge = vector<vector<int>>(current_solution.size());
        num_of_edges = 0 ;
        for(int i = 0; i < solution_graph.size()-1; i++){
            int j = i + 1;
            // always add j ;
            solution_graph[i].push_back(j);
            incoming_edge[j].push_back(i);
            num_of_edges++;
            while(current_solution[j].node->type == Node_type::CS){
                // add skipping edge;
                solution_graph[i].push_back(j+1);
                incoming_edge[j+1].push_back(i);
                num_of_edges++;
                j++;
            }
        }

        // Initialise the model.
        GRB_model = new GRBModel(env);
        // Initialise edge decision variable, each of which is a boolean decision.
        // these edge decision variable indicated whether this edge is traversed or not (to skip charging decision).
        GRB_model->set(GRB_DoubleParam_MIPGap, 0.05);
        GRB_model->set(GRB_DoubleParam_TimeLimit, 5.0);  // Set to 5 seconds

        GRB_visit_vars = new GRBVar[num_of_edges];
        edge_mapper.clear();
        int index = 0;
        for(int i = 0; i < solution_graph.size()-1; i++){
            for(auto j : solution_graph[i]){
                GRB_visit_vars[index] = GRB_model->addVar(0.0, 1.0, 0,
                                                          GRB_BINARY, "visit_"+itos(i)+"_"+itos(j));
                edge_mapper[i* num_of_edges + j] = index;
                index++;
            }
        }





        GRB_time_vars = new GRBVar[current_solution.size()];
        GRB_battery_vars = new GRBVar[current_solution.size()];
        for ( int i = 0; i < current_solution.size(); i++){
            // Initialise travel time decision variable, each of which correspond to a node, indicating when the EV arrive
            // at this node.
            GRB_time_vars[i] = GRB_model->addVar( query->time_start,  query->time_end, 0,
                                                  GRB_CONTINUOUS, "t_"+itos(i));
            //Initial the battery decision variable, each of which correspond to the SOC of each node.
            GRB_battery_vars[i] = GRB_model->addVar(0, ev->battery_capacity, 0,
                                                    GRB_CONTINUOUS, "b_"+itos(i));
        }

        // set each travel time to be between start time and finished time.
        GRB_time_vars[0].set(GRB_DoubleAttr_UB, query->time_start);
        GRB_time_vars[0].set(GRB_DoubleAttr_LB,query->time_start);
        GRB_model->addConstr(GRB_time_vars[current_solution.size() - 1] <= query->time_end, "destination_trav_time");


        GRB_battery_vars[0].set(GRB_DoubleAttr_UB, query->battery_start);
        GRB_battery_vars[0].set(GRB_DoubleAttr_LB, query->battery_start);
        GRB_model->addConstr(GRB_battery_vars[current_solution.size()-1] >= query->battery_end, "destination_battery_constraint");


        // Compute the number of time bucket(N), each time bucket has 20 mins.
        // For each charging station, initialise charging decision variable.
        // each charging station contains N number of boolean decision, which decide whether
        // EV is charging at this timestamp or not. Similarly, for discharing.
        num_of_time_bucket = floor((query->time_end - query->time_start)/time_bucket);
        // create charging variable;
        GRB_charging_vars = new GRBVar*[V_c.size()];
        // create discharging variable;
        GRB_discharging_vars = new GRBVar*[V_c.size()];
        for (int i = 0 ; i < V_c.size(); i++){
            GRB_charging_vars[i] = new GRBVar[num_of_time_bucket];
            GRB_discharging_vars[i] = new GRBVar[num_of_time_bucket];
            for (int j = 0 ; j < num_of_time_bucket; j++) {
                GRB_charging_vars[i][j] = GRB_model->addVar(0.0, 1.0, 0,
                                                            GRB_BINARY, "c_"+itos(i)+"_"+itos(j));
                GRB_discharging_vars[i][j] = GRB_model->addVar(0.0, 1.0, 0,
                                                               GRB_BINARY, "d_"+itos(i)+"_"+itos(j));
            }
        }


        for(int i = 0; i < solution_graph.size()-1; i++){
            // Constraint 1 :  ensure that if a->b is visited, then b->a must also visited.
            // it does not stop at any place between S to D.
            if( i >= 1){
                GRBLinExpr expr = 0;
                for(auto j : solution_graph[i]){
                    expr += GRB_visit_vars[edge_mapper[i* num_of_edges + j]];
                }
                for(auto j : incoming_edge[i]){
                    expr -= GRB_visit_vars[edge_mapper[j* num_of_edges + i]];
                }
                GRB_model->addConstr(expr == 0, "continue_travel_"+itos(i));
            }
            // Constraint 2 : for the edge that only have one outing edge, it must be visited. e.g., S---->O1 must be visited
            if(solution_graph[i].size() == 1 && current_solution[i].node->type != CS ){
                GRB_model->addConstr(GRB_visit_vars[edge_mapper[i* num_of_edges +  solution_graph[i][0]]] == 1);
            }
            // Constraint 3: Make sure the travel time is satisify the order's time window.
            if(search_graph->is_order_node(current_solution[i].node->graph_id)) {
                GRB_model->addConstr(GRB_time_vars[i] <= search_graph->get_order_end_time(current_solution[i].node->graph_id),
                                     "order_time_window_start_" + itos(i));
                GRB_model->addConstr(GRB_time_vars[i] >= search_graph->get_order_start_time(current_solution[i].node->graph_id),
                                     "order_time_window_end_" + itos(i));
            }

            // Constraint 4: Big M notation, make sure if EV travel from a to b. the travel time at b must larger than
            // the travel time at a + service time at a (pickup and delivery time) + travel time (from a to b).
            for(auto j : solution_graph [i]){
                GRBLinExpr expr = GRB_time_vars[i] + (search_graph->get_service_time(current_solution[i].node->graph_id)
                                                      + search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[j].node->graph_id))
                                                     * GRB_visit_vars[edge_mapper[i * num_of_edges + j]] - bigM * (1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);
                GRB_model->addConstr(expr <= GRB_time_vars[j], "travel_time_"+itos(i)+"_"+itos(i + 1));
            }

        }


        for (auto i  : V_c){

            // Constraint 7: Big M notation, make sure the arrival time to each charging station  +  charging/discharging time + travel time from i to j
            // must less than the arrival time of j.

            // Constraint 9: to travel from a charging station i to another place j, make sure the battery level is updated
            // for charging/discharging, also make sure the battery is decreased based on the travel distance and service energy.
            for (auto j :  solution_graph[i]) {
                GRBLinExpr lhs = GRB_battery_vars[j];
                GRBLinExpr rhs = GRB_battery_vars[i] - search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[j].node->graph_id)
                                                       * ev->driving_efficiency * GRB_visit_vars[edge_mapper[i * num_of_edges + j]]
                                 + bigM*(1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);

                for (int t = 0 ; t < num_of_time_bucket; t ++){
                    GRBLinExpr expr =  query->time_start + (t + 1) * time_bucket * (GRB_charging_vars[V_c_mapper[i]][t] +
                                                                                    GRB_discharging_vars[V_c_mapper[i]][t])
                                       + search_graph->get_edge_time(current_solution[i].node->graph_id, current_solution[j].node->graph_id)
                                         * GRB_visit_vars[edge_mapper[i * num_of_edges + j]] - bigM * (1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);
                    GRB_model->addConstr(expr  <= GRB_time_vars[j], "charging_station_travel_time_"+itos(i)+"_"+itos(j));
                    rhs += GRB_charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                    rhs -= GRB_discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                }
                GRB_model->addConstr(lhs  <= rhs, "cs_energy_consume_"+itos(i)+"_"+itos(j));
            }

            GRBLinExpr charging_exp = 0;
            GRBLinExpr discharging_exp = 0;
            GRBLinExpr sum_charging = 0;
            for (int t = 0 ; t < num_of_time_bucket; t ++){
                charging_exp += GRB_charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                discharging_exp += GRB_discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                sum_charging += GRB_charging_vars[V_c_mapper[i]][t];
                sum_charging += GRB_discharging_vars[V_c_mapper[i]][t];

                // Constraint 5: For each station, it can not charge and discharge at the same time.
                GRB_model->addConstr(GRB_charging_vars[V_c_mapper[i]][t] + GRB_discharging_vars[V_c_mapper[i]][t] <= 1, "not_charging_discharging_"+itos(i)+"_"+itos(t));

                // Constraint 6: Big M notation, for each charging station, it can not charge or discharge before arriving this station.
                GRBLinExpr lhs = GRB_time_vars[i] - query->time_start - t * time_bucket;
                GRBLinExpr rhs =
                        bigM * (1 - GRB_charging_vars[V_c_mapper[i]][t] - GRB_discharging_vars[V_c_mapper[i]][t]);
                GRB_model->addConstr(lhs <= rhs, "not_charging_before_arrival_" + itos(i));
            }
            // Constraint 10: make sure that, if charging, the charging amount does not exceed the batter capacity.
            // if discharging, the ev does not discharge more than current battery level.
            GRB_model->addConstr(charging_exp <= ev->battery_capacity - GRB_battery_vars[i], "maximal_charging_"+itos(i));
            GRB_model->addConstr(discharging_exp <= GRB_battery_vars[i], "maximal_discharging_"+itos(i));

            // Constraint 11: For each unvisited charging decision, make sure the battery level is 0. For the visited charging
            // decision, make sure the battery level is less than battery capacity.
            GRBLinExpr exp = 0;
            for(auto j : solution_graph[i]){
                exp += GRB_visit_vars[edge_mapper[i*num_of_edges+j]];
            }
            GRB_model->addConstr(GRB_battery_vars[i] <= exp * ev->battery_capacity, "charging"+itos(i));
            // Constraint 12: For each visited charging decision, make sure it is at least charging for one timestamp.
            GRB_model->addConstr(sum_charging  + bigM * ( 1 - exp ) >= 1, "least_charging_"+itos(i));


        }

        // order - working hour constraints;
        for(int i = 0; i < solution_graph.size()-1; i++){
            if(search_graph->is_order_node(current_solution[i].node->graph_id)) {
                GRB_model->addConstr(GRB_time_vars[i] >= query->working_time_start , "order_working_start_constraint_"+itos(i));
                GRB_model->addConstr(GRB_time_vars[i] + search_graph->get_service_time(current_solution[i].node->graph_id)<= query->working_time_end ,"order_working_end_constraint_"+itos(i));
            }
        }
        for (auto i  : V_c){
            if(!search_graph->is_sd_charging_station(current_solution[i].node->graph_id)) {
                for (int t = 0; t < num_of_time_bucket; t++) {
                    GRBLinExpr expr = query->time_start + (t +1)* time_bucket *
                                                          (GRB_charging_vars[V_c_mapper[i]][t] + GRB_discharging_vars[V_c_mapper[i]][t]);
                    GRB_model->addConstr(expr <= query->working_time_end,
                                    "working_end_charging_station_travel_time_" + itos(i));
                }
                GRB_model->addConstr(GRB_time_vars[i]  >= query->working_time_start, "working_start_charging_station_travel_time_"+itos(i));
            }
        }

        // Constraint 8: to travel from an order i to any other place j, make sure the battery level is decreased according.
        for (auto i : V_os){
            for (auto j : solution_graph[i]){
                GRBLinExpr lhs = GRB_battery_vars[j];
                GRBLinExpr rhs = GRB_battery_vars[i] - (search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[j].node->graph_id)
                                                        * ev->driving_efficiency + search_graph->get_service_energy(current_solution[i].node->graph_id))
                                                       * GRB_visit_vars[edge_mapper[i * num_of_edges + j]] + bigM*(1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);
                GRB_model->addConstr(lhs  <= rhs, "energy_consume_"+itos(i)+"_"+itos(j));
            }
        }


        // objective function:
        GRBLinExpr objective = 0;
        for (auto i : V_c){
            for (int t = 0; t < num_of_time_bucket; t++) {
                objective -= GRB_charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_price(current_solution[i].node->graph_id,t) ;
                objective += GRB_discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_discharging_price(current_solution[i].node->graph_id,t);
            }
        }
        for (auto i : current_solution){
            if(i.node->type == ORDER){
                objective += search_graph->get_profit(i.node->graph_id);
            }
        }
        GRB_model->setObjective(objective , GRB_MAXIMIZE);
        GRB_model->optimize();

        if (GRB_model->get(GRB_IntAttr_Status) == GRB_OPTIMAL || GRB_model->get(GRB_IntAttr_Status) == GRB_TIME_LIMIT) {
            return GRB_model->get(GRB_DoubleAttr_ObjVal);
        }else{
            return -1;
        }
    }


    // This function is used to convert all the MIP variable to vector based variable, and make it easier to input to debugging function
    void get_variable_value (const vector<Decision>& current_solution,vector<bool>& visit_solution_var,vector<double>& time_solution_var,
                             vector<double>& battery_solution_var, vector<vector<bool>>& charging_solution_var,vector<vector<bool>>&discharging_solution_var ){
        time_solution_var = vector<double>(current_solution.size());
        for(int i = 0; i < current_solution.size(); i++){
            time_solution_var[i]=GRB_time_vars[i].get(GRB_DoubleAttr_X);
        }
        battery_solution_var = vector<double>(current_solution.size());
        for(int i = 0; i < current_solution.size(); i++){
            battery_solution_var[i] = GRB_battery_vars[i].get(GRB_DoubleAttr_X);
        }
        visit_solution_var = vector<bool>(num_of_edges);
        for(int i = 0; i < num_of_edges; i++){
            visit_solution_var[i]=GRB_visit_vars[i].get(GRB_DoubleAttr_X);
        }
        charging_solution_var= vector<vector<bool>>(4);
        for(int i = 0; i < 4; i++){
            charging_solution_var[i] = vector<bool>(num_of_time_bucket);
            for(int t =0; t < num_of_time_bucket; t++){
                charging_solution_var[i][t] = GRB_charging_vars[i][t].get(GRB_DoubleAttr_X);
            }
        }

        discharging_solution_var= vector<vector<bool>>(4);
        for(int i = 0; i < 4; i++){
            discharging_solution_var[i] = vector<bool>(num_of_time_bucket);
            for(int t =0; t < num_of_time_bucket; t++){
                discharging_solution_var[i][t] = GRB_discharging_vars[i][t].get(GRB_DoubleAttr_X);
            }
        }
    }


    // This function is used to debug an existing solution, thus take the parameter value as input.
    double debuging_solve_optional_charging(const vector<Decision>& current_solution,vector<bool>& visit_solution_var,vector<double>& time_solution_var,
                                    vector<double>& battery_solution_var, vector<vector<bool>>& charging_solution_var,vector<vector<bool>>&discharging_solution_var ){

        //  Let's say we have a solution like this: S--------O1-------C1---------O2---------C2---------D
        //  This solver will first transfer the solution to this graph problem:
        //  S--------O1-------C1---------O2---------C2---------D
        //           |                   |                     |
        //           |-------------------|---------------------|
        // This basically says, although currently we decided to charge/discharge at C1 and C2, it is possible that
        // the optimal solution may doesn't need to go to C1 or C2.
        // I will try to carry this toy example in the rest of example.

        // V_c records the index of charging decision, in this case it will be <2,4>.
        // V_os records the index of source node and order node, in this case it will be <0,1,3>.
        // V_c_mapper maps the charging decision, to the decision variable of MIP model.
        // In this case, we only need to create two charging decision variable, so it maps 2 -> 0, 4 -> 1.
        vector<int> V_c = vector<int>();
        vector<int> V_os = vector<int>();
        V_c_mapper = vector<int>(current_solution.size(),-1);
        for (int i  = 0 ; i < current_solution.size(); i ++){
            if ( search_graph->is_charging_station_node(current_solution[i].node->graph_id)){
                V_c.push_back(i);
                V_c_mapper[i] = V_c.size()-1;
            }else if ( search_graph->is_order_node(current_solution[i].node->graph_id)) {
                V_os.push_back(i);
            }
        }
        V_os.push_back(0);


        // if there is no charging decision, we simply check whether the battery is sufficient to execute these order.
        V_c_size = V_c.size();
        if(V_c.empty()){
            double battery = query->battery_start;
            for ( int i = 0; i < current_solution.size() - 1 ; i++){
                battery -= search_graph->get_service_energy(current_solution[i].node->graph_id);
                battery -= search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[i+1].node->graph_id)*ev->driving_efficiency;
            }
            if(battery < query->battery_end ){
                return -1 ;
            }else {
                return 0;
            }
        }


        //  S--------O1-------C1---------O2---------C2---------D
        //           |                   |                     |
        //           |-------------------|---------------------|
        // solution graph records outgoing edges of each node. e.g., the outgoing node of S, is <1> (i.e., O1).
        // incoming_edge records incoming edges of each node. e.g., the incoming node of S, is < > (i.e., empty).
        solution_graph = vector<vector<int>>(current_solution.size());
        vector<vector<int>> incoming_edge = vector<vector<int>>(current_solution.size());
        num_of_edges = 0 ;
        for(int i = 0; i < solution_graph.size()-1; i++){
            int j = i + 1;
            // always add j ;
            solution_graph[i].push_back(j);
            incoming_edge[j].push_back(i);
            num_of_edges++;
            while(current_solution[j].node->type == Node_type::CS){
                // add skipping edge;
                solution_graph[i].push_back(j+1);
                incoming_edge[j+1].push_back(i);
                num_of_edges++;
                j++;
            }
        }

        // Initialise the model.
        GRB_model = new GRBModel(env);
        // Initialise edge decision variable, each of which is a boolean decision.
        // these edge decision variable indicated whether this edge is traversed or not (to skip charging decision).
        GRB_model->set(GRB_DoubleParam_MIPGap, 0.05);
        GRB_model->set(GRB_DoubleParam_TimeLimit, 5.0);  // Set to 60 seconds

        GRB_visit_vars = new GRBVar[num_of_edges];
        edge_mapper.clear();
        int index = 0;
        for(int i = 0; i < solution_graph.size()-1; i++){
            for(auto j : solution_graph[i]){
                GRB_visit_vars[index] = GRB_model->addVar(visit_solution_var[index], visit_solution_var[index], 0,
                                                          GRB_BINARY, "visit_"+itos(i)+"_"+itos(j));
                edge_mapper[i* num_of_edges + j] = index;
                index++;
            }
        }
        //edge_mapper map the edge from i to j, to its corresponding index.

        GRB_time_vars = new GRBVar[current_solution.size()];
        GRB_battery_vars = new GRBVar[current_solution.size()];
        for ( int i = 0; i < current_solution.size(); i++){
            // Initialise travel time decision variable, each of which correspond to a node, indicating when the EV arrive
            // at this node.
            GRB_time_vars[i] = GRB_model->addVar( time_solution_var[i],  time_solution_var[i], 0,
                                                  GRB_CONTINUOUS, "t_"+itos(i));
            //Initial the battery decision variable, each of which correspond to the SOC of each node.
            GRB_battery_vars[i] = GRB_model->addVar(battery_solution_var[i], battery_solution_var[i], 0,
                                                    GRB_CONTINUOUS, "b_"+itos(i));
        }


        // set each travel time to be between start time and finished time.
        GRB_time_vars[0].set(GRB_DoubleAttr_UB, query->time_start);
        GRB_time_vars[0].set(GRB_DoubleAttr_LB,query->time_start);
        GRB_model->addConstr(GRB_time_vars[current_solution.size() - 1] <= query->time_end, "destination_trav_time");


        GRB_battery_vars[0].set(GRB_DoubleAttr_UB, query->battery_start);
        GRB_battery_vars[0].set(GRB_DoubleAttr_LB, query->battery_start);
        GRB_model->addConstr(GRB_battery_vars[current_solution.size()-1] >= query->battery_end, "destination_battery_constraint");

        // Compute the number of time bucket(N), each time bucket has 20 mins.
        // For each charging station, initialise charging decision variable.
        // each charging station contains N number of boolean decision, which decide whether
        // EV is charging at this timestamp or not. Similarly, for discharing.
        num_of_time_bucket = floor((query->time_end - query->time_start)/time_bucket);
        // create charging variable;
        GRB_charging_vars = new GRBVar*[V_c.size()];
        // create discharging variable;
        GRB_discharging_vars = new GRBVar*[V_c.size()];
        for (int i = 0 ; i < V_c.size(); i++){
            GRB_charging_vars[i] = new GRBVar[num_of_time_bucket];
            GRB_discharging_vars[i] = new GRBVar[num_of_time_bucket];
            for (int j = 0 ; j < num_of_time_bucket; j++) {
                GRB_charging_vars[i][j] = GRB_model->addVar(charging_solution_var[i][j], charging_solution_var[i][j], 0,
                                                            GRB_BINARY, "c_"+itos(i)+"_"+itos(j));
                GRB_discharging_vars[i][j] = GRB_model->addVar(discharging_solution_var[i][j], discharging_solution_var[i][j], 0,
                                                               GRB_BINARY, "d_"+itos(i)+"_"+itos(j));
            }
        }


        for(int i = 0; i < solution_graph.size()-1; i++){
            // Constraint 1 :  ensure that if a->b is visited, then b->a must also visited.
            // it does not stop at any place between S to D.
            if( i >= 1){
                GRBLinExpr expr = 0;
                for(auto j : solution_graph[i]){
                    expr += GRB_visit_vars[edge_mapper[i* num_of_edges + j]];
                }
                for(auto j : incoming_edge[i]){
                    expr -= GRB_visit_vars[edge_mapper[j* num_of_edges + i]];
                }
                GRB_model->addConstr(expr == 0, "continue_travel_"+itos(i));
            }
            // Constraint 2 : for the edge that only have one outing edge, it must be visited. e.g., S---->O1 must be visited
            if(solution_graph[i].size() == 1 && current_solution[i].node->type != CS ){
                GRB_model->addConstr(GRB_visit_vars[edge_mapper[i* num_of_edges +  solution_graph[i][0]]] == 1);
            }
            // Constraint 3: Make sure the travel time is satisify the order's time window.
            if(search_graph->is_order_node(current_solution[i].node->graph_id)) {
                GRB_model->addConstr(GRB_time_vars[i] <= search_graph->get_order_end_time(current_solution[i].node->graph_id),
                                     "order_time_window_start_" + itos(i));
                GRB_model->addConstr(GRB_time_vars[i] >= search_graph->get_order_start_time(current_solution[i].node->graph_id),
                                     "order_time_window_end_" + itos(i));
            }

            // Constraint 4: Big M notation, make sure if EV travel from a to b. the travel time at b must larger than
            // the travel time at a + service time at a (pickup and delivery time) + travel time (from a to b).
            for(auto j : solution_graph [i]){
                GRBLinExpr expr = GRB_time_vars[i] + (search_graph->get_service_time(current_solution[i].node->graph_id)
                                                      + search_graph->get_edge_time(current_solution[i].node->graph_id,current_solution[j].node->graph_id))
                                                     * GRB_visit_vars[edge_mapper[i * num_of_edges + j]] - bigM * (1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);
                GRB_model->addConstr(expr <= GRB_time_vars[j], "travel_time_"+itos(i)+"_"+itos(i + 1));
            }

        }


        for (auto i  : V_c){

            // Constraint 7: Big M notation, make sure the arrival time to each charging station  +  charging/discharging time + travel time from i to j
            // must less than the arrival time of j.

            // Constraint 9: to travel from a charging station i to another place j, make sure the battery level is updated
            // for charging/discharging, also make sure the battery is decreased based on the travel distance and service energy.
            for (auto j :  solution_graph[i]) {
                GRBLinExpr lhs = GRB_battery_vars[j];
                GRBLinExpr rhs = GRB_battery_vars[i] - search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[j].node->graph_id)
                                                       * ev->driving_efficiency * GRB_visit_vars[edge_mapper[i * num_of_edges + j]]
                                 + bigM*(1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);

                for (int t = 0 ; t < num_of_time_bucket; t ++){
                    GRBLinExpr expr =  query->time_start + (t + 1) * time_bucket * (GRB_charging_vars[V_c_mapper[i]][t] +
                                                                                    GRB_discharging_vars[V_c_mapper[i]][t])
                                       + search_graph->get_edge_time(current_solution[i].node->graph_id, current_solution[j].node->graph_id)
                                         * GRB_visit_vars[edge_mapper[i * num_of_edges + j]] - bigM * (1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);
                    GRB_model->addConstr(expr  <= GRB_time_vars[j], "charging_station_travel_time_"+itos(i)+"_"+itos(j));
                    rhs += GRB_charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                    rhs -= GRB_discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                }
                GRB_model->addConstr(lhs  <= rhs, "cs_energy_consume_"+itos(i)+"_"+itos(j));
            }

            GRBLinExpr charging_exp = 0;
            GRBLinExpr discharging_exp = 0;
            GRBLinExpr sum_charging = 0;
            for (int t = 0 ; t < num_of_time_bucket; t ++){
                charging_exp += GRB_charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                discharging_exp += GRB_discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(current_solution[i].node->graph_id);
                sum_charging += GRB_charging_vars[V_c_mapper[i]][t];
                sum_charging += GRB_discharging_vars[V_c_mapper[i]][t];

                // Constraint 5: For each station, it can not charge and discharge at the same time.
                GRB_model->addConstr(GRB_charging_vars[V_c_mapper[i]][t] + GRB_discharging_vars[V_c_mapper[i]][t] <= 1, "not_charging_discharging_"+itos(i)+"_"+itos(t));

                // Constraint 6: Big M notation, for each charging station, it can not charge or discharge before arriving this station.
                GRBLinExpr lhs = GRB_time_vars[i] - query->time_start - t * time_bucket;
                GRBLinExpr rhs =
                        bigM * (1 - GRB_charging_vars[V_c_mapper[i]][t] - GRB_discharging_vars[V_c_mapper[i]][t]);
                GRB_model->addConstr(lhs <= rhs, "not_charging_before_arrival_" + itos(i));
            }
            // Constraint 10: make sure that, if charging, the charging amount does not exceed the batter capacity.
            // if discharging, the ev does not discharge more than current battery level.
            GRB_model->addConstr(charging_exp <= ev->battery_capacity - GRB_battery_vars[i], "maximal_charging_"+itos(i));
            GRB_model->addConstr(discharging_exp <= GRB_battery_vars[i], "maximal_discharging_"+itos(i));

            // Constraint 11: For each unvisited charging decision, make sure the battery level is 0. For the visited charging
            // decision, make sure the battery level is less than battery capacity.
            GRBLinExpr exp = 0;
            for(auto j : solution_graph[i]){
                exp += GRB_visit_vars[edge_mapper[i*num_of_edges+j]];
            }
            GRB_model->addConstr(GRB_battery_vars[i] <= exp * ev->battery_capacity, "charging"+itos(i));
            // Constraint 12: For each visited charging decision, make sure it is at least charging for one timestamp.
            GRB_model->addConstr(sum_charging  + bigM * ( 1 - exp ) >= 1, "least_charging_"+itos(i));


        }

        // order - working hour constraints;
        for(int i = 0; i < solution_graph.size()-1; i++){
            if(search_graph->is_order_node(current_solution[i].node->graph_id)) {
                GRB_model->addConstr(GRB_time_vars[i] >= query->working_time_start , "order_working_start_constraint_"+itos(i));
                GRB_model->addConstr(GRB_time_vars[i] + search_graph->get_service_time(current_solution[i].node->graph_id)<= query->working_time_end ,"order_working_end_constraint_"+itos(i));
            }
        }
        for (auto i  : V_c){
            if(!search_graph->is_sd_charging_station(current_solution[i].node->graph_id)) {
                for (int t = 0; t < num_of_time_bucket; t++) {
                    GRBLinExpr expr = query->time_start + (t + 1) * time_bucket *
                                                          (GRB_charging_vars[V_c_mapper[i]][t] + GRB_discharging_vars[V_c_mapper[i]][t]);
                    GRB_model->addConstr(expr <= query->working_time_end,
                                         "working_end_charging_station_travel_time_" + itos(i));
                }
                GRB_model->addConstr(GRB_time_vars[i]  >= query->working_time_start, "working_start_charging_station_travel_time_"+itos(i));
            }
        }

        // Constraint 8: to travel from an order i to any other place j, make sure the battery level is decreased according.
        for (auto i : V_os){
            for (auto j : solution_graph[i]){
                GRBLinExpr lhs = GRB_battery_vars[j];
                GRBLinExpr rhs = GRB_battery_vars[i] - (search_graph->get_edge_distance(current_solution[i].node->graph_id,current_solution[j].node->graph_id)
                                                        * ev->driving_efficiency + search_graph->get_service_energy(current_solution[i].node->graph_id))
                                                       * GRB_visit_vars[edge_mapper[i * num_of_edges + j]] + bigM*(1 - GRB_visit_vars[edge_mapper[i * num_of_edges + j]]);
                GRB_model->addConstr(lhs  <= rhs, "energy_consume_"+itos(i)+"_"+itos(j));
            }
        }



        // objective function:
        GRBLinExpr objective = 0;
        for (auto i : V_c){
            for (int t = 0; t < num_of_time_bucket; t++) {
                objective -= GRB_charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_price(current_solution[i].node->graph_id,t) ;
                objective += GRB_discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_discharging_price(current_solution[i].node->graph_id,t);
            }
        }
        for (auto i : current_solution){
            if(i.node->type == ORDER){
                objective += search_graph->get_profit(i.node->graph_id);
            }
        }
        GRB_model->setObjective(objective , GRB_MAXIMIZE);
        GRB_model->optimize();

        if (GRB_model->get(GRB_IntAttr_Status) == GRB_OPTIMAL || GRB_model->get(GRB_IntAttr_Status) == GRB_TIME_LIMIT) {
            cout<<"Printing decision variable"<<endl;
            cout<<"time vars: "<<endl;
            for(int i = 0; i < current_solution.size(); i++){
                cout<<std::setprecision(16)<<GRB_time_vars[i].get(GRB_DoubleAttr_X)<< ",";
            }
            cout << endl;
            cout<<"battery vars: "<<endl;
            for(int i = 0; i < current_solution.size(); i++){
                cout<<std::setprecision(16)<<GRB_battery_vars[i].get(GRB_DoubleAttr_X)<< ",";
            }
            cout << endl;
            cout<<"visit vars: "<<endl;
            for(int i = 0; i < num_of_edges; i++){
                cout<<std::setprecision(16)<<(int)GRB_visit_vars[i].get(GRB_DoubleAttr_X)<< ",";
            }
            cout << endl;
            cout<<"charging vars: "<<endl;
            for(int i = 0; i < V_c.size(); i++){
                for(int t =0; t < num_of_time_bucket; t++){
                    cout<<std::setprecision(16)<<(int)GRB_charging_vars[i][t].get(GRB_DoubleAttr_X)<< ",";
                }
                cout << endl;
            }
            cout << endl;
            cout<<"discharging vars: "<<endl;
            for(int i = 0; i < V_c.size(); i++){
                for(int t =0; t < num_of_time_bucket; t++){
                    cout<<std::setprecision(16)<<(int)GRB_discharging_vars[i][t].get(GRB_DoubleAttr_X)<< ",";
                }
                cout << endl;
            }
            cout << endl;

            return GRB_model->get(GRB_DoubleAttr_ObjVal);
        }else{
            cout<<"infeasible solution"<<endl;
            GRB_model->computeIIS();
            GRB_model->write("./model.ilp");
            return -1;
        }
    }





    // retrieve the solution.
    vector<Decision> get_current_solution( const vector<Decision>& current_solution ) {
        vector<Decision> new_solution = vector<Decision>{Decision{search_graph->get_node_ptr(0)}};
        int curr_id = 0;
        while (new_solution.back().node->type != Node_type::DESTINATION) {
            for (auto j: solution_graph[curr_id]) {
                if (GRB_visit_vars[edge_mapper[num_of_edges * curr_id + j]].get(GRB_DoubleAttr_X) > 0.5) {
                    if (search_graph->is_charging_station_node(current_solution[j].node->graph_id)) {
                        int charge_begin = num_of_time_bucket * 2;
                        int charge_end = 0;
                        new_solution.push_back(Decision{current_solution[j].node,
                                                        GRB_time_vars[j].get(GRB_DoubleAttr_X),
                                                        GRB_battery_vars[j].get(GRB_DoubleAttr_X),
                        });
                        new_solution[new_solution.size()-1].charging_plan.resize(num_of_time_bucket);
                        new_solution[new_solution.size()-1].discharging_plan.resize(num_of_time_bucket);
                        for (int t = 0; t < num_of_time_bucket; t++) {
                            if (GRB_charging_vars[V_c_mapper[j]][t].get(GRB_DoubleAttr_X) > 0.5) {
                                new_solution[new_solution.size()-1].charging_plan[t] = true;
                            }
                            if (GRB_discharging_vars[V_c_mapper[j]][t].get(GRB_DoubleAttr_X) > 0.5) {
                                new_solution[new_solution.size()-1].discharging_plan[t] = true;
                            }
                        }
                        double arrive_time = GRB_time_vars[j].get(GRB_DoubleAttr_X);
                        int bucket_start = ceil((arrive_time - query->time_start)/time_bucket);
                        // move the charging and discharging to earliest possible .
                        for( int t = 0; t < num_of_time_bucket; t ++){
                            if(new_solution[new_solution.size()-1].charging_plan[t]){
                                bool swapped  = false;
                                for( int tt = bucket_start; tt < t; tt++){
                                    if(!new_solution[new_solution.size()-1].charging_plan[tt]
                                       && !new_solution[new_solution.size()-1].discharging_plan[tt]){
                                        if(search_graph->get_bucket_charging_price(new_solution[new_solution.size()-1].node->graph_id,tt)
                                           == search_graph->get_bucket_charging_price(new_solution[new_solution.size()-1].node->graph_id,tt)){
                                            new_solution[new_solution.size()-1].charging_plan[tt] = true;
                                            new_solution[new_solution.size()-1].charging_plan[t] = false;
                                            charge_begin = min(charge_begin, tt);
                                            charge_end = max(charge_end, tt);
                                            swapped = true;
                                            break;
                                        }
                                    }
                                }
                                if(!swapped){
                                    charge_begin = min(charge_begin, t);
                                    charge_end = max(charge_end, t);
                                }
                            }
                        }

                        for( int t = 0; t < num_of_time_bucket; t ++){
                            if(new_solution[new_solution.size()-1].discharging_plan[t]){
                                bool swapped  = false;
                                for( int tt = bucket_start; tt < t; tt++){
                                    if(!new_solution[new_solution.size()-1].charging_plan[t]
                                       && !new_solution[new_solution.size()-1].discharging_plan[t]){
                                        if(search_graph->get_bucket_discharging_price(new_solution[new_solution.size()-1].node->graph_id,tt)
                                           == search_graph->get_bucket_discharging_price(new_solution[new_solution.size()-1].node->graph_id,tt)){
                                            new_solution[new_solution.size()-1].discharging_plan[tt] = true;
                                            new_solution[new_solution.size()-1].discharging_plan[t] = false;
                                            charge_begin = min(charge_begin, tt);
                                            charge_end = max(charge_end, tt);
                                            swapped = true;
                                            break;
                                        }
                                    }
                                }
                                if(!swapped){
                                    charge_begin = min(charge_begin, t);
                                    charge_end = max(charge_end, t);
                                }
                            }
                        }
                        new_solution[new_solution.size()-1].charging_begin = charge_begin;
                        new_solution[new_solution.size()-1].charging_end = charge_end;
                    } else {
                        new_solution.push_back(Decision{current_solution[j].node,
                                                        GRB_time_vars[j].get(GRB_DoubleAttr_X),
                                                        GRB_battery_vars[j].get(GRB_DoubleAttr_X)
                        });
                    }
                    curr_id = j;
                    break;
                }
            }
        }
        return new_solution;
    }
//        vector<int> V_c = vector<int>();
//        vector<int> V_os = vector<int>();
//        V_c_mapper = vector<int>(current_solution.size(),-1);
//        for (int i  = 0 ; i < current_solution.size(); i ++){
//            if ( search_graph->is_charging_station_node(current_solution[i].node->graph_id)){
//                V_c.push_back(i);
//                V_c_mapper[i] = V_c.size()-1;
//            }else if ( search_graph->is_order_node(current_solution[i].node->graph_id)) {
//                V_os.push_back(i);
//            }
//        }
//            cout<<"Printing decision metric:"<<endl;
//            for(int i = 0; i < current_solution.size() - 1; i++){
//                cout<<"printing edge for node: "<<i<<endl;
//                for(auto j : solution_graph[i]){
//                    cout << "From: "<< i << " to " << j << ": "<<GRB_visit_vars[edge_mapper[num_of_edges*i + j]].get(GRB_DoubleAttr_X)<< endl;
//                }
//                cout << "" <<endl;
//            }
//
//            cout << "Printing charging decision: "<< endl;
//            for ( auto i : V_c){
//                for (int t = 0 ; t < num_of_time_bucket; t ++){
//                    cout<<"Charging station: "<< current_solution[i].node->graph_id << "Time step: " << t << ". "<< GRB_charging_vars[V_c_mapper[i]][t].get(GRB_DoubleAttr_X)<< endl;
//                }
//            }
//
//            cout << "Printing discharging decision: "<< endl;
//            for ( auto i : V_c){
//                for (int t = 0 ; t < num_of_time_bucket; t ++){
//                    cout<<"Discharging station: "<< current_solution[i].node->graph_id << "Time step: " << t << ". "<< GRB_discharging_vars[V_c_mapper[i]][t].get(GRB_DoubleAttr_X)<< endl;
//                }
//            }
//    }

    void reset_solver(){
        if(V_c_size != 0 ){
            delete GRB_model;
            delete[] GRB_time_vars;
            delete[] GRB_visit_vars;
            delete[] GRB_battery_vars;

            for (int i = 0; i < V_c_size; i++)
                delete[] GRB_charging_vars[i];

            for (int i = 0; i < V_c_size; i++)
                delete[] GRB_discharging_vars[i];
        }
    }



    double evaluate_solution( const vector<int>&  action_sequence){
        vector<int> V_c = vector<int>();
        vector<int> V_c_mapper = vector<int>(action_sequence.size(),-1);
        for (int i  = 0 ; i < action_sequence.size(); i ++){
            if ( search_graph->is_charging_station_node(action_sequence[i])){
                V_c.push_back(action_sequence[i]);
                V_c_mapper[i] = V_c.size()-1;
            }
        }
        if(V_c.empty()){
            return 0 ;
        }

        GRBModel model = GRBModel(env);


        GRBVar *time_vars = new GRBVar[action_sequence.size()];
        for ( int i = 0; i < action_sequence.size(); i++){
            time_vars[i] = model.addVar( query->time_start,  query->time_end, 0,
                                         GRB_CONTINUOUS, "t_"+itos(i));
        }

        time_vars[0].set(GRB_DoubleAttr_UB, query->time_start);
        time_vars[0].set(GRB_DoubleAttr_LB,query->time_start);
        model.addConstr(time_vars[action_sequence.size() - 1] <= query->time_end, "destination_trav_time");

        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            if(search_graph->is_order_node(action_sequence[i])) {
                model.addConstr(time_vars[i] <= search_graph->get_order_end_time(action_sequence[i]),
                                "order_time_window_start_" + itos(i));
                model.addConstr(time_vars[i] >= search_graph->get_order_start_time(action_sequence[i]),
                                "order_time_window_end_" + itos(i));
            }
        }
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            GRBLinExpr expr = time_vars[i] + (search_graph->get_service_time(action_sequence[i])
                                              + search_graph->get_edge_time(action_sequence[i],action_sequence[i + 1]));
            model.addConstr(expr <= time_vars[i + 1], "travel_time_"+itos(i)+"_"+itos(i + 1));
        }

        int number_of_time_bucket = floor((query->time_end - query->time_start)/time_bucket);


        // create charging variable;
        GRBVar ** charging_vars = NULL;
        charging_vars = new GRBVar*[V_c.size()];
        for (int i = 0; i < V_c.size(); i++)
            charging_vars[i] = new GRBVar[number_of_time_bucket];
        for (int i = 0 ; i < V_c.size(); i++){
            for (int j = 0 ; j < number_of_time_bucket; j++) {
                charging_vars[i][j] = model.addVar(0.0, 1.0, 0,
                                                   GRB_BINARY, "c_"+itos(i)+"_"+itos(j));
            }
        }

        // create discharging variable;
        GRBVar ** discharging_vars = NULL;
        discharging_vars = new GRBVar*[V_c.size()];
        for (int i = 0; i < V_c.size(); i++)
            discharging_vars[i] = new GRBVar[number_of_time_bucket];
        for (int i = 0 ; i < V_c.size(); i++){
            for (int j = 0 ; j < number_of_time_bucket; j++) {
                discharging_vars[i][j] = model.addVar(0.0, 1.0, 0,
                                                      GRB_BINARY, "d_"+itos(i)+"_"+itos(j));
            }
        }

        // can not discharging and charging at the same time;
        // implementation of Eq:(15).
        for (int i = 0 ; i < V_c.size(); i++){
            for (int t = 0 ; t < number_of_time_bucket; t++) {
                model.addConstr(charging_vars[i][t] + discharging_vars[i][t] <= 1, "not_charging_discharging_"+itos(i)+"_"+itos(t));
            }
        }


        for (int i = 0 ; i < V_c.size(); i++){
            GRBLinExpr exp = 0;
            for (int t = 0 ; t < number_of_time_bucket; t++) {
                exp += charging_vars[i][t] + discharging_vars[i][t];
            }
            model.addConstr(exp >=1, "must_charge_one_"+itos(i));
        }

        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            if(search_graph->is_charging_station_node(action_sequence[i])) {
                for (int t = 0; t < number_of_time_bucket; t++) {
                    GRBLinExpr lhs = time_vars[i] - query->time_start - t * time_bucket;
                    GRBLinExpr rhs =
                            bigM * (1 - charging_vars[V_c_mapper[i]][t] - discharging_vars[V_c_mapper[i]][t]);
                    model.addConstr(lhs <= rhs, "not_charging_before_arrival_" + itos(i));

                    GRBLinExpr expr = query->time_start + (t + 1) * time_bucket * (charging_vars[V_c_mapper[i]][t] +
                                                                                   discharging_vars[V_c_mapper[i]][t])
                                      + search_graph->get_edge_time(action_sequence[i], action_sequence[i + 1]);
                    model.addConstr(expr <= time_vars[i + 1],
                                    "charging_station_travel_time_" + itos(i) + "_" + itos(i + 1));
                }
            }
        }


        GRBVar *battery_vars = new GRBVar[action_sequence.size()];
        for ( int i = 0; i < action_sequence.size(); i++){
            battery_vars[i] = model.addVar(0, ev->battery_capacity, 0,
                                           GRB_CONTINUOUS, "b_"+itos(i));
        }
        // implementation of Eq:(11).
        battery_vars[0].set(GRB_DoubleAttr_UB, query->battery_start);
        battery_vars[0].set(GRB_DoubleAttr_LB, query->battery_start);
        // implementation of Eq:(12).
        model.addConstr(battery_vars[action_sequence.size()-1] >= query->battery_end, "destination_battery_constraint");


        // make sure each traversal between edges has enough of battery.
        // implementation of Eq:(13).
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            if(search_graph->is_charging_station_node(action_sequence[i])) {

                GRBLinExpr lhs = battery_vars[i+1];
                GRBLinExpr rhs = battery_vars[i] - search_graph->get_edge_distance(action_sequence[i],action_sequence[i+1])* ev->driving_efficiency;
                for (int t = 0 ; t < number_of_time_bucket; t ++){
                    rhs += charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]);
                    rhs -= discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]);
                }
                model.addConstr(lhs  <= rhs, "cs_energy_consume_"+itos(i)+"_"+itos(i+1));

                GRBLinExpr charging_exp = 0;
                GRBLinExpr discharging_exp = 0;
                for (int t = 0 ; t < number_of_time_bucket; t ++){
                    charging_exp += charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]) ;
                    discharging_exp += discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_amount(action_sequence[i]) ;
                }
                model.addConstr(charging_exp <= ev->battery_capacity - battery_vars[i], "maximal_charging_"+itos(i));
                model.addConstr(discharging_exp <= battery_vars[i], "maximal_discharging_"+itos(i));

            }else{
                GRBLinExpr lhs = battery_vars[i+1];
                GRBLinExpr rhs = battery_vars[i] - (search_graph->get_edge_distance(action_sequence[i],action_sequence[i+1])* ev->driving_efficiency +
                                                    search_graph->get_service_energy(action_sequence[i]));
                model.addConstr(lhs  <= rhs, "energy_consume_"+itos(i)+"_"+itos(i+1));

            }
        }



        // objective function:
        GRBLinExpr objective = 0;
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            if(search_graph->is_charging_station_node(action_sequence[i])) {
                for (int t = 0; t < number_of_time_bucket; t++) {
                    objective -= charging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_charging_price(action_sequence[i],t) ;
                    objective += discharging_vars[V_c_mapper[i]][t] * search_graph->get_bucket_discharging_price(action_sequence[i],t);
                }
            }
        }

        model.setObjective(objective , GRB_MAXIMIZE);
        model.optimize();

        if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL) {
//            cout <<" here "<<endl;
//            print_solution(action_sequence,V_c_mapper,number_of_time_bucket,battery_vars,time_vars,charging_vars,discharging_vars);
            return model.get(GRB_DoubleAttr_ObjVal);
        }else{
//            cout<<"not feasible"<<endl;
            return -1;
        }
    }







    void validate_order_plan (vector<int> action_sequence){
        // check if a plan only contains order is validate or not ?
        double profit = 0;
        int current_id = 0;
        double battery = ev->battery_capacity;
        double time = query->time_start;
        cout<<" Printing plan: "<< endl;
        cout<<" Initial battery: "<< battery << endl;
        cout<<" Time start: "<< time << endl;
        cout<<" Node Id: 0. " << endl;
        cout<< *search_graph->get_node_ptr(action_sequence[0]) <<endl;
        for( int i = 1; i < action_sequence.size(); i++){
            battery -= ev->driving_efficiency * search_graph->get_edge_distance(current_id,action_sequence[i])
                       + search_graph->get_service_energy(action_sequence[i]);
            double arrival_time  =  time + search_graph->get_edge_time(current_id,action_sequence[i]);
            cout<<" Arrival time: "<< arrival_time << endl;
            if( i != action_sequence.size()-1 ){
                if( arrival_time > search_graph->get_order_end_time(action_sequence[i])){
                    cout << " invalid !!!!!!!!!!" <<endl;
                }

                cout<<" Start service time: "<<  max(arrival_time, search_graph->get_order_start_time(action_sequence[i])) << endl;
                cout<<" Service time: "<<  search_graph ->get_service_time(action_sequence[i]) << endl;
                time = max(arrival_time, search_graph->get_order_start_time(action_sequence[i]))+
                       search_graph ->get_service_time(action_sequence[i]);
            }else{
                time = arrival_time + search_graph ->get_service_time(action_sequence[i]);
            }
            profit += search_graph->get_profit(action_sequence[i]);

            cout<<" Finish battery: "<< battery << endl;
            cout<<" Finish time: "<< time << endl;
            cout<<" Node Id: "<< action_sequence[i] << endl;
            cout<< *search_graph->get_node_ptr(action_sequence[i]) <<endl;
            current_id = action_sequence[i];

        }
        cout<<"Total Profit:" << profit <<endl;
        double battery_cost = 0;
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            battery_cost += search_graph->get_edge_distance(action_sequence[i],action_sequence[i+1])* ev->driving_efficiency +
                            search_graph->get_service_energy(action_sequence[i]);
        }
        cout <<" Total battery" <<battery_cost << endl ;


        cout<<"Checking with optimization" << profit <<endl;
        GRBModel model = GRBModel(env);
        GRBVar *time_vars = new GRBVar[action_sequence.size()];
        for ( int i = 0; i < action_sequence.size(); i++){
            time_vars[i] = model.addVar( query->time_start,  query->time_end, 0,
                                         GRB_CONTINUOUS, "t_"+itos(i));
        }

        time_vars[0].set(GRB_DoubleAttr_UB, query->time_start);
        time_vars[0].set(GRB_DoubleAttr_LB,query->time_start);
        model.addConstr(time_vars[action_sequence.size() - 1] <= query->time_end, "destination_trav_time");

        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            if(search_graph->is_order_node(action_sequence[i])) {
                model.addConstr(time_vars[i] <= search_graph->get_order_end_time(action_sequence[i]),
                                "order_time_window_start_" + itos(i));
                model.addConstr(time_vars[i] >= search_graph->get_order_start_time(action_sequence[i]),
                                "order_time_window_end_" + itos(i));
            }
        }
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            GRBLinExpr expr = time_vars[i] + (search_graph->get_service_time(action_sequence[i])
                                              + search_graph->get_edge_time(action_sequence[i],action_sequence[i + 1]));
            model.addConstr(expr <= time_vars[i + 1], "travel_time_"+itos(i)+"_"+itos(i + 1));
        }


        GRBVar *battery_vars = new GRBVar[action_sequence.size()];
        for ( int i = 0; i < action_sequence.size(); i++){
            battery_vars[i] = model.addVar(0, ev->battery_capacity, 0,
                                           GRB_CONTINUOUS, "b_"+itos(i));
        }
        // implementation of Eq:(11).
        battery_vars[0].set(GRB_DoubleAttr_UB, query->battery_start);
        battery_vars[0].set(GRB_DoubleAttr_LB, query->battery_start);
        // implementation of Eq:(12).
        model.addConstr(battery_vars[action_sequence.size()-1] >= query->battery_end, "destination_battery_constraint");


        // make sure each traversal between edges has enough of battery.
        // implementation of Eq:(13).
        for(int i = 0; i < action_sequence.size() - 1 ; i++ ){
            GRBLinExpr lhs = battery_vars[i+1];
            GRBLinExpr rhs = battery_vars[i] - (search_graph->get_edge_distance(action_sequence[i],action_sequence[i+1])* ev->driving_efficiency +
                                                search_graph->get_service_energy(action_sequence[i]));
            model.addConstr(lhs  <= rhs, "energy_consume_"+itos(i)+"_"+itos(i+1));


        }

        GRBLinExpr objective = 0;
        for (int i = 0; i < action_sequence.size() ; i++ ){
            objective += time_vars[i];
        }

        model.setObjective(objective , GRB_MAXIMIZE);
        model.optimize();

        if (model.get(GRB_IntAttr_Status) != GRB_OPTIMAL) {
            cout<<" invalid plan "<< endl;
        }
    }

private:
    int time_bucket;
    Graph* search_graph;
    Query* query;
    EV_setting* ev;
    GRBEnv env = GRBEnv(true);
    GRBModel* GRB_model;
    GRBVar * GRB_time_vars;
    GRBVar * GRB_visit_vars;
    GRBVar ** GRB_charging_vars;
    GRBVar ** GRB_discharging_vars;
    GRBVar * GRB_battery_vars ;
    int V_c_size;
    int num_of_edges;
    int num_of_time_bucket;
    vector<int> V_c_mapper;
    vector<vector<int>> solution_graph;
    std::map<int, int> edge_mapper;
};