/* Copyright 2023, Gurobi Optimization, LLC */

/* This example formulates and solves the following simple MIP model:

     maximize    x +   y + 2 z
     subject to  x + 2 y + 3 z <= 4
                 x +   y       >= 1
                 x, y, z binary
*/

#include "gurobi_c++.h"
#include "graph.h"
#include <cassert>
#include <cstdlib>
#include <cmath>
#include <sstream>
using namespace std;

int bigM = 100000;
int time_bucket = 10;
bool VERBOSE = false;


string itos(int i) {stringstream s; s << i; return s.str(); }

int read_file(const string input_file){
    std::ifstream inputFile(input_file);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open the file." << std::endl;
        return 1;
    }

    std::string line;
    std::string column1, column2;
    if (std::getline(inputFile, line)) {
        std::istringstream stream(line);

        // Read the first and second columns
        if (std::getline(stream, column1, ',') && std::getline(stream, column2, ',')) {
            std::cout << "First value: " << column1 << ", Second value: " << column2 << std::endl;
        } else {
            std::cerr << "Error: Could not read two values from the line." << std::endl;
        }
    } else {
        std::cerr << "Error: Could not read the first line." << std::endl;
    }

    inputFile.close();
    int num = stoi(column2);
    if(num == 0){num = 1;};
    return num;
}

int revisit_count(const std::string& file1, const std::string& file2){
    int num1 = read_file(file1);
    int num2 = read_file(file2);

    return std::max(num1,num2);
}

void print_objective(const  GRBModel& model){
    GRBQuadExpr objExpr = model.getObjective();
    double objectiveValue = model.get(GRB_DoubleAttr_ObjVal);
    std::cout << "Objective Function Value: " << objectiveValue << std::endl;
    GRBVar* vars = model.getVars();
    int numVars = model.get(GRB_IntAttr_NumVars);
    std::cout << "Objective Function Details:" << std::endl;
    for (int i = 0; i < numVars; i++) {
        GRBVar var = vars[i];
        double coefficient = var.get(GRB_DoubleAttr_Obj);
        std::cout<< "Name: "<< var.get(GRB_StringAttr_VarName) << " Variable " << i << ": Coefficient = " << coefficient << std::endl;

    }
    delete[] vars;
}


void print_solution(const int& num_of_nodes, GRBVar **vars, GRBVar *time_vars,GRBVar **charging_vars, GRBVar **discharging_vars,GRBVar *battery_level,
                    const Graph& g , const vector<int>& V_c_mapper,EV_setting ev, Query q, std::ostream& os){
    os << "Optimal Solution:"<<endl;
    os << endl;
    os << q << endl;
    os << ev << endl;
    os <<"Printing solution:" << endl;
    os <<"Departure from the source: "<< g.get_s_nodes() << ", departure time: "<< g.get_query_start_time() <<endl;
    os <<"Battery level :" << battery_level[g.get_s_nodes()].get(GRB_DoubleAttr_X)  <<endl;
    os <<endl;
    vector<bool> node_visited(num_of_nodes,false);
    node_visited[g.get_s_nodes()] = true;
    int current_node = g.get_s_nodes();
    bool found = true;
    int number_of_time_bucket = floor((g.get_query_end_time() - g.get_query_start_time())/time_bucket);
    double profit = 0;
    while (found){
        found = false;
        for (int j = 0; j < num_of_nodes; j++) {
            if (vars[current_node][j].get(GRB_DoubleAttr_X) > 0.5) {
                if ( g.is_order_node(j) && node_visited[j]){
                        os <<" Infeasible solution: order node already visited"<<endl;
                }
                if(g.is_charging_station_node(j)){
                    int count = 0;
                    for (int k = 0; k < num_of_nodes; k++){
                        if (vars[j][k].get(GRB_DoubleAttr_X) > 0.5){
                            count ++;
                        }
                    }
                    if(count > 1){
                        os <<"charging station has more than one outgoing arcs"<<endl;
                    }
                }
                os << "Traveling from node: "<<current_node << " to "<< j << ", travel time: "<< g.get_edge_time(current_node, j)
                <<" Energy consume: "<<g.get_edge_distance(current_node, j) * ev.driving_efficiency << endl;
                os << "Arrival at node: "<< j <<" arrival time: "<< time_vars[j].get(GRB_DoubleAttr_X) << endl;
                os << g.get_node(j) <<endl;
                os << "Battery level: "<< battery_level[j].get(GRB_DoubleAttr_X) << endl;

                if(g.is_order_node(j)){
                    profit += g.get_profit(j);
                }
                if(g.is_charging_station_node(j)){
                    for (int  t = 0; t < number_of_time_bucket; t++ ){
                        if(charging_vars[V_c_mapper[j]][t].get(GRB_DoubleAttr_X) > 0.5){
                            os << "Charging: " << g.get_query_start_time() + t*time_bucket <<" to " << g.get_query_start_time() +(t+1)*time_bucket <<
                            " energy charged: "<< g.get_bucket_charging_amount(j) <<
                            " charging cost: "<< g.get_bucket_charging_price(j,t) << std::endl;
                            profit -= g.get_bucket_charging_price(j,t);
                        }
                    }
                    os<< std::endl;
                    for (int  t = 0; t < number_of_time_bucket; t++ ){
                        if(discharging_vars[V_c_mapper[j]][t].get(GRB_DoubleAttr_X) > 0.5){
                            os << "Discharging: " << g.get_query_start_time() + t*time_bucket <<" to " << g.get_query_start_time() + (t+1)*time_bucket<<
                            " energy discharged: "<< g.get_bucket_charging_amount(j) <<
                            " discharging cost: "<< g.get_bucket_discharging_price(j,t) << std::endl;
                            profit += g.get_bucket_discharging_price(j,t);
                        }
                    }
                    os << std::endl;
                }
                os << " "<<endl;
                current_node = j;
                node_visited[j] = true;
                found = true;
                break;
            }
        }
    }
    os <<"Sum of Profit: " << profit<<endl;
}


void  run_MIP_model( Graph& g, Query& q, EV_setting& ev, GRBModel&  model, std::ostream& os, int vn){

//    model.set(GRB_DoubleParam_TimeLimit, 5.0);

    g.initialise_graph(q,ev,time_bucket,vn,1);
    int num_of_nodes = g.get_number_of_nodes();

    GRBVar **vars = NULL;
    vars = new GRBVar*[num_of_nodes];
    for (int i = 0; i < num_of_nodes; i++)
        vars[i] = new GRBVar[num_of_nodes];

    for ( int i = 0; i < num_of_nodes; i++){
        for ( int j = 0; j < num_of_nodes; j++) {
            vars[i][j] = model.addVar(0.0, 1.0, 0,
                                      GRB_BINARY, "x_"+itos(i)+"_"+itos(j));
        }
    }
    vector<vector<int>> vn_nodes = g.get_virtual_nodes();
    for(auto& same_nodes : vn_nodes){
        for ( auto i : same_nodes){
            for(auto j : same_nodes){
                if(i == j) continue;
                // never travel between virtual nodes;
                vars[i][j].set(GRB_DoubleAttr_UB, 0);
                vars[j][i].set(GRB_DoubleAttr_UB, 0);
            }
        }
    }

    for (int i = 0; i < num_of_nodes; i++){
        // do not self travel
        vars[i][i].set(GRB_DoubleAttr_UB, 0);
        // do not travel to s
        vars[i][g.get_s_nodes()].set(GRB_DoubleAttr_UB, 0);
        // do not travel from s
        vars[g.get_d_nodes()][i].set(GRB_DoubleAttr_UB, 0);
    }

    auto forbidden = g.get_forbidden_arc();
    for( auto forbidden_arc : forbidden){
        vars[get<0>(forbidden_arc)][get<1>(forbidden_arc)].set(GRB_DoubleAttr_UB, 0);
    }

    // must departure at s constraint;
    // implementation of Eq:(2).
    GRBLinExpr must_departure_s_constraint = 0;
    const vector<int> V_ocd = g.get_union_nodes({Node_type::ORDER,Node_type::CS,Node_type::DESTINATION});
    GRBLinExpr must_not_arrive_s_constraint = 0;
    for(auto i : V_ocd){
        must_departure_s_constraint  += vars[g.get_s_nodes()][i];
    }
    model.addConstr(must_departure_s_constraint == 1, "must_departure_s");



    // must arrive at d constraint;
    // implementation of Eq:(3).
    const vector<int> V_ocs = g.get_union_nodes({Node_type::ORDER,Node_type::CS,Node_type::SOURCE});
    GRBLinExpr must_arrival_d_constraint = 0;
    GRBLinExpr must_not_leave_d_constraint = 0;
    for(auto i : V_ocs){
        must_arrival_d_constraint += vars[i][g.get_d_nodes()];
    }
    model.addConstr(must_arrival_d_constraint == 1, "must_arrival_d");


    // Make sure each order is visit at most once;
    // implementation of Eq:(4).
    const vector<int> V_o = g.get_union_nodes({Node_type::ORDER});
    for(auto i : V_o){
        GRBLinExpr expr = 0;
        for(auto j : V_ocd){
            expr += vars[j][i];
        }
        model.addConstr(expr <= 1, "do_not_visit_twice_"+itos(i));
    }

    // for order and charging station, if we arrive we must leave.
    // implementation of Eq:(5).
    const vector<int> V_oc = g.get_union_nodes({Node_type::ORDER,Node_type::CS});
    for(auto i : V_oc){
        GRBLinExpr expr = 0;
        for(auto j : V_ocs){
            expr += vars[j][i];
        }
        for (auto j : V_ocd) {
            expr -= vars[i][j];
        }
        model.addConstr(expr == 0, "continue_travel_"+itos(i));
    }



    // travel_time variable;
    GRBVar *time_vars = new GRBVar[num_of_nodes];
    for ( int i = 0; i < num_of_nodes; i++){
        time_vars[i] = model.addVar(g.get_query_start_time(),  g.get_query_end_time(), 0,
                                    GRB_CONTINUOUS, "t_"+itos(i));
    }
    //set source node to start time
    // implementation of Eq:(6).
    time_vars[g.get_s_nodes()].set(GRB_DoubleAttr_UB, g.get_query_start_time());
    time_vars[g.get_s_nodes()].set(GRB_DoubleAttr_LB, g.get_query_start_time());
    model.addConstr(time_vars[g.get_d_nodes()] <= g.get_query_end_time(), "destination_trav_time");

    // add constraint such that the travel time must respect each order's time window.
    // implementation of Eq:(7).
    for(auto i : V_o){
        model.addConstr(time_vars[i] <= g.get_order_end_time(i), "order_time_window_start_"+itos(i));
        model.addConstr(time_vars[i] >= g.get_order_start_time(i), "order_time_window_end_"+itos(i));
    }

    // make sure each visit has enough time to complete the service and travel between edges.
    // implementation of Eq:(8).
    for(auto i : V_ocs){
        for(auto j : V_ocd){
            GRBLinExpr expr = time_vars[i] + (g.get_service_time(i) + g.get_edge_time(i,j)) * vars[i][j] -
                              bigM * (1 - vars[i][j]);
            model.addConstr(expr <= time_vars[j], "travel_time_"+itos(i)+"_"+itos(j));
        }
    }


    const vector<int> V_c = g.get_union_nodes({Node_type::CS});
    vector<int>V_c_mapper(num_of_nodes,-1);
    for( int i = 0 ; i < V_c.size(); i++ ){
        V_c_mapper[V_c[i]] = i;
    }

    int number_of_time_bucket = floor((q.time_end - q.time_start)/time_bucket);


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


    // charging must not happen before arrival previous station.
    // implementation of Eq:(9).
    for (auto i  : V_c){
        for (auto j :  V_ocd) {
            for (int t = 0 ; t < number_of_time_bucket; t ++){
                GRBLinExpr expr = q.time_start + (t+1) * time_bucket * (charging_vars[V_c_mapper[i]][t] + discharging_vars[V_c_mapper[i]][t])
                                  + g.get_edge_time(i,j) * vars[i][j] -  bigM * (1 - vars[i][j]);
                model.addConstr(expr  <= time_vars[j], "charging_station_travel_time_"+itos(i)+"_"+itos(j));
            }
        }
    }


    // order - working hour constraints;
    for(auto i : V_o){
        model.addConstr(time_vars[i] >= q.working_time_start, "order_working_start_constraint_"+itos(i));
        model.addConstr(time_vars[i] + g.get_service_time(i)<= q.working_time_end ,"order_working_end_constraint_"+itos(i));
    }

    for (auto i  : V_c){
        if(!g.is_sd_charging_station(i)) {
            for (int t = 0; t < number_of_time_bucket; t++) {
                GRBLinExpr expr = q.time_start + (t + 1) * time_bucket *
                                                 (charging_vars[V_c_mapper[i]][t] + discharging_vars[V_c_mapper[i]][t]);
                model.addConstr(expr <= q.working_time_end,
                                "working_end_charging_station_travel_time_" + itos(i) + "_" + itos(g.get_d_nodes()));
            }
            model.addConstr(time_vars[i] >= q.working_time_start, "working_start_charging_station_travel_time_"+itos(i));
        }
//        else{
//            GRBLinExpr expr = time_vars[i] - bigM*(1-vars[i][g.get_d_nodes()]);
//            model.addConstr(expr <= q.working_time_end, "must arrive before work finished");
//            int a  = g.get_d_nodes();
//            GRBLinExpr expr2 = time_vars[g.get_d_nodes()] - bigM*vars[i][g.get_d_nodes()];
//            model.addConstr(expr2 <= q.working_time_end, "must arrive before work finished2");
//        }
    }

    // must not charging before arrive;
    // implementation of Eq:(10).
    for (auto i  : V_c){
//        for (int n = 0; n < 1; n++) {
            for (int t = 0 ; t < number_of_time_bucket; t ++){
                GRBLinExpr lhs = time_vars[i] - q.time_start - t* time_bucket ;
                GRBLinExpr rhs = bigM * (1 - charging_vars[V_c_mapper[i]][t] - discharging_vars[V_c_mapper[i]][t] ) ;
                model.addConstr(lhs  <= rhs, "not_charging_before_arrival_"+itos(i));
            }
//        }
    }




    GRBVar *battery_vars = new GRBVar[num_of_nodes];
    for ( int i = 0; i < num_of_nodes; i++){
        battery_vars[i] = model.addVar(0, ev.battery_capacity, 0,
                                       GRB_CONTINUOUS, "b_"+itos(i));
    }
    // implementation of Eq:(11).
    battery_vars[g.get_s_nodes()].set(GRB_DoubleAttr_UB, q.battery_start);
    battery_vars[g.get_s_nodes()].set(GRB_DoubleAttr_LB, q.battery_start);
    // implementation of Eq:(12).
    model.addConstr(battery_vars[g.get_d_nodes()] >= q.battery_end, "destination_battery_constraint");


    // make sure each traversal between edges has enough of battery.
    // implementation of Eq:(13).
    const vector<int> V_os = g.get_union_nodes({Node_type::ORDER,Node_type::SOURCE});
    for (auto i : V_os){
        for (auto j : V_ocd){
            GRBLinExpr lhs = battery_vars[j];
            GRBLinExpr rhs = battery_vars[i] - (g.get_edge_distance(i,j)* ev.driving_efficiency +
                    g.get_service_energy(i)) * vars[i][j] + bigM*(1 - vars[i][j]);
            model.addConstr(lhs  <= rhs, "energy_consume_"+itos(i)+"_"+itos(j));
        }
    }


    // make sure after charging/discharging, there are enough of battery.
    // implementation of Eq:(14).
    for (auto i : V_c){
        for (auto j : V_ocd){
            GRBLinExpr lhs = battery_vars[j];
            GRBLinExpr rhs = battery_vars[i] - g.get_edge_distance(i,j)* ev.driving_efficiency * vars[i][j]
                             + bigM*(1 - vars[i][j]);
            for (int t = 0 ; t < number_of_time_bucket; t ++){
                rhs += charging_vars[V_c_mapper[i]][t] * g.get_bucket_charging_amount(i);
                rhs -= discharging_vars[V_c_mapper[i]][t] * g.get_bucket_charging_amount(i);
            }
            model.addConstr(lhs  <= rhs, "cs_energy_consume_"+itos(i)+"_"+itos(j));
        }
    }

    // No discharging below the battery, No charging exceed the battery capacity.
    // implementation of Eq:(16) - (17).
    for (auto i : V_c){
        GRBLinExpr charging_exp = 0;
        GRBLinExpr discharging_exp = 0;
        for (int t = 0 ; t < number_of_time_bucket; t ++){
            charging_exp += charging_vars[V_c_mapper[i]][t] * g.get_bucket_charging_amount(i) ;
            discharging_exp += discharging_vars[V_c_mapper[i]][t] * g.get_bucket_charging_amount(i) ;
        }
        model.addConstr(charging_exp <= ev.battery_capacity - battery_vars[i], "maximal_charging_"+itos(i));
        model.addConstr(discharging_exp <= battery_vars[i], "maximal_discharging_"+itos(i));
    }




    // set non-visit node, to battery level 0 ;
    // implementation of Eq:(18).
    GRBVar *visited_vars = new GRBVar[num_of_nodes];
    for ( int i = 0; i < num_of_nodes; i++){
        visited_vars[i] = model.addVar(0, 1, 0,
                                       GRB_BINARY, "visited"+itos(i));
    }
    for (auto i : V_ocs){
        GRBLinExpr exp = 0;
        for(auto j : V_ocd){
            exp += vars[i][j];
        }
        model.addConstr(visited_vars[i] == exp, "visited_charging"+itos(i));
        model.addConstr(battery_vars[i] <= visited_vars[i] * ev.battery_capacity, "charging"+itos(i));
    }

    for (auto i : V_c){
        GRBLinExpr charging_exp = 0;
        GRBLinExpr discharging_exp = 0;
        for (int t = 0 ; t < number_of_time_bucket; t ++){
            charging_exp += charging_vars[V_c_mapper[i]][t];
            discharging_exp += discharging_vars[V_c_mapper[i]][t];
        }
        model.addConstr( charging_exp + discharging_exp  + bigM * ( 1 - visited_vars[i] ) >= 1, "no_visit_no_charging_"+itos(i));
    }



    // objective function:
    GRBLinExpr objective = 0;
    for (auto i : V_o){
        for (auto j : V_ocd){
            objective += vars[i][j] * g.get_profit(i);
        }
    }
    for (auto i : V_c) {
        for (int t = 0; t < number_of_time_bucket; t++) {
            objective -= charging_vars[V_c_mapper[i]][t] * g.get_bucket_charging_price(i,t) ;
            objective += discharging_vars[V_c_mapper[i]][t] * g.get_bucket_discharging_price(i,t);
        }
    }

    model.setObjective(objective , GRB_MAXIMIZE);
    model.optimize();

    if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL || model.get(GRB_IntAttr_Status) == GRB_TIME_LIMIT) {
        // print_objective(model);
        // The model has an optimal solution
        std::cout << "Optimal solution found!" << std::endl;
        if(VERBOSE) {
            double sum = 0;
            for (auto i: V_c) {
                for (int t = 0; t < number_of_time_bucket; t++) {
                    if (charging_vars[V_c_mapper[i]][t].get(GRB_DoubleAttr_X) > 0.5) {
                        cout << "charging at station: " << i << " " << t << endl;
                    }
                    if (discharging_vars[V_c_mapper[i]][t].get(GRB_DoubleAttr_X) > 0.5) {
                        cout << "discharging at station: " << i << " " << t << " cost:"
                             << g.get_bucket_discharging_price(i, t) << endl;
                        sum += g.get_bucket_discharging_price(i, t);
                    }
                }
            }
            cout << "Discharging amount: " << sum << endl;
            const vector<int> V_ocsd = g.get_union_nodes({Node_type::ORDER,Node_type::CS,Node_type::DESTINATION,Node_type::SOURCE});
            for (auto i: V_ocsd) {
                cout << "Node Id: " << i << ", Battery level: " << battery_vars[i].get(GRB_DoubleAttr_X) << endl;
                cout << "Node Id: " << i << ", Travel time: " << time_vars[i].get(GRB_DoubleAttr_X) << endl;
                cout << "Node Id: " << i << ", Visited: " << visited_vars[i].get(GRB_DoubleAttr_X) << endl;
            }
        }
        bool a = vars[33][34].get(GRB_DoubleAttr_X) > 0.5;
        cout<<"checking variable "<< a << endl;
        print_solution(num_of_nodes,vars,time_vars,charging_vars,discharging_vars,battery_vars,g,V_c_mapper,ev,q,os);
        // Access and print variable values
    } else{
        os<< "No optimal solution found." << endl;
        os << endl;
        os << q << endl;
        os << ev << endl;
    }
}



int
main(int   argc,
     char *argv[])
{
    const char *binary_name = argv[0];

    if (argc != 4) {
        std::cerr
                << std::endl
                << "USAGE: " << binary_name
                << "<datafile.csv> <queryfile.csv> <instance>" << std::endl
                << std::endl;

        return EXIT_FAILURE;
    }
    const std::string datafile(argv[1]);
    const std::string queryfile(argv[2]);
    const std::string instance(argv[3]);



    std::ifstream file(queryfile); // Replace with your CSV file's name
    Graph g = Graph(datafile, 1,1);

    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return 0;
    }

    string lns_revisit_file = "dataset/result/small/lns/" + instance   + "_revisit.csv";
    string ga_revisit_file = "/Users/jcdu3/Desktop/Research/EOP_GA/dataset/result/small/" + instance + ".revisit";
    int vn = revisit_count(lns_revisit_file,ga_revisit_file);
    if(vn > 2){
        vn = 2;
    }
    cout << instance << " " << vn << endl;


    std::string line;
    bool first_line_skipped = false;
    int query_count = 0 ;
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

        //Graph g = Graph(datafile, q);

        GRBEnv env = GRBEnv(true);
        env.set("LogFile", "dataset/result/small/revisit/" + instance + "_mip.log");
        env.start();
        GRBModel model = GRBModel(env);
        std::ofstream output("dataset/result/small/revisit/" + instance + ".mip");
        run_MIP_model(g,q,ev,model,output,vn);
        output.close();
        query_count ++;
    }
    file.close();
    return 0;
}
