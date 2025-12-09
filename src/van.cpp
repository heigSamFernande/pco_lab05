/* Lab05 - PCO
 * Date : 09.12.2025
 * Auteurs : Samuel Fernandez - Khelfi Amine
 */

/* Fichier : van.cpp
 * Cette classe représente le véhicule de maintenance (van) chargé du rééquilibrage des vélos.
 * Il exécute une boucle continue de tâches : charger des vélos au dépôt, visiter les stations (sites) pour déposer
 * les vélos manquants ou retirer les excédentaires, puis retourner au dépôt pour vider son chargement restant.
 * L'objectif est de maintenir un niveau de stock adéquat dans toutes les stations.
 */

#include "van.h"

BikingInterface* Van::binkingInterface = nullptr;
std::array<BikeStation*, NB_SITES_TOTAL> Van::stations{};

Van::Van(unsigned int _id)
    : id(_id),
      currentSite(DEPOT_ID)
{}

void Van::run() {
    while (true) {
        // Vérifier si un arrêt a été demandé pour ce thread
        if (auto* self = PcoThread::thisThread(); self && self->stopRequested()) {
            break;
        }

        // 1. Charger la camionnette au dépôt
        loadAtDepot();

        // 2. Parcourir tous les sites pour les équilibrer
        for (unsigned int s = 0; s < NBSITES; ++s) {
            driveTo(s);
            balanceSite(s);
        }

        // 3. Retourner au dépôt et vider la camionnette
        returnToDepot();

        // 4. Faire une pause (durée constante) via PcoThread
        PcoThread::usleep(2'000'000); // 2 secondes
    }
    log("Van s'arrête proprement");
}

void Van::setInterface(BikingInterface* _binkingInterface){
    binkingInterface = _binkingInterface;
}

void Van::setStations(const std::array<BikeStation*, NB_SITES_TOTAL>& _stations) {
    stations = _stations;
}

void Van::log(const QString& msg) const {
    if (binkingInterface) {
        binkingInterface->consoleAppendText(0, msg);
    }
}

void Van::driveTo(unsigned int _dest) {
    if (currentSite == _dest)
        return;

    unsigned int travelTime = randomTravelTimeMs();
    if (binkingInterface) {
        binkingInterface->vanTravel(currentSite, _dest, travelTime);
    }

    currentSite = _dest;
}

void Van::loadAtDepot() {
    driveTo(DEPOT_ID);

    // D = nombre de vélos au dépôt
    size_t D = stations[DEPOT_ID]->nbBikes();

    // a = nombre de vélos déjà dans la camionnette
    size_t a = cargo.size();

    // On ne dépasse jamais la capacité de la camionnette
    size_t capacityLeft = (VAN_CAPACITY > a) ? (VAN_CAPACITY - a) : 0;
    if (capacityLeft == 0 || D == 0) {
        if (binkingInterface) {
            binkingInterface->setBikes(DEPOT_ID, stations[DEPOT_ID]->nbBikes());
        }
        return;
    }

    // Charger au plus min(2, D) vélos, sans dépasser la capacité restante
    size_t toLoad = std::min<size_t>({static_cast<size_t>(2), D, capacityLeft});
    if (toLoad > 0) {
        std::vector<Bike*> loaded = stations[DEPOT_ID]->getBikes(toLoad);
        for (Bike* b : loaded) {
            if (b) {
                cargo.push_back(b);
            }
        }
    }

    if (binkingInterface) {
        binkingInterface->setBikes(DEPOT_ID, stations[DEPOT_ID]->nbBikes());
    }
}


void Van::balanceSite(unsigned int _site)
{
    if (_site >= NBSITES) {
        return;
    }

    BikeStation* station = stations[_site];
    if (!station) {
        return;
    }

    // a = nombre de vélos dans la camionnette
    size_t a = cargo.size();

    // Vi = nombre de vélos sur le site i
    size_t Vi = station->nbBikes();

    const size_t target = BORNES - 2; // cible par site

    // 2a. Si Vi > B-2 : retirer des vélos du site vers la camionnette
    if (Vi > target && a < VAN_CAPACITY) {
        size_t surplus = Vi - target;
        size_t capacityLeft = VAN_CAPACITY - a;
        size_t c = std::min(surplus, capacityLeft);

        if (c > 0) {
            std::vector<Bike*> taken = station->getBikes(c);
            for (Bike* b : taken) {
                if (b) {
                    cargo.push_back(b);
                    ++a;
                }
            }
            Vi = station->nbBikes();
        }
    }

    // 2b. Si Vi < B-2 : déposer des vélos depuis la camionnette
    if (Vi < target && a > 0) {
        size_t deficit = target - Vi;
        size_t c = std::min(deficit, a); // nombre de vélos à déposer

        std::vector<Bike*> toAdd;
        toAdd.reserve(c);

        size_t cDeposited = 0;

        // Priorité : remplir les types manquants
        for (size_t t = 0; t < Bike::nbBikeTypes && cDeposited < c; ++t) {
            if (station->countBikesOfType(t) == 0) {
                Bike* b = takeBikeFromCargo(t);
                if (b) {
                    toAdd.push_back(b);
                    ++cDeposited;
                    --a;
                }
            }
        }

        // Puis, compléter avec n'importe quels vélos de la camionnette
        while (cDeposited < c && a > 0 && !cargo.empty()) {
            Bike* b = cargo.back();
            cargo.pop_back();
            toAdd.push_back(b);
            ++cDeposited;
            --a;
        }

        if (!toAdd.empty()) {
            // addBikes peut éventuellement rejeter des vélos si la station est pleine
            std::vector<Bike*> rejected = station->addBikes(std::move(toAdd));
            // Les vélos rejetés retournent dans la camionnette
            for (Bike* b : rejected) {
                if (b) {
                    cargo.push_back(b);
                    ++a;
                }
            }
        }
    }

    // Mise à jour de la GUI pour le site et le dépôt
    if (binkingInterface) {
        binkingInterface->setBikes(_site, stations[_site]->nbBikes());
        binkingInterface->setBikes(DEPOT_ID, stations[DEPOT_ID]->nbBikes());
    }
}

void Van::returnToDepot() {
    driveTo(DEPOT_ID);

    size_t cargoCount = cargo.size();

    if (cargoCount > 0) {
        // 3. Vider la camionnette au dépôt
        BikeStation* depot = stations[DEPOT_ID];
        if (depot) {
            std::vector<Bike*> toAdd = std::move(cargo);
            cargo.clear();

            std::vector<Bike*> rejected = depot->addBikes(std::move(toAdd));
            // Les vélos rejetés (si la capacité du dépôt est atteinte) restent dans la camionnette
            for (Bike* b : rejected) {
                if (b) {
                    cargo.push_back(b);
                }
            }
        }
    }

    if (binkingInterface) {
        binkingInterface->setBikes(DEPOT_ID, stations[DEPOT_ID]->nbBikes());
    }
}

Bike* Van::takeBikeFromCargo(size_t type) {
    for (size_t i = 0; i < cargo.size(); ++i) {
        if (cargo[i]->bikeType == type) {
            Bike* bike = cargo[i];
            cargo[i] = cargo.back();
            cargo.pop_back();
            return bike;
        }
    }
    return nullptr;
}

