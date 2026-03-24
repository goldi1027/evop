//
// Created by Bojie Shen on 23/8/2023.
//
#include <cmath>
#pragma once
const double EarthRadius = 6371.0; // Earth's radius in kilometers
const double Epsilon = 0.000001;

double degreesToRadians(double degrees) {
    return degrees * M_PI / 180.0;
}

double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    double dLat = degreesToRadians(lat2 - lat1);
    double dLon = degreesToRadians(lon2 - lon1);

    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(degreesToRadians(lat1)) * std::cos(degreesToRadians(lat2)) *
               std::sin(dLon / 2) * std::sin(dLon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    double distance = EarthRadius * c;
    return distance;
}


double calculate_Euclidean_distance(double x1, double y1, double x2, double y2) {
    return std::sqrt(std::pow(x2 - x1, 2) + std::pow(y2 - y1, 2));
}


int random_integer(int min, int max){
    if(min == max ) {
        return max;
    }else {
        return min + (rand() % (max - min));
    }
}

