//
// Created by Bojie Shen on 23/8/2023.
//
#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <piecewiseFunction.h>
#include "utils.h"
#include <json.hpp>

using namespace std;
using json = nlohmann::json;

struct EV_setting{
    double battery_capacity;
    double driving_range;
    double driving_speed;
    double driving_efficiency;
    friend std::ostream& operator<<(std::ostream& os, const EV_setting& ev) {
        os << "EV Setting: " <<endl;
        os << "Battery Capacity: " << ev.battery_capacity << ", Driving Range: "
        << ev.driving_range << ", Driving Speed: " << ev.driving_speed <<endl;
        return os;
    }
};

struct Query{
    double x1;
    double y1;
    double x2;
    double y2;
    double time_start;
    double time_end;
    double working_time_start;
    double working_time_end;
    double battery_start;
    double battery_end;
    friend std::ostream& operator<<(std::ostream& os, const Query& q) {
        os  << "Query Setting: " <<endl;
        os  << "x1: " << q.x1 << ", y1: "<< q.y1 << ", x2: "<< q.x2 << ", y2: " << q.y2
            << ", Time Start: "<< q.time_start << ", Time End: "<< q.time_end
            << ", Working Time Start: "<< q.working_time_start << ", Working Time End: "<< q.working_time_end
            <<", Battery Start: " << q.battery_start << ", Battery End: " <<q.battery_end <<endl;
        return os;
    }
};

struct Order {
    double start_x;
    double start_y;
    double dest_x;
    double dest_y;
    double time_start;
    double time_end;
    double profit;
    double service_time;
    double service_energy;
    double service_distance;
};

struct Charging_station {
    double x;
    double y;
    PiecewiseConstantFunction charging_price;
    PiecewiseConstantFunction discharging_price;
    double kilowatts_per_hours;
    double bucket_charging_amount;
    vector<double> bucket_charging_price;
    vector<double> bucket_discharging_price;
    bool sd_charging;
};

struct SD_query {
    double x;
    double y;
    double time_start;
    double time_end;
};

union Node_object{
    Order* order;
    Charging_station* cs;
    SD_query* sd;
};

enum Node_type { SOURCE, ORDER, CS, DESTINATION};

struct Node {
    Node_type type;
    Node_object object;
    int graph_id;

    bool operator==(const Node& other) const {
        return graph_id == other.graph_id;
    }

    friend std::ostream& operator<<(std::ostream& os, const Node& node) {
        switch (node.type) {
            case Node_type::ORDER:
                os<< "Node type : order, "
                     "time window: [ "<< node.object.order->time_start << " , " << node.object.order->time_end<< " ], "
                     "profit: "<< node.object.order->profit << " service time: "<< node.object.order->service_time
                        << " service energy: "<< node.object.order->service_energy <<", Node ID: " << node.graph_id;
                break;
            case Node_type::CS:
                os<< "Node type : charging station, Node ID: " << node.graph_id;
                break;
            case Node_type::DESTINATION:
                os<< "Node type : destination, "
                     "time window: [ " <<node.object.sd->time_start <<" , "<< node.object.sd->time_end << " ]";
                break;
            default:
                os<< "Node type : source; "
                     "time window: [ " <<node.object.sd->time_start <<" , "<< node.object.sd->time_end << " ]";
        }
        return os;
    }
};

struct Edge {
    double travel_time;
    double travel_distance;
};

struct Decision{
    Node* node;
    double arrival_time;
    double battery_level;
    int charging_begin;
    int charging_end;
    double latest_arrive;
    double earliest_departure;
    vector<bool> charging_plan;
    vector<bool> discharging_plan;
    double battery_fwd;
    double battery_bwd;

    bool operator == (const Decision& other) const {
        return node == other.node &&arrival_time ==other.arrival_time && battery_level == other.battery_level ;
    }
};

class Graph {

public:
    //Graph(string filename,const Query& q) { load_graph(filename,q);}
    Graph(string filename, double charging_factor, double watts_factor) { load_graph(filename, charging_factor, watts_factor);}
    Graph(string order_file, int grid_index) {
        load_order(order_file, 1,1);
        load_charging_station(1, 1);
    }

    double parse_price(const json& entry) {
        if (!entry.contains("price")) {
            throw std::runtime_error("Missing price field");
        }

        const auto& price_entry = entry.at("price");
        if (price_entry.is_number()) {
            return price_entry.get<double>();
        } else if (price_entry.is_string()) {
            std::string price_str = price_entry.get<std::string>();
            if (price_str == "Free" || price_str == "free") {
                return 0.0;  // Treat "Free" as 0
            } else {
                throw std::runtime_error("Non-numeric price string: " + price_str);
            }
        } else {
            throw std::runtime_error("Invalid type for price field");
        }
    }
    void load_charging_station(double charging_factor, double watts_factor){
        charging_scale_factor = charging_factor;
        charging_watts = watts_factor;
        std::vector<std::pair<int, double>> discharging_prices;  // time in minutes, price
        string discharging_file = "/Users/jcdu3/Desktop/Research/goldi-ridehailing/Goldi_EV/dataset/cs/discharging_price_jan1.csv";
        string filename = "/Users/jcdu3/Desktop/Research/goldi-ridehailing/Goldi_EV/dataset/cs/charging_stations_filtered.json";
        std::ifstream file(discharging_file);
        std::string line;
        std::getline(file, line); // Skip header if exists
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string time_str, price_str;
            std::getline(ss, time_str, ',');
            std::getline(ss, price_str, ',');

            int time = std::stoi(time_str);
            double price = std::stod(price_str);
            //cout << time << "," << price << endl;
            discharging_prices.emplace_back(time, price);
        }


        std::ifstream inFile(filename);
        if (!inFile.is_open()) {
            std::cerr << "Error: Could not open file " << filename << "\n";
        }

        json data;
        try {
            inFile >> data;
        } catch (const std::exception& e) {
            std::cerr << "Error: Failed to parse JSON - " << e.what() << "\n";

        }
        cout << "Number of charging stations loaded: " << data.size() << endl;
        for (const auto& entry : data) {
            try {
                double lon = entry.at("longitude").get<double>();
                double lat = entry.at("latitude").get<double>();
                double rate = entry.at("charging_rate_kw").get<double>();
                double price = parse_price(entry);

                Charging_station* cs = new Charging_station  {lon,lat};
                cs->charging_price.addSegment(charging_factor * price,0);
                cs->kilowatts_per_hours = watts_factor * rate;
                cs->sd_charging = false;
                for (const auto& [time, price] : discharging_prices) {
                    cs->discharging_price.addSegment(charging_factor * price, time);
                }
                raw_nodes.push_back(Node{
                        Node_type::CS,
                        Node_object{
                                .cs = cs  // Initialize the SD_query member
                        }
                });
            } catch (const std::exception& e) {
                std::cerr << "Warning: Skipping invalid entry - " << e.what() << "\n";
            }
        }


    }

    void load_graph (string filename, double charging_factor, double watts_factor){
        //query = q;
        std::ifstream file(filename); // Replace with your CSV file's name
        charging_scale_factor = charging_factor;
        charging_watts = watts_factor;

        if (!file.is_open()) {
            std::cerr << "Failed to open the file." << std::endl;
            return;
        }

        std::string line;
        bool first_line_skipped = false;
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
            if (values[1] == "o") {
                raw_nodes.push_back(Node{Node_type::ORDER, new Order { stod(values[2]),stod(values[3]),
                                                                   stod(values[4]),stod(values[5]),
                                                                   stod(values[6]), stod(values[7]),0,stod(values[9]),0,stod(values[8])}}
                );
            } else if (values[1] == "c") {
                Charging_station* cs = new Charging_station  {stod(values[2]),stod(values[3])};
                cs->charging_price.addSegment(charging_scale_factor * stod(values[11]),0);

                if(stod(values[11]) < 0.117){
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]),0);
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]), 1320);
                    cs->discharging_price.addSegment(charging_factor *stod(values[11]), 960);
                }
                else{
                    //cs->discharging_price.addSegment(charging_factor *0.125, 0);
                    //cs->discharging_price.addSegment(charging_factor *0.125, 1320);
                    cs->discharging_price.addSegment(charging_factor *0.117, 960);
                }

                if(stod(values[11]) < 0.061){
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]), 420);
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]), 1260);
                    cs->discharging_price.addSegment(charging_factor *stod(values[11]), 0);
                    cs->discharging_price.addSegment(charging_factor *stod(values[11]), 1260);

                }
                else{
                    cs->discharging_price.addSegment(charging_factor *0.061, 420);
                    cs->discharging_price.addSegment(charging_factor *0.061, 1260);
                }

                if(stod(values[11]) < 0.043){
                    cs->discharging_price.addSegment(charging_factor *stod(values[11]), 600);
                }
                else{
                    cs->discharging_price.addSegment(charging_factor *0.043, 600);
                }


//                cs->discharging_price.addSegment(charging_scale_factor * 0.125, 0);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.048, 420);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.102, 900);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.048, 1260);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.125, 1320);
                cs->kilowatts_per_hours = charging_watts * stod(values[10]);
                cs->sd_charging = false;
                raw_nodes.push_back(Node{
                        Node_type::CS,
                        Node_object{
                                .cs = cs  // Initialize the SD_query member
                        }
                });
            }
        }
        // source and destination charging station can not be initialised here, as we dont have query defined here.
        file.close();
    }

    void load_order (string filename, double charging_factor, double watts_factor){
        //query = q;
        std::ifstream file(filename); // Replace with your CSV file's name
        charging_scale_factor = charging_factor;
        charging_watts = watts_factor;

        if (!file.is_open()) {
            std::cerr << "Failed to open the file." << std::endl;
            return;
        }

        std::string line;
        bool first_line_skipped = false;
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
            if (values[1] == "o") {
                raw_nodes.push_back(Node{Node_type::ORDER, new Order { stod(values[2]),stod(values[3]),
                                                                       stod(values[4]),stod(values[5]),
                                                                       stod(values[6]), stod(values[7]),stod(values[10]),stod(values[9]),0,stod(values[8])}}
                );
            }
        }
        // source and destination charging station can not be initialised here, as we dont have query defined here.
        file.close();
    }
    void load_graph_v1g (string filename, double charging_factor, double watts_factor){
        //query = q;
        std::ifstream file(filename); // Replace with your CSV file's name
        charging_scale_factor = charging_factor;
        charging_watts = watts_factor;

        if (!file.is_open()) {
            std::cerr << "Failed to open the file." << std::endl;
            return;
        }

        std::string line;
        bool first_line_skipped = false;
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
            if (values[1] == "o") {
                raw_nodes.push_back(Node{Node_type::ORDER, new Order { stod(values[2]),stod(values[3]),
                                                                       stod(values[4]),stod(values[5]),
                                                                       stod(values[6]), stod(values[7]),0,stod(values[9]),0,stod(values[8])}}
                );
            } else if (values[1] == "c") {
                Charging_station* cs = new Charging_station  {stod(values[2]),stod(values[3])};
                cs->charging_price.addSegment(charging_scale_factor * stod(values[11]),0);

                if(stod(values[11]) < 0.117){
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]),0);
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]), 1320);
                    cs->discharging_price.addSegment(charging_factor *0, 960);
                }
                else{
                    //cs->discharging_price.addSegment(charging_factor *0.125, 0);
                    //cs->discharging_price.addSegment(charging_factor *0.125, 1320);
                    cs->discharging_price.addSegment(charging_factor *0, 960);
                }

                if(stod(values[11]) < 0.061){
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]), 420);
                    //cs->discharging_price.addSegment(charging_factor *stod(values[11]), 1260);
                    cs->discharging_price.addSegment(charging_factor *0, 0);
                    cs->discharging_price.addSegment(charging_factor *0, 1260);

                }
                else{
                    cs->discharging_price.addSegment(charging_factor *0, 420);
                    cs->discharging_price.addSegment(charging_factor *0, 1260);
                }

                if(stod(values[11]) < 0.043){
                    cs->discharging_price.addSegment(charging_factor *0, 600);
                }
                else{
                    cs->discharging_price.addSegment(charging_factor *0, 600);
                }


//                cs->discharging_price.addSegment(charging_scale_factor * 0.125, 0);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.048, 420);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.102, 900);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.048, 1260);
//                cs->discharging_price.addSegment(charging_scale_factor * 0.125, 1320);
                cs->kilowatts_per_hours = charging_watts * stod(values[10]);
                cs->sd_charging = false;
                raw_nodes.push_back(Node{
                        Node_type::CS,
                        Node_object{
                                .cs = cs  // Initialize the SD_query member
                        }
                });
            }
        }
        // source and destination charging station can not be initialised here, as we dont have query defined here.
        file.close();
    }


    void initialise_graph(const Query& q, const EV_setting& ev_setting, int tb, int vn, double factor, bool not_melbourne = false){
        nodes.clear();
        edges.clear();
        order_nodes.clear();
        cs_nodes.clear();
        virtual_nodes.clear();
        ev = ev_setting;
        query = q;

        time_bucket = tb;
        virtual_nodes_size = vn;
        order_factor = factor;

        nodes.push_back(Node{
                Node_type::SOURCE,
                Node_object{
                        .sd = new SD_query{query.x1, query.y1, query.time_start, query.time_end}  // Initialize the SD_query member
                }
        });
        if(not_melbourne){
            append_order_ny();
        }
        else{
            append_order();
        }

        append_charging_station();
        nodes.push_back(Node{
                Node_type::DESTINATION,
                Node_object{
                        .sd = new SD_query{query.x2, query.y2, query.time_start, query.time_end}  // Initialize the SD_query member
                }
        });
        create_edges();
        create_nodes_index();
        init_forbidden_arc();
    }

    double compute_profit(const Node& n ){
        double hourly_start = n.object.order->time_start /60;
//        double travel_dist = haversineDistance(n.object.order->start_x, n.object.order->start_y,
//                                               n.object.order->dest_x,n.object.order->dest_y);
        double travel_dist = n.object.order->service_distance;
        double base_price, time_price, km_price = 0;
//        if(hourly_start > 9 && hourly_start < 17){
//            base_price = 4.85;
//            km_price = travel_dist * 1.553;
//            time_price = n.object.order->service_time* 0.399;
//        }else{
//            base_price = 6.05;
//            km_price = travel_dist * 1.725;
//            time_price = n.object.order->service_time* 0.438;
//        }

        base_price  = 2.75;
        km_price = travel_dist * 1.49;
        time_price = n.object.order->service_time * 0.39;
        return order_factor * (0.7*(base_price + time_price + km_price));
    }

    void append_order_ny(){
        for( auto n : raw_nodes){
            if(n.type == Node_type::ORDER){
                double order_earliest_dep_time = n.object.order->time_start;
                double order_latest_arr_time = n.object.order->time_end + n.object.order->service_time+60;
                if(!(order_latest_arr_time <= query.working_time_start || order_earliest_dep_time >= query.working_time_end)){
                    // ev.driving_speed : km/h;
//                    n.object.order->service_time = haversineDistance(n.object.order->start_x, n.object.order->start_y,
//                                                                     n.object.order->dest_x,n.object.order->dest_y) / ev.driving_speed * 60;
                    n.object.order->service_energy = n.object.order->service_distance * ev.driving_efficiency;
//                    n.object.order->service_energy = haversineDistance(n.object.order->start_x, n.object.order->start_y,
//                                                                       n.object.order->dest_x,n.object.order->dest_y) * ev.driving_efficiency;
                    n.object.order->time_end = n.object.order->time_end + 60;
                    nodes.push_back(n);
                }
            }
        }
        cout << "Number of nodes after appending order: " << raw_nodes.size() << endl;
    }

    void append_order(){
        for( auto n : raw_nodes){
            if(n.type == Node_type::ORDER){
                double order_earliest_dep_time = n.object.order->time_start;
                double order_latest_arr_time = n.object.order->time_end + n.object.order->service_time;
                if(!(order_latest_arr_time <= query.working_time_start || order_earliest_dep_time >= query.working_time_end)){
                    // ev.driving_speed : km/h;
//                    n.object.order->service_time = haversineDistance(n.object.order->start_x, n.object.order->start_y,
//                                                                     n.object.order->dest_x,n.object.order->dest_y) / ev.driving_speed * 60;
                    n.object.order->service_energy = n.object.order->service_distance * ev.driving_efficiency;
//                    n.object.order->service_energy = haversineDistance(n.object.order->start_x, n.object.order->start_y,
//                                                                       n.object.order->dest_x,n.object.order->dest_y) * ev.driving_efficiency;
                    n.object.order->time_end = n.object.order->time_end - n.object.order->service_time;
                    n.object.order->profit = compute_profit(n);
                    nodes.push_back(n);
                }
            }
        }
    }

    void append_cs(Charging_station* cs, int number_of_bucket, Node n){
        cs->bucket_charging_amount = cs->kilowatts_per_hours * time_bucket / 60;
        cs->bucket_charging_price = vector<double>(number_of_bucket);
        cs->bucket_discharging_price = vector<double>(number_of_bucket);
        for (int t = 0 ; t < number_of_bucket; t ++){
            // price is in kwh.
            cs->bucket_charging_price[t]  = cs->charging_price.evaluate(t*time_bucket + query.time_start) *  cs->bucket_charging_amount;
            all_time_charging_price[t] = max(all_time_charging_price[t], cs->bucket_charging_price[t]);
            all_time_highest_charging_price = max(all_time_highest_charging_price, cs->charging_price.evaluate(t*time_bucket + query.time_start));
        }
        for (int t = 0 ; t < number_of_bucket; t ++){
            cs->bucket_discharging_price[t]  = cs->discharging_price.evaluate(t*time_bucket + query.time_start) *  cs->bucket_charging_amount;
            all_time_discharging_price[t] = max(all_time_discharging_price[t], cs->bucket_discharging_price[t]);
        }
        vector<int> same_vertex;
        for(int i = 0; i < virtual_nodes_size; i ++){
            nodes.push_back(n);
            same_vertex.push_back(nodes.size()-1);
        }
        virtual_nodes.push_back(same_vertex);
    }


    void append_charging_station(){
        int number_of_bucket = floor((query.time_end - query.time_start)/time_bucket);
        all_time_discharging_price = vector<double>(number_of_bucket);
        all_time_charging_price = vector<double>(number_of_bucket);
        all_time_highest_charging_price = 0;
        for( auto n : raw_nodes){
            if(n.type == Node_type::CS){
                append_cs( n.object.cs,number_of_bucket,n);
            }
        }
        Charging_station* start_cs = new Charging_station  {query.x1,query.y1};
//        start_cs->charging_price.addSegment(0.25, 0);
//        start_cs->discharging_price.addSegment(0.25, 0);
        start_cs->charging_price.addSegment(charging_scale_factor * 0.2665,0);
        start_cs->discharging_price.addSegment(charging_scale_factor * 0.2665,0);
        //start_cs->discharging_price.addSegment(charging_scale_factor * 0,0);
        start_cs->charging_price.addSegment(charging_scale_factor * 0.412,900);
        start_cs->discharging_price.addSegment(charging_scale_factor * 0.412,900);
        //start_cs->discharging_price.addSegment(charging_scale_factor * 0,900);
        start_cs->charging_price.addSegment(charging_scale_factor * 0.2665,1260);
        start_cs->discharging_price.addSegment(charging_scale_factor * 0.2665,1260);
        //start_cs->discharging_price.addSegment(charging_scale_factor * 0,1260);
        start_cs->kilowatts_per_hours = charging_watts * 7;
        start_cs->sd_charging = true;
        append_cs( start_cs,number_of_bucket,Node{
                Node_type::CS,
                Node_object{
                        .cs = start_cs  // Initialize the SD_query member
                }});
//        Charging_station* end_cs = new Charging_station  {query.x2,query.y2};
//        end_cs->charging_price.addSegment(0.25, 0);
//        end_cs->discharging_price.addSegment(0.25, 0);
//        end_cs->kilowatts_per_hours = 7;
//        end_cs->sd_charging = true;
//        append_cs( end_cs,number_of_bucket,Node{
//                Node_type::CS,
//                Node_object{
//                        .cs = end_cs  // Initialize the SD_query member
//                }});
    }

    double get_all_time_highest_charging_price(){
        return all_time_highest_charging_price;
    }

    const vector<vector<int>>& get_virtual_nodes(){
        return virtual_nodes;
    }

    Node* get_node_ptr(int node_id){
        return &nodes[node_id];
    }

//    void load_graph (string filename){
//        std::ifstream file(filename); // Replace with your CSV file's name
//
//        if (!file.is_open()) {
//            std::cerr << "Failed to open the file." << std::endl;
//            return;
//        }
//
//        std::string line;
//        bool first_line_skipped = false;
//        while (getline(file, line)) {
//            if (!first_line_skipped) {
//                first_line_skipped = true;
//                continue; // Skip the first line
//            }
//            std::vector<std::string> values;
//            std::istringstream ss(line);
//            std::string value;
//
//            while (getline(ss, value, ',')) { // Assuming comma-separated values
//                values.push_back(value);
//            }
//
//            if (values[1] == "o") {
//                nodes.push_back(Node{Node_type::ORDER, new Order { stod(values[2]),stod(values[3]),
//                                                                     stod(values[4]),stod(values[5]),
//                                                                     stod(values[6]), stod(values[7]),
//                                                                     haversineDistance(stod(values[2]),stod(values[3]),
//                                                                                       stod(values[4]),stod(values[5])) * 2,
//                                                                   haversineDistance(stod(values[2]),stod(values[3]),
//                                                                                     stod(values[4]),stod(values[5]))/60*60}}
//                                                                                       );
//            } else if (values[1] == "c") {
//                Charging_station* cs = new Charging_station  {stod(values[2]),stod(values[3]),
//                                     };
//                cs->charging_price.addSegment(stod(values[8]),0);
//                cs->discharging_price.addSegment(0.125, 0);
//                cs->discharging_price.addSegment(0.048, 420);
//                cs->discharging_price.addSegment(0.102, 900);
//                cs->discharging_price.addSegment(0.048, 1260);
//                cs->discharging_price.addSegment(0.125, 1320);
//                cs->kilowatts_per_hours = stod(values[9]);
//
//
//                cs->bucket_charging_amount = cs->kilowatts_per_hours * time_bucket / 60;
//                cs->bucket_charging_price = vector<double>(day_time / time_bucket);
//                cs->bucket_discharging_price = vector<double>(day_time / time_bucket);
//                for (int t = 0 ; t < (day_time / time_bucket); t ++){
//                    // price is in kwh.
//                    cs->bucket_charging_price[t]  = cs->charging_price.evaluate(t*time_bucket) *  cs->bucket_charging_amount;
//                }
//                for (int t = 0 ; t < (day_time / time_bucket); t ++){
//                    cs->bucket_discharging_price[t]  = cs->discharging_price.evaluate(t*time_bucket)*  cs->bucket_charging_amount;
//                }
//
//                nodes.push_back(Node{
//                        Node_type::CS,
//                        Node_object{
//                                .cs = cs  // Initialize the SD_query member
//                        }
//                });
//            }
//        }
//        file.close();
//    }



    std::tuple<double, double>  get_departure_location(const Node& n){
        switch (n.type) {
            case Node_type::ORDER:
                return std::make_tuple(n.object.order->dest_x, n.object.order->dest_y);
                break;
            case Node_type::CS:
                return std::make_tuple(n.object.cs->x, n.object.cs->y);
                break;
            default:
                return std::make_tuple(n.object.sd->x, n.object.sd->y);
        }
    }

    std::tuple<double, double>  get_arrival_location(const Node& n){
        switch (n.type) {
            case Node_type::ORDER:
                return std::make_tuple(n.object.order->start_x,n.object.order->start_y);
                break;
            case Node_type::CS:
                return std::make_tuple(n.object.cs->x, n.object.cs->y);
                break;
            default:
                return std::make_tuple(n.object.sd->x, n.object.sd->y);
        }
    }



    void create_edges(){
        int index = 0 ;
        edges.resize(nodes.size());
        for (const auto& n : nodes){
            for (const auto& m : nodes){
                double travel_distance = haversineDistance(get<0>(get_departure_location(n)),
                        get<1>(get_departure_location(n)),
                        get<0>(get_arrival_location(m)),
                        get<1>(get_arrival_location(m))
                        );
//                double euc_distance = calculate_Euclidean_distance(get<0>(get_departure_location(n)),
//                                                           get<1>(get_departure_location(n)),
//                                                           get<0>(get_arrival_location(m)),
//                                                           get<1>(get_arrival_location(m))
//                );
//                cout<< "Geo distance: " << travel_distance <<" Euclidean distance: "<<euc_distance <<endl;
                edges[index].push_back(Edge{travel_distance/ev.driving_speed*60,travel_distance});
            }
            edges[index].shrink_to_fit();
            index ++;
        }
    }

    double get_all_discharging_price (int t){
        return all_time_discharging_price[t];
    }

    void create_nodes_index(){
        int index = 0 ;
        for ( auto& n : nodes){
            n.graph_id = index;
            switch (n.type) {
                case Node_type::ORDER:
                    order_nodes.push_back(index);
                    successor_nodes.push_back(index);
                    index ++;
                    break;
                case Node_type::CS:
                    cs_nodes.push_back(index);
                    successor_nodes.push_back(index);
                    index ++;
                    break;
                default:
                    index ++;
            }
        }
        successor_nodes.push_back(get_d_nodes());
        successor_nodes.shrink_to_fit();
        order_nodes.shrink_to_fit();
        cs_nodes.shrink_to_fit();
    }

    double get_bucket_charging_amount(int node_id) const{
        return nodes[node_id].object.cs->bucket_charging_amount;
    }


    double get_profit(int node_id) const{
        switch (nodes[node_id].type) {
            case Node_type::ORDER:
                return nodes[node_id].object.order->profit;
            default:
                return 0;
        }
    }

    double get_bucket_charging_price(int node_id,int t) const{
        return nodes[node_id].object.cs->bucket_charging_price[t];
    }

    double get_bucket_discharging_price(int node_id,int t) const{
        return nodes[node_id].object.cs->bucket_discharging_price[t];
    }


    double get_service_time(int node_id){
        switch (nodes[node_id].type) {
            case Node_type::ORDER:
                return nodes[node_id].object.order->service_time;
            default:
                return 0;
        }
    }

    double get_service_energy(int node_id){
        switch (nodes[node_id].type) {
            case Node_type::ORDER:
                return nodes[node_id].object.order->service_energy;
            default:
                return 0;
        }
    }
    const vector<int>& get_successors() const{
        return  successor_nodes;
    }

    const vector<int>& get_order_nodes() const {
        return  order_nodes;
    }

    const vector<int>& get_cs_nodes() const{
        return  cs_nodes;
    }


    int get_s_nodes() const{
        return  0 ;
    }

    int get_d_nodes() const{
        return  nodes.size()-1 ;
    }


    vector<int> get_union_nodes(const vector<Node_type>& NT){
        vector<int> union_nodes;
        for(auto nt : NT){
            switch (nt) {
                case Node_type::ORDER:
                    for (auto index : get_order_nodes()){
                        union_nodes.push_back(index);
                    }
                    break;
                case Node_type::CS:
                    for (auto index : get_cs_nodes()){
                        union_nodes.push_back(index);
                    }
                    break;
                case Node_type::SOURCE:
                    union_nodes.push_back(get_s_nodes());
                    break;
                default:
                    // destination;
                    union_nodes.push_back(get_d_nodes());
            }
        }
        return union_nodes;
    }

    int get_number_of_nodes(){
        return nodes.size();
    }

    double get_edge_distance(int i, int j) const {
        return edges[i][j].travel_distance;
    }

    double get_edge_time(int i, int j) const {
        return edges[i][j].travel_time;
    }

    double get_query_start_time() const {
        return nodes[0].object.sd->time_start;
    }

    double get_query_end_time() const {
        return nodes[0].object.sd->time_end;
    }

    double get_order_start_time(int order_id) const {
        return nodes[order_id].object.order->time_start;
    }

    double get_order_end_time(int order_id) const {
        return nodes[order_id].object.order->time_end;
    }

    const Node& get_node( int node_id) const{
        return nodes[node_id];
    }

    bool is_order_node( int node_id) const{
        return nodes[node_id].type == Node_type::ORDER;
    }

    bool is_charging_station_node( int node_id) const{
        return nodes[node_id].type == Node_type::CS;
    }

    bool is_sd_charging_station(int node_id) const {
        return nodes[node_id].object.cs->sd_charging;
    }
    void print_graph() const{
        for( auto n : nodes){
            cout<< n <<endl;
        }
    }


    void init_forbidden_arc(){
        forbidden_arc.clear();
        const vector<int>& order_node = get_order_nodes();
        for(auto i : order_node){
            for(auto j : order_node){
                if( i  == j ){
                    continue;
                }
                if(nodes[i].object.order->time_start + get_edge_time(i,j)  >= nodes[j].object.order->time_end ){
                    forbidden_arc.push_back(make_tuple(i,j));
                }
            }
        }
    }

    const vector<std::tuple<int, int>>& get_forbidden_arc(){
        return forbidden_arc;
    }

    double get_max_charging_price(int t){
        return all_time_charging_price[t];
    }


    double get_accumulate_time ( int id_1 , int id_2){
        auto id_1_dep = get_departure_location(nodes[id_1]);
        auto id_1_arr = get_arrival_location(nodes[id_1]);

        auto id_2_dep = get_departure_location(nodes[id_2]);
        auto id_2_arr = get_arrival_location(nodes[id_2]);

        double t1  = haversineDistance(get<0>(id_1_dep),
                                                        get<1>(id_1_dep),
                                                        get<0>(id_2_dep),
                                                        get<1>(id_2_dep)) / ev.driving_speed*60;

        double t2  = haversineDistance(get<0>(id_1_arr),
                                       get<1>(id_1_arr),
                                       get<0>(id_2_arr),
                                       get<1>(id_2_arr)) / ev.driving_speed*60;
        return t1 + t2;
    }

    ~Graph(){
        // free all memory for each node.
        for (auto n : raw_nodes){
            if (n.type == Node_type::ORDER) {
                delete n.object.order;
            } else if (n.type == Node_type::CS) {
                delete n.object.cs;
            } else if (n.type == Node_type::SOURCE || n.type == Node_type::DESTINATION) {
                // Assuming SD_query is stored in the union for SOURCE and DESTINATION nodes
                delete n.object.sd;
            }
        }
    }

private:
    // raw data;
    vector<Node> raw_nodes;

    // filter data;
    vector<Node> nodes;
    vector<vector<Edge>> edges;
    vector<int> order_nodes;
    vector<int> cs_nodes;
    vector<int> successor_nodes;
    vector<vector<int>> virtual_nodes;
    vector<double> all_time_discharging_price;
    vector<double> all_time_charging_price;
    vector<std::tuple<int, int>>  forbidden_arc;
    Query query;
    EV_setting ev;
    int time_bucket;
    int virtual_nodes_size;

    double charging_scale_factor;
    double charging_watts;
    double all_time_highest_charging_price;
    double order_factor;
};
