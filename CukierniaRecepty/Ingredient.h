#pragma once
#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>
#include "Unit.h"

class Ingredient {
   public:
    Wt::WString name = "Nieznana nazwa";
    double price = 0.0;
    int kcal = 0;
    double fat = 0;
    double saturatedAcids = 0;
    double carbohydrates = 0;
    double sugar = 0;
    double protein = 0;
    double salt = 0;
    Wt::Dbo::dbo_traits<Unit>::IdType unitID = Wt::Dbo::dbo_traits<Unit>::invalidId();
    int ownerID = -1;

    template <class Action>
    void persist(Action& action) {
        Wt::Dbo::field(action, name, "name");
        Wt::Dbo::field(action, price, "price");
        Wt::Dbo::field(action, kcal, "kcal");
        Wt::Dbo::field(action, fat, "fat");
        Wt::Dbo::field(action, saturatedAcids, "saturated_acids");
        Wt::Dbo::field(action, carbohydrates, "carbohydrates");
        Wt::Dbo::field(action, sugar, "sugar");
        Wt::Dbo::field(action, protein, "protein");
        Wt::Dbo::field(action, salt, "salt");
        Wt::Dbo::field(action, unitID, "unit_id");
        Wt::Dbo::field(action, ownerID, "owner_id");
    }
};
