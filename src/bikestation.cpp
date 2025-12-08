/* Lab05 - PCO
 * Date : 26.11.2025
 * Auteurs : Samuel Fernandez - Khelfi Amine
 */

/*  Fichier : bikestation.cpp
 *
 *  Cette classe représente ...
 */

#include "bikestation.h"
#include <pcosynchro/pcomutex.h>

BikeStation::BikeStation(int _capacity) : capacity(_capacity) {}

BikeStation::~BikeStation() {
    ending();
}

void BikeStation::putBike(Bike* _bike) {
    if (!_bike) return; // Sécurité

    mutex.lock();

    if (endSimulation)
    {
        mutex.unlock();
        return;
    }

    // While car moniteur Mesa. Si aucun slot de libre
    while (nbBikes() >= capacity && !endSimulation)
    {
        slots_available.wait(&mutex);
    }

    if (endSimulation)
    {
        mutex.unlock();
        return;
    }

    // Déposer vélo
    size_t type = _bike->bikeType;
    storage[type].push_back(_bike);

    // On signale vélo libre
    bikes_of_type_available[type].notifyOne();

    mutex.unlock();
}

Bike* BikeStation::getBike(size_t _bikeType) {
    mutex.lock();

    // Si vélo souhaité pas dispo
    while (storage[_bikeType].empty() && !endSimulation)
    {
        bikes_of_type_available[_bikeType].wait(&mutex);
    }

    // Récupération vélo
    Bike* bike = storage[_bikeType].front();
    storage[_bikeType].pop_front();

    // On signale slot libre
    slots_available.notifyOne();

    mutex.unlock();

    return bike;
}

std::vector<Bike*> BikeStation::addBikes(std::vector<Bike*> _bikesToAdd) {

    mutex.lock();
    std::vector<Bike*> rejectedBikes;

    // Si la station est en cours d'arrêt, on rejette tous les vélos
    if (endSimulation)
    {
        mutex.unlock();
        return _bikesToAdd;
    }

    for (Bike* bike : _bikesToAdd)
    {
        // Il y a de la place
        if (nbBikes() < capacity)
        {
            size_t type = bike->bikeType;
            storage[type].push_back(bike);
            bikes_of_type_available[type].notifyOne();
        }
        else
        {
            // Pas de place, on retourne le vélo au Van
            rejectedBikes.push_back(bike);
        }
    }

    mutex.unlock();
    return rejectedBikes;
}

std::vector<Bike*> BikeStation::getBikes(size_t _nbBikes) {
    mutex.lock();

    std::vector<Bike*> retrievedBikes;
    size_t count = 0;

    // On parcourt les types de 0 à N
    for (size_t type = 0; type < Bike::nbBikeTypes; ++type)
    {
        // Tant qu'on n'a pas atteint la limite et qu'il y a des vélos de ce type
        while (count < _nbBikes && !storage[type].empty())
        {
            Bike* bike = storage[type].front();
            storage[type].pop_front();
            retrievedBikes.push_back(bike);
            count++;
        }

        if (count >= _nbBikes) break;
    }

    // Comme on a pu libérer beaucoup de places, on réveille tout le monde en attente de slot.
    if (count > 0) {
        slots_available.notifyAll();
    }

    mutex.unlock();
    return retrievedBikes;
}

size_t BikeStation::countBikesOfType(size_t type) const {

    if (type >= Bike::nbBikeTypes)
        return -1;

    return storage[type].size();
}

// Pas besoin de lock, utilisé que dans un contexte ou le mutex est déjà lock
size_t BikeStation::nbBikes() {

    size_t totalBikes = 0;

    // Récupération du nb d'éléments pour chaque type
    for (size_t type = 0; type < Bike::nbBikeTypes; ++type)
    {
        totalBikes += storage[type].size();
    }


    return totalBikes;
}

size_t BikeStation::nbSlots() {
    return capacity;
}

void BikeStation::ending() {
    mutex.lock();

    endSimulation = true;

    // Réveiller tous les threads en attente
    for (size_t type = 0; type < Bike::nbBikeTypes; ++type)
    {
        bikes_of_type_available[type].notifyAll();
    }

    slots_available.notifyAll();

    mutex.unlock();
}
