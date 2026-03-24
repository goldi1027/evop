//
// Created by Bojie Shen on 23/8/2023.
//
#pragma once
#include <iostream>
#include <vector>

using namespace std;

class PiecewiseConstantFunction {
public:
    PiecewiseConstantFunction() {}
    ~PiecewiseConstantFunction() {
        // The destructor clears the segments vector
        segments.clear();
    }
    void addSegment(double value, double start) {
        segments.push_back({value, start});
    }

    double evaluate(double x) const {
        for(int i = segments.size()-1; i>=0; i --){
            if (x >= segments[i].start) {
                return segments[i].value;
            }
        }
        return 0.0; // Default value if x is before the first segment
    }

private:
    struct Segment {
        double value;
        double start;
    };

    vector<Segment> segments;
};