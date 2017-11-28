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

	double scaledIngredientValue(Database& db, std::function<double(const Ingredient&)> value) const {
        Wt::Dbo::ptr<Ingredient> ingredient = db.find<Ingredient>().where("id = ?").bind(this->ingredientID);
        if (ingredient.id() == Wt::Dbo::dbo_traits<Ingredient>::invalidId()) {
            return -1;
        }

        auto ingredientRecordQuantityInBaseUnits = 1.0;
        auto ingredientRecordUnitPath = Unit::pathToTheRoot(db, this->unitID);
        for (const auto& unit : ingredientRecordUnitPath) {
            ingredientRecordQuantityInBaseUnits *= unit->quantity;
        }

        auto ingredientQuantityInBaseUnits = 1.0;
        auto ingredientUnitPath = Unit::pathToTheRoot(db, ingredient->unitID);
        for (const auto& unit : ingredientUnitPath) {
            ingredientQuantityInBaseUnits *= unit->quantity;
        }

        return value(*ingredient) / ingredientQuantityInBaseUnits * ingredientRecordQuantityInBaseUnits * this->quantity;
    }
};

class Recipe {
   public:
    Wt::WString name = "Nieznana nazwa";
    Wt::Dbo::collection<Wt::Dbo::ptr<IngredientRecord>> ingredientRecords;
    int ownerID = -1;

    template <class Action>
    void persist(Action& action) {
        Wt::Dbo::field(action, name, "name");
        Wt::Dbo::field(action, ownerID, "owner_id");
        Wt::Dbo::hasMany(action, ingredientRecords, Wt::Dbo::ManyToOne, "recipe");
    }

    //-1 in the case of error, otherwise sum of chosen scaled ingredient values
    double totalIngredientValue(Database& db, std::function<double(const Ingredient&)> value) const {
        auto transaction = Wt::Dbo::Transaction{db};

        auto result = 0.0;
        for (const auto& ingredientRecord : ingredientRecords) {
            auto sum  = ingredientRecord->scaledIngredientValue(db, [&](const Ingredient& i) { return value(i); });
            if (sum == -1) {
                return -1;
            }

            result += sum;
        }

        return result;
    }
};
