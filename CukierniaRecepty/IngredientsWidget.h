#pragma once
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

class IngredientsWidget : public Wt::WContainerWidget {
   public:
    IngredientsWidget(Wt::WContainerWidget* root = nullptr) {
        addButton = std::make_unique<Wt::WPushButton>(L"Dodaj składnik", this);
        addButton->clicked().connect(this, &IngredientsWidget::showAddDialog);

        ingredientList = std::make_unique<Wt::WTable>(this);
        ingredientList->addStyleClass("table table-stripped table-bordered");
        populateTableHeader(*ingredientList, "Nazwa", "Cena", "Jednostka", L"Usuń");
        populateIngredientList();
    }

    void populateIngredientList() {
        populateIngredientTable();
        makeTableEditable();
        setupDeleteAction();
    }

   private:
    std::unique_ptr<Wt::WTable> ingredientList;
    std::unordered_map<int, Wt::Dbo::dbo_traits<Ingredient>::IdType> rowToID;
    std::unique_ptr<Wt::WPushButton> addButton;

    void showAddDialog() {
        Wt::WDialog* dialog = new Wt::WDialog(L"Dodaj składnik");
        auto nameField = createLabeledField<Wt::WLineEdit>("Nazwa", dialog->contents());
        auto priceField = createLabeledField<Wt::WLineEdit>("Cena", dialog->contents());
        auto unitField = createLabeledField<Wt::WComboBox>("Jednoska", dialog->contents());
        auto unitIDs = populateComboBox<Unit>(*unitField, [](const Unit& elem) { return elem.name; });
        auto validationInfo = new Wt::WText(dialog->contents());

        // setup validators
        auto nameValidator = new Wt::WValidator(true);
        nameValidator->setMandatory(true);
        nameField->setValidator(nameValidator);
        auto priceValidator = new Wt::WDoubleValidator;
        priceValidator->setMandatory(true);
        priceField->setValidator(priceValidator);

        auto addButton = new Wt::WPushButton("Dodaj", dialog->footer());
        addButton->setDefault(true);
        addButton->clicked().connect(std::bind([=] {
            if (nameField->validate() != Wt::WValidator::Valid) {
                validationInfo->setText(Wt::WString(L"Nazwa składnika nie może być pusta!"));
            } else if (priceField->validate() != Wt::WValidator::Valid) {
                validationInfo->setText(Wt::WString(L"Podana cena jest niepoprawna(musi składać się z ciągu cyfr i maksymalnie pojedyńczej kropki)"));
            } else {
                dialog->accept();
            }
        }));

        auto cancelButton = new Wt::WPushButton("Anuluj", dialog->footer());
        cancelButton->clicked().connect(dialog, &Wt::WDialog::reject);
        dialog->rejectWhenEscapePressed();

        dialog->finished().connect(std::bind([=]() {
            if (dialog->result() == Wt::WDialog::Accepted) {
                auto ingredient = new Ingredient;
                ingredient->name = nameField->text();
                ingredient->price = std::stod(priceField->text());
                ingredient->unitID = unitIDs[unitField->currentIndex()];
                db.add<Ingredient>(ingredient);
                populateIngredientList();
            }

            delete dialog;
        }));

        dialog->show();
    }

    void populateIngredientTable() {
        rowToID.clear();
        populateTable<Ingredient>(*ingredientList, [&](const Wt::Dbo::ptr<Ingredient>& ingredient, int row) {
            rowToID.insert(std::make_pair(row, ingredient.id()));
            auto transaction = Wt::Dbo::Transaction{db};

            Wt::Dbo::ptr<Unit> unit = db.find<Unit>().where("id = ?").bind(ingredient->unitID);
            auto unitName = unit.id() != Wt::Dbo::dbo_traits<Unit>::invalidId() ? unit->name : L"Błędna jednostka";

            return std::vector<Wt::WString>{ingredient->name, std::to_string(ingredient->price), unitName, "X"};
        });
    }

    void makeTableEditable() {
        // make ingredient name editable
        makeTextCellsInteractive(*ingredientList, 0, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WValidator(true).validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return std::move(oldContent);
            }

            Wt::Dbo::Transaction transaction(db);
            Wt::Dbo::ptr<Ingredient> ingredient = db.find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->name = filledField.text();
            return Wt::WString(ingredient->name);
        });

        // make ingredient price editable
        makeTextCellsInteractive(*ingredientList, 1, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            Wt::WDoubleValidator validator;
            validator.setMandatory(true);
            if (validator.validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent.narrow();
            }

            Wt::Dbo::Transaction transaction(db);
            Wt::Dbo::ptr<Ingredient> ingredient = db.find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->price = std::stod(filledField.text());
            return std::to_string(ingredient->price);
        });

        // make ingredient unit editable
        auto unitKeys = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>>();
        makeCellsInteractive<Wt::WComboBox>(
            *ingredientList, 2,
            [this, unitKeys](int row, Wt::WComboBox& editField) {
                *unitKeys =
                    populateComboBox<Unit>(editField, [](const Unit& unit) { return unit.name; },
                                           [this, row](Wt::Dbo::ptr<Unit> potentialUnit) {
                                               auto transaction = Wt::Dbo::Transaction(db);
                                               auto ingredient = (Wt::Dbo::ptr<Ingredient>)db.find<Ingredient>().where("id = ?").bind(rowToID[row]);

                                               if (ingredient->unitID == potentialUnit.id() || Unit::isDescended(ingredient->unitID, potentialUnit.id()) ||
                                                   Unit::isDescended(potentialUnit.id(), ingredient->unitID)) {
                                                   return true;
                                               }
                                               return false;
                                           });

                auto oldContent = (Wt::WText*)ingredientList->elementAt(row, 2)->widget(0);
                auto oldUnitName = oldContent->text();
                auto indexOfOldUnit = editField.findText(oldUnitName);
                editField.setCurrentIndex(indexOfOldUnit);
            },
            [this, unitKeys](int row, const Wt::WComboBox& filledEditField, Wt::WString oldContent) {
                auto transaction = Wt::Dbo::Transaction(db);

                auto ingredient = (Wt::Dbo::ptr<Ingredient>)db.find<Ingredient>().where("id = ?").bind(rowToID[row]);
                auto unit = (Wt::Dbo::ptr<Unit>)db.find<Unit>().where("id = ?").bind((*unitKeys)[filledEditField.currentIndex()]);
                ingredient.modify()->unitID = unit.id();

                return filledEditField.currentText();
            });
    }

    void setupDeleteAction() {
        for (auto row = ingredientList->headerCount(); row < ingredientList->rowCount(); row++) {
            ingredientList->elementAt(row, 3)->clicked().connect(std::bind([this, row] {
                Wt::Dbo::Transaction transaction(db);

                auto ingredient = (Wt::Dbo::ptr<Ingredient>)db.find<Ingredient>().where("id = ?").bind(rowToID[row]);

                auto recipes = (Wt::Dbo::collection<Wt::Dbo::ptr<Recipe>>)db.find<Recipe>();
                for (const auto& recipe : recipes) {
                    for (const auto& ingredientRecord : recipe->ingredientRecords) {
                        if (ingredientRecord->ingredientID == ingredient.id()) {
                            auto dialog = new Wt::WDialog(L"Składnik jest używany");
                            auto okButton = new Wt::WPushButton("OK", dialog->footer());
                            okButton->clicked().connect(dialog, &Wt::WDialog::accept);

                            auto message = Wt::WString(L"Składnik jest używany co najmniej w przepisie ") + recipe->name;
                            message += L", więc nie może zostać usunięty.";
                            auto messageWidget = new Wt::WText(std::move(message), dialog->contents());

                            dialog->finished().connect(std::bind([dialog] { delete dialog; }));

                            dialog->show();

                            return;
                        }
                    }
                }

                ingredient.remove();
                populateIngredientList();  // deleting screws up references to rows in lambdas inside, so rebuild table
            }));
        }
    }
};
