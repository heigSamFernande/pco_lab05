/* Lab05 - PCO
 * Date : 09.12.2025
 * Auteurs : Samuel Fernandez - Khelfi Amine
 */

/* Fichier : person.h
 * Cette classe représente une personne simulant l'utilisation du système de partage de vélos.
 * Elle effectue des boucles continues de trajets : prendre un vélo sur un site,
 * rouler vers une destination, déposer le vélo, marcher vers un autre site, et prendre un vélo pour rentrer
 * à sa station de départ (domicile).
 * Elle interagit avec les stations de vélos (BikeStation).
 */

#include "person.h"
#include "bike.h"
#include <random>

BikingInterface* Person::binkingInterface = nullptr;
std::array<BikeStation*, NB_SITES_TOTAL> Person::stations{};


Person::Person(unsigned int _id) : id(_id), homeSite(0), currentSite(0) {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, Bike::nbBikeTypes - 1);
    preferredType = dist(rng);

    if (binkingInterface) {
        log(QString("Person %1, préfère type %2")
                .arg(id).arg(preferredType));
    }
}

void Person::setStations(const std::array<BikeStation*, NB_SITES_TOTAL>& _stations){
    Person::stations = _stations;
}

void Person::setInterface(BikingInterface* _binkingInterface) {
    binkingInterface = _binkingInterface;
}

void Person::run() {
    while (true) {
        // Attendre qu'un vélo disponible et le prendre
        Bike* bike = takeBikeFromSite(currentSite);

        // Si nullptr est retourné -> Simulation terminée
        if (bike == nullptr)
            break;

        // Choisir un autre site j != i
        unsigned int destinationSite = chooseOtherSite(currentSite);

        // Aller au site j avec le vélo
        bikeTo(destinationSite, bike);

        // Attendre qu'une borne du site devienne libre et libérer son vélo
        depositBikeAtSite(destinationSite, bike);

        // Aller à pied à un autre site k
        unsigned int nextSite = chooseOtherSite(currentSite);
        walkTo(nextSite);
    }
}

Bike* Person::takeBikeFromSite(unsigned int _site) {

    Bike* bike = stations[_site]->getBike(preferredType);

    // Si bike est nullptr -> Ffin simulation
    if (bike == nullptr)
        return nullptr;

    // Mise à jour de l'interface graphique
    if (binkingInterface)
        binkingInterface->setBikes(_site, stations[_site]->nbBikes());

    return bike;
}

void Person::depositBikeAtSite(unsigned int _site, Bike* _bike) {
    // Vérification de sécurité
    if (_bike == nullptr)
        return;

    // Déposer le vélo à la station
    stations[_site]->putBike(_bike);

    // Mise à jour de l'interface graphique
    if (binkingInterface)
        binkingInterface->setBikes(_site, stations[_site]->nbBikes());
}

void Person::bikeTo(unsigned int _dest, Bike* _bike) {
    unsigned int t = bikeTravelTime();
    if (binkingInterface) {
        binkingInterface->travel(id, currentSite, _dest, t);
    }
    currentSite = _dest;
}

void Person::walkTo(unsigned int _dest) {
    unsigned int t = walkTravelTime();
    if (binkingInterface) {
        binkingInterface->walk(id, currentSite, _dest, t);
    }
    currentSite = _dest;
}

unsigned int Person::chooseOtherSite(unsigned int _from) const {
    return randomSiteExcept(NBSITES, _from);
}

unsigned int Person::bikeTravelTime() const {
    return randomTravelTimeMs() + 1000;
}

unsigned int Person::walkTravelTime() const {
    return randomTravelTimeMs() + 2000;
}

void Person::log(const QString& msg) const {
    if (binkingInterface) {
        binkingInterface->consoleAppendText(id, msg);
    }
}

