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
    const std::wstring colIngredient = L"Składnik";
    const std::wstring colQuantity = L"Ilość";
    const std::wstring colUnit = L"Jednostka";
    const std::wstring colKcal = L"Kaloryczność";
    const std::wstring colFats = L"Tłuszcze";
    const std::wstring colSatAcids = L"Kwasy nasycone";
    const std::wstring colCarbs = L"Węglowodany";
    const std::wstring colSugar = L"Cukry";
    const std::wstring colProtein = L"Białka";
    const std::wstring colSalt = L"Sól";
    const std::wstring colCost = L"Koszt";
    const std::wstring colDelete = L"Usuń";
   public:
    RecipeDetailsWidget(Wt::WContainerWidget*, Database& db) : db(&db) {
        if(db.users->find(db.login.user())->user()->accessLevel != 0) {
            addButton = std::make_unique<Wt::WPushButton>(L"Dodaj składnik", this);
            addButton->clicked().connect(this, &RecipeDetailsWidget::showAddDialog);
        }

        ingredientList = std::make_unique<Wt::WTable>(this);
        ingredientList->addStyleClass("table table-stripped table-bordered");
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

        if(db->users->find(db->login.user())->user()->accessLevel != 0) {
            makeTableEditable();
            setupDeleteAction();
        }
    }

    Wt::Dbo::dbo_traits<Recipe>::IdType currentRecipe = Wt::Dbo::dbo_traits<Recipe>::invalidId();
   private:
    Database* db;
    std::unique_ptr<Wt::WTable> ingredientList;
    std::unordered_map<int, Wt::Dbo::dbo_traits<IngredientRecord>::IdType> rowToID;
    std::unique_ptr<Wt::WPushButton> addButton;

    void showAddDialog() {
        Wt::WDialog* dialog = new Wt::WDialog(L"Dodaj składnik");

        auto nameField = createLabeledField<Wt::WComboBox>(L"Składnik", dialog->contents());
        auto ingredientIDs = populateComboBox<Ingredient>(*db, *nameField, [](const Ingredient& elem) { return elem.name; },
            [this](const Wt::Dbo::ptr<Ingredient>& elem) { return elem->ownerID == db->users->find(db->login.user())->user()->firmID; });

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
                                                  return Unit::sameBranch(*db, potentialUnit.id(), ingredient->unitID);
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

    void updateColumn(const std::wstring& colName, int row, const Wt::WString& newContent) {
        for(auto i = 0; i < ingredientList->columnCount(); i++) {
            auto currColName = ((Wt::WText*)ingredientList->elementAt(0, i)->widget(0))->text();
            if (currColName == colName) {
                auto elem = (Wt::WText*)ingredientList->elementAt(row, i)->widget(0);
                elem->setText(newContent);
                return;
            }
        }
    }

    void populateIngredientTable() {
        rowToID.clear();

        populateTable<IngredientRecord>(*db, *ingredientList,
            [&](const Wt::Dbo::ptr<IngredientRecord>& ingredientRecord, int row) {
                auto transaction = Wt::Dbo::Transaction{*db};
                rowToID.insert(std::make_pair(row, ingredientRecord.id()));
                std::vector<std::pair<std::wstring, Wt::WString>> columns;

                Wt::Dbo::ptr<Unit> unit = db->find<Unit>().where("id = ?").bind(ingredientRecord->unitID);
                auto unitName = unit.id() != Wt::Dbo::dbo_traits<Unit>::invalidId() ? unit->name : L"Błędna jednostka";

                Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(ingredientRecord->ingredientID);
                auto ingredientName =
                    ingredient.id() != Wt::Dbo::dbo_traits<Ingredient>::invalidId() ? ingredient->name : L"Błędny skladnik";

                auto cost = ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.price; });
                auto kcal = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.kcal; }));
                auto fat = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.fat; }));
                auto saturatedAcids = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.saturatedAcids; }));
                auto carbohydrates = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.carbohydrates; }));
                auto sugar = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.sugar; }));
                auto protein = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.protein; }));
                auto salt = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.salt; }));

                columns.emplace_back(colIngredient, ingredientName);
                columns.emplace_back(colUnit, unitName);
                columns.emplace_back(colQuantity, std::to_string(ingredientRecord->quantity));
                if(db->users->find(db->login.user())->user()->accessLevel != 0)
                    columns.emplace_back(colCost, cost == -1 ? L"Błąd, nie można obliczyć kosztu" : std::to_wstring(cost));

                columns.emplace_back(colKcal, kcal);
                columns.emplace_back(colFats, fat);
                columns.emplace_back(colSatAcids, saturatedAcids);
                columns.emplace_back(colCarbs, carbohydrates);
                columns.emplace_back(colSugar, sugar);
                columns.emplace_back(colProtein, protein);
                columns.emplace_back(colSalt, salt);

                if(db->users->find(db->login.user())->user()->accessLevel != 0)
                    columns.emplace_back(colDelete, "X");
                return columns;
            },
            [&](const Wt::Dbo::ptr<IngredientRecord>& element) {
                Wt::Dbo::Transaction t{*db};
                return element->recipe.id() == currentRecipe; 
            });
    }

    void makeTableEditable() {
         // make ingredient field editable
        auto ingredientKeys = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Ingredient>::IdType>>();
        makeCellsInteractive<Wt::WComboBox>(
            *ingredientList, colIngredient,
            [this, ingredientKeys](int row, Wt::WComboBox& editField) {
                *ingredientKeys = populateComboBox<Ingredient>(*db, editField, [](const Ingredient& ingredient) { return ingredient.name; },
                    [this](const Wt::Dbo::ptr<Ingredient>& elem) { return elem->ownerID == db->users->find(db->login.user())->user()->firmID; });

                auto oldContent = (Wt::WText*)ingredientList->elementAt(row, findColumn(*ingredientList, colIngredient))->widget(0);
                auto oldIngredientName = oldContent->text();
                auto indexOfOldIngredient = editField.findText(oldIngredientName);
                editField.setCurrentIndex(indexOfOldIngredient);
            },
            [this, ingredientKeys](int row, const Wt::WComboBox& filledEditField, Wt::WString) {
                auto transaction = Wt::Dbo::Transaction(*db);

                auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
                auto ingredient = (Wt::Dbo::ptr<Ingredient>)db->find<Ingredient>().where("id = ?").bind((*ingredientKeys)[filledEditField.currentIndex()]);
                if (ingredient.id() != ingredientRecord->ingredientID) {
                    ingredientRecord.modify()->ingredientID = ingredient.id();

                    auto cost = ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.price; });
                    auto kcal = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.kcal; }));
                    auto fat = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.fat; }));
                    auto saturatedAcids = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.saturatedAcids; }));
                    auto carbohydrates = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.carbohydrates; }));
                    auto sugar = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.sugar; }));
                    auto protein = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.protein; }));
                    auto salt = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.salt; }));

                    updateColumn(colCost, row, cost == -1 ? L"Błąd, nie można obliczyć kosztu" : std::to_wstring(cost));
                    updateColumn(colKcal, row, kcal);
                    updateColumn(colFats, row, fat);
                    updateColumn(colSatAcids, row, saturatedAcids);
                    updateColumn(colCarbs, row, carbohydrates);
                    updateColumn(colSugar, row, sugar);
                    updateColumn(colProtein, row, protein);
                    updateColumn(colSalt, row, salt);
                }

                return filledEditField.currentText();
            });

        // make ingredient quantity editable
        makeTextCellsInteractive(*ingredientList, colQuantity, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            Wt::WDoubleValidator validator;
            validator.setMandatory(true);
            if (validator.validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent.narrow();
            }
            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<IngredientRecord> ingredientRecord = db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
            if (std::stod(filledField.text()) != ingredientRecord->quantity) {
                ingredientRecord.modify()->quantity = std::stod(filledField.text());

                auto cost = ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.price; });
                auto kcal = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.kcal; }));
                auto fat = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.fat; }));
                auto saturatedAcids = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.saturatedAcids; }));
                auto carbohydrates = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.carbohydrates; }));
                auto sugar = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.sugar; }));
                auto protein = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.protein; }));
                auto salt = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.salt; }));

                updateColumn(colCost, row, cost == -1 ? L"Błąd, nie można obliczyć kosztu" : std::to_wstring(cost));
                updateColumn(colKcal, row, kcal);
                updateColumn(colFats, row, fat);
                updateColumn(colSatAcids, row, saturatedAcids);
                updateColumn(colCarbs, row, carbohydrates);
                updateColumn(colSugar, row, sugar);
                updateColumn(colProtein, row, protein);
                updateColumn(colSalt, row, salt);
            }

            return std::to_string(ingredientRecord->quantity);
        });

        // make ingredient unit editable
        auto unitKeys = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>>();
        makeCellsInteractive<Wt::WComboBox>(
            *ingredientList, colUnit,
            [this, unitKeys](int row, Wt::WComboBox& editField) {
                *unitKeys = populateComboBox<Unit>(
                    *db, editField, [](const Unit& unit) { return unit.name; },
                    [this, row](Wt::Dbo::ptr<Unit> potentialUnit) {
                        auto transaction = Wt::Dbo::Transaction(*db);
                        auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
                        return Unit::sameBranch(*db, potentialUnit.id(), ingredientRecord->unitID);
                    });

                auto oldContent = (Wt::WText*)ingredientList->elementAt(row, findColumn(*ingredientList, colUnit))->widget(0);
                auto oldUnitName = oldContent->text();
                auto indexOfOldUnit = editField.findText(oldUnitName);
                editField.setCurrentIndex(indexOfOldUnit);
            },
            [this, unitKeys](int row, const Wt::WComboBox& filledEditField, Wt::WString) {
                auto transaction = Wt::Dbo::Transaction(*db);

                auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
                auto unit = (Wt::Dbo::ptr<Unit>)db->find<Unit>().where("id = ?").bind((*unitKeys)[filledEditField.currentIndex()]);

                if (ingredientRecord->unitID != unit.id()) {
                    ingredientRecord.modify()->unitID = unit.id();

                    auto cost = ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.price; });
                    auto kcal = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.kcal; }));
                    auto fat = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.fat; }));
                    auto saturatedAcids = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.saturatedAcids; }));
                    auto carbohydrates = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.carbohydrates; }));
                    auto sugar = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.sugar; }));
                    auto protein = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.protein; }));
                    auto salt = std::to_wstring(ingredientRecord->scaledIngredientValue(*db, [](const Ingredient& i) { return i.salt; }));

                    updateColumn(colCost, row, cost == -1 ? L"Błąd, nie można obliczyć kosztu" : std::to_wstring(cost));
                    updateColumn(colKcal, row, kcal);
                    updateColumn(colFats, row, fat);
                    updateColumn(colSatAcids, row, saturatedAcids);
                    updateColumn(colCarbs, row, carbohydrates);
                    updateColumn(colSugar, row, sugar);
                    updateColumn(colProtein, row, protein);
                    updateColumn(colSalt, row, salt);
                }

                return filledEditField.currentText();
            });
    }

    void setupDeleteAction() {
        auto column = findColumn(*ingredientList, colDelete);
        if (column == -1)
            return;

        for (auto row = ingredientList->headerCount(); row < ingredientList->rowCount(); row++) {
            ingredientList->elementAt(row, column)->clicked().connect(std::bind([this, row] {
                auto confirmationDialog = new Wt::WDialog(L"Potwierdzenie usunięcia składnika przepisu");
                auto yesButton = new Wt::WPushButton("Tak", confirmationDialog->footer());
                auto noButton = new Wt::WPushButton("Nie", confirmationDialog->footer());
                new Wt::WText(L"Czy napewno usunąć składnik tego przepisu?", confirmationDialog->contents());
                yesButton->clicked().connect(confirmationDialog, &Wt::WDialog::accept);
                noButton->clicked().connect(confirmationDialog, &Wt::WDialog::reject);
                confirmationDialog->rejectWhenEscapePressed();

                confirmationDialog->finished().connect(std::bind([this, confirmationDialog, row] {
                    if (confirmationDialog->result() != Wt::WDialog::Accepted)
                        return;

                    Wt::Dbo::Transaction transaction(*db);

                    auto ingredientRecord = (Wt::Dbo::ptr<IngredientRecord>)db->find<IngredientRecord>().where("id = ?").bind(rowToID[row]);
                    ingredientRecord.remove();
                    populateIngredientList();  // deleting screws up references to rows in lambdas inside, so rebuild table
                    delete confirmationDialog;
                }));

                confirmationDialog->show();
            }));
        }
    }
};
