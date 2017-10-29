#pragma once
#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>
#include "Unit.h"

class Ingredient {
   public:
    Wt::WString name = "Nieznana nazwa";
    double price = 0.0;
    Wt::Dbo::dbo_traits<Unit>::IdType unitID = Wt::Dbo::dbo_traits<Unit>::invalidId();
    int ownerID = -1;

    template <class Action>
    void persist(Action& action) {
        Wt::Dbo::field(action, name, "name");
        Wt::Dbo::field(action, price, "price");
        Wt::Dbo::field(action, unitID, "unit_id");
        Wt::Dbo::field(action, ownerID, "owner_id");
    }
};
