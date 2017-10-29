#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <Wt/Dbo/Session>
#include <Wt/WLineEdit>
#include <Wt/WDialog>
#include <Wt/WPushButton>
#include <Wt/WDoubleValidator>
#include "helpers.h"
#include "Ingredient.h"
#include "Unit.h"
#include "Recipe.h"

class RecipeDetailsWidget : public Wt::WContainerWidget {
   public:
    RecipeDetailsWidget(Wt::WContainerWidget*, Database& db) {
        this->db = &db;
        addButton = std::make_unique<Wt::WPushButton>(L"Dodaj składnik", this);
        addButton->clicked().connect(this, &RecipeDetailsWidget::showAddDialog);

        ingredientList = std::make_unique<Wt::WTable>(this);
        ingredientList->addStyleClass("table table-stripped table-bordered");

        populateTableHeader(*ingredientList, "Skladnik", "Ilosc", "Jednostka", L"Usuń");
        populateIngredientList();
    }

    void setRecipe(Wt::Dbo::dbo_traits<Recipe>::IdType recipeID) {
        currentRecipe = recipeID;
        populateIngredientList();
    }

    void populateIngredientList() {
        if (currentRecipe == Wt::Dbo::dbo_traits<Recipe>::invalidId()) {
            return;
        }

        populateIngredientTable();
        makeTableEditable();
        setupDeleteAction();
    }

   private:
    Database* db;
    std::unique_ptr<Wt::WTable> ingredientList;
    std::unordered_map<int, Wt::Dbo::dbo_traits<IngredientRecord>::IdType> rowToID;
    std::unique_ptr<Wt::WPushButton> addButton;
    Wt::Dbo::dbo_traits<Recipe>::IdType currentRecipe;

    void showAddDialog() {
        Wt::WDialog* dialog = new Wt::WDialog(L"Dodaj składnik");

        auto nameField = createLabeledField<Wt::WComboBox>(L"Składnik", dialog->contents());
        auto ingredientIDs = populateComboBox<Ingredient>(*db, *nameField, [](const Ingredient& elem) { return elem.name; });

        auto quantityField = createLabeledField<Wt::WLineEdit>(L"Ilość", dialog->contents());

        auto unitField = createLabeledField<Wt::WComboBox>("Jednostka", dialog->contents());
        auto unitIDs = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>>();

        nameField->changed().connect(std::bind([=] {
            auto ingredientID = ingredientIDs[nameField->currentIndex()];

            unitField->clear();
            *unitIDs = populateComboBox<Unit>(*db, *unitField, [](const Unit& unit) { return unit.name; },
                                              [this, ingredientID](Wt::Dbo::ptr<Unit> potentialUnit) {
                                                  auto transaction = Wt::Dbo::Transaction(*db);
                                                  auto ingredient = (Wt::Dbo::ptr<Ingredient>)db->find<Ingredient>().where("id = ?").bind(ingredientID);

                                                  if (ingredient->unitID == potentialUnit.id() || Unit::isDescended(*db, ingredient->unitID, potentialUnit.id()) ||
                                                      Unit::isDescended(*db, potentialUnit.id(), ingredient->unitID)) {
                                                      return true;
                                                  }
                                                  return false;
                                              });
        }));

        nameField->changed().emit();

        // setup validation
        auto validationInfo = new Wt::WText(dialog->contents());
        auto quantityValidator = new Wt::WDoubleValidator;
        quantityValidator->setMandatory(true);
        quantityField->setValidator(quantityValidator);

        auto addButton = new Wt::WPushButton("Dodaj", dialog->footer());
        addButton->setDefault(true);
        addButton->clicked().connect(std::bind([=] {
            if (quantityField->validate() != Wt::WValidator::Valid) {
                validationInfo->setText(Wt::WString(L"Ilosc skladnika musi byc podana(w poprawnym formacie)!"));
            } else {
                dialog->accept();
            }
        }));

        auto cancelButton = new Wt::WPushButton("Anuluj", dialog->footer());
        cancelButton->clicked().connect(dialog, &Wt::WDialog::reject);
        dialog->rejectWhenEscapePressed();

        dialog->finished().connect(std::bind([=]() {
            if (dialog->result() == Wt::WDialog::Accepted) {
                auto transaction = Wt::Dbo::Transaction{*db};

                auto ingredientRecord = new IngredientRecord;
                ingredientRecord->ingredientID = ingredientIDs[nameField->currentIndex()];
                ingredientRecord->quantity = std::stod(quantityField->text());
                ingredientRecord->unitID = (*unitIDs)[unitField->currentIndex()];
                ingredientRecord->recipe = (Wt::Dbo::ptr<Recipe>)db->find<Recipe>().where("id = ?").bind(currentRecipe);
                db->add<IngredientRecord>(ingredientRecord);
                populateIngredientList();
            }

            delete dialog;
        }));

        dialog->show();
    }

    void populateIngredientTable() {
        rowToID.clear();

        populateTable<IngredientRecord>(*db, *ingredientList,
                                        [&](const Wt::Dbo::ptr<IngredientRecord>& ingredientRecord, int row) {
                                            rowToID.insert(std::make_pair(row, ingredientRecord.id()));
                                            auto transaction = Wt::Dbo::Transaction{*db};

                                            Wt::Dbo::ptr<Unit> unit = db->find<Unit>().where("id = ?").bind(ingredientRecord->unitID);
                                            auto unitName = unit.id() != Wt::Dbo::dbo_traits<Unit>::invalidId() ? unit->name : L"Błędna jednostka";

                                            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(ingredientRecord->ingredientID);
                                            auto ingredientName =
                                                ingredient.id() != Wt::Dbo::dbo_traits<Ingredient>::invalidId() ? ingredient->name : L"Błędny skladnik";

                                            return std::vector<Wt::WString>{ingredientName, std::to_string(ingredientRecord->quantity), unitName, "X"};

                                        },
                                        [this](const Wt::Dbo::ptr<IngredientRecord>& element) { return element->recipe.id() == currentRecipe; });
    }

    void makeTableEditable() {
        // make ingredient field editable
        auto ingredientKeys = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Ingredient>::IdType>>();
        makeCellsInteractive<Wt::WComboBox>(
            *ingredientList, 0,
            [this, ingredientKeys](int row, Wt::WComboBox& editField) {
                *ingredientKeys = populateComboBox<Ingredient>(*db, editField, [](const Ingredient& ingredient) { return ingredient.name; });

                auto oldContent = (Wt::WText*)ingredientList->elementAt(row, 0)->widget(0);
                auto oldIngredientName = oldContent->text();
                auto indexOfOldIngredient = editField.findText(oldIngredientName);
                editField.setCurrentIndex(indexOfOldIngredient);
            },
            [this, ingredientKeys](int row, const Wt::WComboBox& filledEditField, Wt::WString) {
                auto transaction = Wt::Dbo::Transaction(*db);

                auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
                auto ingredient = (Wt::Dbo::ptr<Ingredient>)db->find<Ingredient>().where("id = ?").bind((*ingredientKeys)[filledEditField.currentIndex()]);
                ingredientRecord.modify()->ingredientID = ingredient.id();

                return filledEditField.currentText();
            });

        // make ingredient quantity editable
        makeTextCellsInteractive(*ingredientList, 1, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            Wt::WDoubleValidator validator;
            validator.setMandatory(true);
            if (validator.validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent.narrow();
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<IngredientRecord> ingredientRecord = db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
            ingredientRecord.modify()->quantity = std::stod(filledField.text());
            return std::to_string(ingredientRecord->quantity);
        });

        // make ingredient unit editable
        auto unitKeys = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>>();
        makeCellsInteractive<Wt::WComboBox>(
            *ingredientList, 2,
            [this, unitKeys](int row, Wt::WComboBox& editField) {
                *unitKeys = populateComboBox<Unit>(
                    *db, editField, [](const Unit& unit) { return unit.name; },
                    [this, row](Wt::Dbo::ptr<Unit> potentialUnit) {
                        auto transaction = Wt::Dbo::Transaction(*db);
                        auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);

                        if (ingredientRecord->unitID == potentialUnit.id() || Unit::isDescended(*db, ingredientRecord->unitID, potentialUnit.id()) ||
                            Unit::isDescended(*db, potentialUnit.id(), ingredientRecord->unitID)) {
                            return true;
                        }
                        return false;
                    });

                auto oldContent = (Wt::WText*)ingredientList->elementAt(row, 2)->widget(0);
                auto oldUnitName = oldContent->text();
                auto indexOfOldUnit = editField.findText(oldUnitName);
                editField.setCurrentIndex(indexOfOldUnit);
            },
            [this, unitKeys](int row, const Wt::WComboBox& filledEditField, Wt::WString) {
                auto transaction = Wt::Dbo::Transaction(*db);

                auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
                auto unit = (Wt::Dbo::ptr<Unit>)db->find<Unit>().where("id = ?").bind((*unitKeys)[filledEditField.currentIndex()]);
                ingredientRecord.modify()->unitID = unit.id();

                return filledEditField.currentText();
            });
    }

    void setupDeleteAction() {
        for (auto row = ingredientList->headerCount(); row < ingredientList->rowCount(); row++) {
            ingredientList->elementAt(row, 3)->clicked().connect(std::bind([this, row] {
                Wt::Dbo::Transaction transaction(*db);

                auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
                ingredientRecord.remove();
                populateIngredientList();  // deleting screws up references to rows in lambdas inside, so rebuild table
            }));
        }
    }
};
