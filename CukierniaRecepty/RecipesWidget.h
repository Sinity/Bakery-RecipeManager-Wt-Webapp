#pragma once
#include <memory>
#include <unordered_map>
#include <Wt/Dbo/Session>
#include <Wt/WContainerWidget>
#include <Wt/WBreak>
#include <Wt/WDialog>
#include <Wt/WApplication>
#include "Recipe.h"
#include "RecipeDetailsWidget.h"
#include "helpers.h"
#include "database.h"

class RecipesWidget : public Wt::WContainerWidget {
    const std::wstring colName = L"Nazwa";
    const std::wstring colKcal = L"Kaloryczność";
    const std::wstring colFats = L"Tłuszcze";
    const std::wstring colSatAcids = L"Kwasy nasycone";
    const std::wstring colCarbs = L"Węglowodany";
    const std::wstring colSugar = L"Cukry";
    const std::wstring colProtein = L"Białka";
    const std::wstring colSalt = L"Sól";
    const std::wstring colCost = L"Koszt";
    const std::wstring colDelete = L"Usuń";
    const std::wstring colDetails = L"Szczegóły";
   public:
    Wt::Dbo::dbo_traits<Recipe>::IdType currentRecipe = Wt::Dbo::dbo_traits<Recipe>::invalidId();

    RecipesWidget(Wt::WContainerWidget*, Database& db) : db(&db) {
        Wt::Dbo::Transaction t{db};
        if(db.users->find(db.login.user())->user()->accessLevel != 0) {
            addButton = std::make_unique<Wt::WPushButton>("Dodaj przepis", this);
            addButton->clicked().connect(this, &RecipesWidget::showAddDialog);
        }

        addWidget(new Wt::WBreak);
        addWidget(new Wt::WLabel("Filtr: "));
        filter = std::make_unique<Wt::WLineEdit>(this);
        filter->setTextSize(filter->text().value().length() + 1);
        filter->setPlaceholderText(L"Część nazwy szukanego przepisu");
        filter->enterPressed().connect(std::bind([this] {
            populateRecipeList();
        }));

        recipeList = std::make_unique<Wt::WTable>(this);
        recipeList->addStyleClass("table table-stripped table-bordered");

        populateRecipeList();
    }

    void populateRecipeList() {
        populateRecipeTable([this](const Wt::Dbo::ptr<Recipe>& recipe) {
            Wt::Dbo::Transaction t{*db};
            auto recipeName = std::wstring(recipe->name);
            return recipe->ownerID == db->users->find(db->login.user())->user()->firmID && recipeName.find(filter->text()) != std::wstring::npos;
        });

        if(db->users->find(db->login.user())->user()->accessLevel != 0) {
            makeTableEditable();
            setupDeleteAction();
        }

        setupGotoDetails();
    }

   private:
    Database* db;
    std::unique_ptr<Wt::WLineEdit> filter;
    bool validFilter = false;
    std::unique_ptr<Wt::WTable> recipeList;
    std::unordered_map<int, Wt::Dbo::dbo_traits<Recipe>::IdType> rowToID;
    std::unique_ptr<Wt::WPushButton> addButton;

    void showAddDialog() {
        Wt::WDialog* dialog = new Wt::WDialog("Dodaj przepis");

        // setup ingredient list
        Wt::WTable* ingredientList = new Wt::WTable(dialog->contents());
        ingredientList->addStyleClass("table table-stripped table-bordered");
        populateTableHeader(*ingredientList, L"Nazwa składnika", L"Ilość składnika", L"Jednostka składnika");

        // setup ingredient list entires <===> id mappers
        auto rowToIngredient = std::make_shared<std::unordered_map<int, Wt::Dbo::dbo_traits<Ingredient>::IdType>>();
        auto rowToUnit = std::make_shared<std::unordered_map<int, Wt::Dbo::dbo_traits<Unit>::IdType>>();

        // setup UI fields
        auto nameField = createLabeledField<Wt::WLineEdit>(L"Nazwa przepisu", dialog->contents());
        auto ingredientField = createLabeledField<Wt::WComboBox>(L"Składnik", dialog->contents());
        auto ingredientQuantityField = createLabeledField<Wt::WLineEdit>(L"Ilość składnika", dialog->contents());
        auto ingredientUnitField = createLabeledField<Wt::WComboBox>(L"Jednostka składnika", dialog->contents());
        auto addIngredientButton = new Wt::WPushButton(L"Dodaj składnik", dialog->contents());
        auto validationInfo = new Wt::WText(dialog->contents());

        // setup validators
        auto nameValidator = new Wt::WValidator(true);
        nameField->setValidator(nameValidator);
        auto ingredientQuantityValidator = new Wt::WDoubleValidator;
        ingredientQuantityValidator->setMandatory(true);
        ingredientQuantityField->setValidator(ingredientQuantityValidator);

        // fill combo box fields and setup combo box index <===> id mappers
        auto tempIngredientIDs = populateComboBox<Ingredient>(*db, *ingredientField, [](const Ingredient& ingredient) { return ingredient.name; },
            [this](const Wt::Dbo::ptr<Ingredient>& elem) { return elem->ownerID == db->users->find(db->login.user())->user()->firmID; });
        auto ingredientIDs = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Ingredient>::IdType>>(std::move(tempIngredientIDs));

        auto unitIDs = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>>();

        ingredientField->changed().connect(std::bind([=] {
            auto ingredientID = (*ingredientIDs)[ingredientField->currentIndex()];

            ingredientUnitField->clear();
            *unitIDs = populateComboBox<Unit>(*db, *ingredientUnitField, [ingredientID](const Unit& unit) { return unit.name; },
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

        ingredientField->changed().emit();

        // adding ingredients logic
        addIngredientButton->clicked().connect(std::bind([=] {
            if (ingredientQuantityField->validate() != Wt::WValidator::Valid) {
                validationInfo->show();
                validationInfo->setText(L"Ilość składnika jest niepoprawna(musi składać się z ciągu cyfr i opcjonalnej kropki)");
                return;
            }
            validationInfo->hide();

            auto rowIndex = ingredientList->rowCount();
            (*rowToIngredient)[rowIndex] = (*ingredientIDs)[ingredientField->currentIndex()];
            (*rowToUnit)[rowIndex] = (*unitIDs)[ingredientUnitField->currentIndex()];

            ingredientList->elementAt(rowIndex, 0)->addWidget(new Wt::WText(ingredientField->currentText()));
            ingredientList->elementAt(rowIndex, 1)->addWidget(new Wt::WText(ingredientQuantityField->text()));
            ingredientList->elementAt(rowIndex, 2)->addWidget(new Wt::WText(ingredientUnitField->currentText()));
        }));

        auto addButton = new Wt::WPushButton(L"Zatwierdź przepis", dialog->footer());
        addButton->setDefault(true);
        addButton->clicked().connect(std::bind([=] {
            if (nameField->validate() != Wt::WValidator::Valid) {
                validationInfo->show();
                validationInfo->setText(L"Nazwa przepisu nie może być pusta!");
            } else {
                dialog->accept();
            }
        }));

        auto cancelButton = new Wt::WPushButton("Anuluj", dialog->footer());
        cancelButton->clicked().connect(dialog, &Wt::WDialog::reject);
        dialog->rejectWhenEscapePressed();

        // adding recipe
        dialog->finished().connect(std::bind([=]() {
            if (dialog->result() == Wt::WDialog::Accepted) {
                addRecipe(nameField->text(), *ingredientList, rowToIngredient, rowToUnit);
                populateRecipeList();
            }

            delete dialog;
        }));

        dialog->show();
    }

    void addRecipe(const Wt::WString& name, Wt::WTable& ingredients,
                   std::shared_ptr<std::unordered_map<int, Wt::Dbo::dbo_traits<Ingredient>::IdType>> rowToIngredient,
                   std::shared_ptr<std::unordered_map<int, Wt::Dbo::dbo_traits<Unit>::IdType>> rowToUnit) {
        Wt::Dbo::Transaction transaction(*db);
        auto recipe = db->add(new Recipe);
        recipe.modify()->name = name;
        recipe.modify()->ownerID = db->users->find(db->login.user())->user()->firmID;

        for (auto row = ingredients.headerCount(); row < ingredients.rowCount(); row++) {
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind((*rowToIngredient)[row]);
            Wt::Dbo::ptr<Unit> unit = db->find<Unit>().where("id = ?").bind((*rowToUnit)[row]);

            auto ingredientQuantityText = (Wt::WText*)ingredients.elementAt(row, 1)->widget(0);

            auto ingredientRecord = db->add(new IngredientRecord);
            ingredientRecord.modify()->ingredientID = ingredient.id();
            ingredientRecord.modify()->unitID = unit.id();
            ingredientRecord.modify()->quantity = std::stod(ingredientQuantityText->text().narrow());
            ingredientRecord.modify()->recipe = recipe;
        }
    }

    void populateRecipeTable(std::function<bool(const Wt::Dbo::ptr<Recipe>& element)> filter) {
        rowToID.clear();
        populateTable<Recipe>(*db, *recipeList, [&](const Wt::Dbo::ptr<Recipe>& recipe, int row) {
            auto transaction = Wt::Dbo::Transaction{*db};
            rowToID.insert(std::make_pair(row, recipe.id()));
            std::vector<std::pair<std::wstring, Wt::WString>> columns;

            auto cost = recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.price; });
            auto kcal = std::to_wstring(recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.kcal; }));
            auto fat = std::to_wstring(recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.fat; }));
            auto saturatedAcids = std::to_wstring(recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.saturatedAcids; }));
            auto carbohydrates = std::to_wstring(recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.carbohydrates; }));
            auto sugar = std::to_wstring(recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.sugar; }));
            auto protein = std::to_wstring(recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.protein; }));
            auto salt = std::to_wstring(recipe->totalIngredientValue(*db, [](const Ingredient& i) { return i.salt; }));

            columns.emplace_back(colName, recipe->name);
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
        }, filter);
    }

    void makeTableEditable() {
        makeTextCellsInteractive(*recipeList, colName, [this](int row, const Wt::WLineEdit& editField, Wt::WString oldContent) {
            if (Wt::WValidator(true).validate(editField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            auto transaction = Wt::Dbo::Transaction(*db);
            auto recipe = (Wt::Dbo::ptr<Recipe>)db->find<Recipe>().where("id = ?").bind(rowToID[row]);
            recipe.modify()->name = editField.text();
            return Wt::WString(editField.text());
        });
    }

    void setupDeleteAction() {
        auto column = findColumn(*recipeList, colDelete);
        if (column == -1)
            return;

        for (auto row = recipeList->headerCount(); row < recipeList->rowCount(); row++) {
            recipeList->elementAt(row, column)->clicked().connect(std::bind([this, row] {
                auto confirmationDialog = new Wt::WDialog(L"Potwierdzenie usunięcia przepisu");
                auto yesButton = new Wt::WPushButton("Tak", confirmationDialog->footer());
                auto noButton = new Wt::WPushButton("Nie", confirmationDialog->footer());
                new Wt::WText(L"Czy napewno usunąć przepis?", confirmationDialog->contents());
                yesButton->clicked().connect(confirmationDialog, &Wt::WDialog::accept);
                noButton->clicked().connect(confirmationDialog, &Wt::WDialog::reject);
                confirmationDialog->rejectWhenEscapePressed();

                confirmationDialog->finished().connect(std::bind([this, confirmationDialog, row] {
                    if (confirmationDialog->result() != Wt::WDialog::Accepted)
                        return;

                    Wt::Dbo::Transaction transaction(*db);

                    auto recipe = (Wt::Dbo::ptr<Recipe>)db->find<Recipe>().where("id = ?").bind(rowToID[row]);
                    recipe.modify()->ingredientRecords.clear();
                    recipe.remove();
                    populateRecipeList();
                    delete confirmationDialog;
                }));

                confirmationDialog->show();
            }));
        }
    }

    void setupGotoDetails() {
        auto column = findColumn(*recipeList, colDetails);
        if (column == -1) {
            column = recipeList->columnCount();
            recipeList->setHeaderCount(1);
            recipeList->elementAt(0, column)->addWidget(new Wt::WText(colDetails));
        }

        for (auto row = recipeList->headerCount(); row < recipeList->rowCount(); row++) {
            auto link = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/recipe"), L"Szczegóły");
            link->clicked().connect(std::bind([row, this] { currentRecipe = rowToID[row]; }));

            recipeList->elementAt(row, recipeList->columnCount() - 1)->addWidget(link);
        }
    }
};
