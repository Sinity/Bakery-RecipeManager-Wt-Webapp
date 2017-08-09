#pragma once
#include <numeric>
#include <Wt/Dbo/Dbo>
#include "Unit.h"
#include "Ingredient.h"

class Recipe;

class IngredientRecord {
   public:
    double quantity = 0.0;
    Wt::Dbo::dbo_traits<Ingredient>::IdType ingredientID = Wt::Dbo::dbo_traits<Ingredient>::invalidId();
    Wt::Dbo::dbo_traits<Unit>::IdType unitID = Wt::Dbo::dbo_traits<Unit>::invalidId();
    Wt::Dbo::ptr<Recipe> recipe;

    template <class Action>
    void persist(Action& action) {
        Wt::Dbo::field(action, quantity, "quantity");
        Wt::Dbo::field(action, unitID, "unit_id");
        Wt::Dbo::field(action, ingredientID, "ingredient_id");
        Wt::Dbo::belongsTo(action, recipe, "recipe");
    }

	double price() const{
        Wt::Dbo::ptr<Ingredient> ingredient = db.find<Ingredient>().where("id = ?").bind(this->ingredientID);
        if (ingredient.id() == Wt::Dbo::dbo_traits<Ingredient>::invalidId()) {
            return -1;
        }

        auto ingredientRecordQuantityInBaseUnits = 1.0;
        auto ingredientRecordUnitPath = Unit::pathToTheRoot(this->unitID);
        for (auto i = 0u; i < ingredientRecordUnitPath.size(); i++) {
            ingredientRecordQuantityInBaseUnits *= ingredientRecordUnitPath[i]->quantity;
        }

        auto ingredientQuantityInBaseUnits = 1.0;
        auto ingredientUnitPath = Unit::pathToTheRoot(ingredient->unitID);
        for (auto i = 0u; i < ingredientUnitPath.size(); i++) {
            ingredientQuantityInBaseUnits *= ingredientUnitPath[i]->quantity;
        }

        return ingredient->price / ingredientQuantityInBaseUnits * ingredientRecordQuantityInBaseUnits * this->quantity;
    }
};

class Recipe {
   public:
    Wt::WString name = "Nieznana nazwa";
    Wt::Dbo::collection<Wt::Dbo::ptr<IngredientRecord>> ingredientRecords;

    template <class Action>
    void persist(Action& action) {
        Wt::Dbo::field(action, name, "name");
        Wt::Dbo::hasMany(action, ingredientRecords, Wt::Dbo::ManyToOne, "recipe");
    }

    //-1 in the case of error, otherwise total price of this recipe
    double totalPrice() const {
        auto transaction = Wt::Dbo::Transaction{db};

        auto result = 0.0;
        for (const auto& ingredientRecord : ingredientRecords) {
            auto price = ingredientRecord->price();
            if (price == -1) {
                return -1;
            }

            result += price;
        }

        return result;
    }
};
