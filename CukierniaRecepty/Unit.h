#pragma once
#include <Wt/Dbo/Dbo>
#include "database.h"

class Unit {
   public:
    Wt::WString name = "Nieznana nazwa";
    Wt::Dbo::dbo_traits<Unit>::IdType baseUnitID = Wt::Dbo::dbo_traits<Unit>::invalidId();
    double quantity = 1.0;
    int ownerID = -1;

    template <class Action>
    void persist(Action& action) {
        Wt::Dbo::field(action, name, "name");
        Wt::Dbo::field(action, baseUnitID, "base_unit_id");
        Wt::Dbo::field(action, quantity, "quantity");
        Wt::Dbo::field(action, ownerID, "owner_id");
    }

    // returns all the parent units, self included, to the root unit. Root unit is last in the vector.
    static std::vector<Wt::Dbo::ptr<Unit>> pathToTheRoot(Database& db, Wt::Dbo::dbo_traits<Unit>::IdType unit) {
        auto results = std::vector<Wt::Dbo::ptr<Unit>>{};

        auto currentID = unit;
        while (currentID != Wt::Dbo::dbo_traits<Unit>::invalidId()) {
            Wt::Dbo::ptr<Unit> currentUnit = db.find<Unit>().where("id = ?").bind(currentID);
            if (currentUnit.id() == Wt::Dbo::dbo_traits<Unit>::invalidId()) {
                return results;
            }
            results.push_back(currentUnit);
            currentID = currentUnit->baseUnitID;
        }

        return results;
    }

    // Warning: it will treat self as a parent of itself
    static bool isDescended(Database& db, Wt::Dbo::dbo_traits<Unit>::IdType potentialChildID, Wt::Dbo::dbo_traits<Unit>::IdType potentialParentID) {
        if (potentialParentID == Wt::Dbo::dbo_traits<Unit>::invalidId() || potentialChildID == Wt::Dbo::dbo_traits<Unit>::invalidId()) {
            return false;
        }

        if (potentialChildID == potentialParentID) {
            return true;
        }

        Wt::Dbo::ptr<Unit> potentialChild = db.find<Unit>().where("id = ?").bind(potentialChildID);
        return isDescended(db, potentialChild->baseUnitID, potentialParentID);
    }
};
