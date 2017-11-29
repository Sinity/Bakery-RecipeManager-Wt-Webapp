#pragma once
#include <memory>
#include <unordered_map>
#include <Wt/Dbo/Session>
#include <Wt/WLineEdit>
#include <Wt/WDialog>
#include <Wt/WPushButton>
#include <Wt/WDoubleValidator>
#include <Wt/WIntValidator>
#include "helpers.h"
#include "Ingredient.h"
#include "Unit.h"
#include "Recipe.h"

class IngredientsWidget : public Wt::WContainerWidget {
   public:
    IngredientsWidget(Wt::WContainerWidget*, Database& db) {
        this->db = &db;
        addButton = std::make_unique<Wt::WPushButton>(L"Dodaj składnik", this);
        addButton->clicked().connect(this, &IngredientsWidget::showAddDialog);

        ingredientList = std::make_unique<Wt::WTable>(this);
        ingredientList->addStyleClass("table table-stripped table-bordered");

        if(this->db->users->find(this->db->login.user())->user()->accessLevel == 0)
            populateTableHeader(*ingredientList, "Nazwa", "Energia", L"Tłuszcz", "Kwasy nasycone",
                                L"Węglowodany", "Cukry", L"Białko", L"Sól", L"Jednostka", L"Usuń");
        else
            populateTableHeader(*ingredientList, "Nazwa", "Cena", "Energia", L"Tłuszcz", "Kwasy nasycone",
                                L"Węglowodany", "Cukry", L"Białko", L"Sól", "Jednostka", L"Usuń");
        populateIngredientList();
    }

    void populateIngredientList() {
        populateIngredientTable();
        makeTableEditable();
        setupDeleteAction();
    }

   private:
    Database* db;
    std::unique_ptr<Wt::WTable> ingredientList;
    std::unordered_map<int, Wt::Dbo::dbo_traits<Ingredient>::IdType> rowToID;
    std::unique_ptr<Wt::WPushButton> addButton;

    Wt::WDoubleValidator* createNutritionValidator(Wt::WLineEdit* field) {
        auto validator = new Wt::WDoubleValidator;
        validator->setMandatory(true);
        field->setValidator(validator);
        return validator;
    }

    template<typename Field>
    bool allValid(Field fPtr) {
        return fPtr->validate() == Wt::WValidator::Valid;
    }

    template<typename First, typename... Fields>
    bool allValid(First fieldPtr, Fields... other) {
        return fieldPtr->validate() == Wt::WValidator::Valid && allValid(other...);
    }

    void showAddDialog() {
        Wt::WDialog* dialog = new Wt::WDialog(L"Dodaj składnik");
        auto nameField = createLabeledField<Wt::WLineEdit>("Nazwa", dialog->contents());
        auto priceField = createLabeledField<Wt::WLineEdit>("Cena", dialog->contents());
        auto kcalField = createLabeledField<Wt::WLineEdit>("Energia", dialog->contents());
        auto fatField = createLabeledField<Wt::WLineEdit>(L"Tłuszcze", dialog->contents());
        auto saturatedAcidsField = createLabeledField<Wt::WLineEdit>("Kwasy nasycone", dialog->contents());
        auto carbohydratesField = createLabeledField<Wt::WLineEdit>(L"Węglowodany", dialog->contents());
        auto sugarField = createLabeledField<Wt::WLineEdit>("Cukry", dialog->contents());
        auto proteinField = createLabeledField<Wt::WLineEdit>(L"Białko", dialog->contents());
        auto saltField = createLabeledField<Wt::WLineEdit>(L"Sól", dialog->contents());
        auto unitField = createLabeledField<Wt::WComboBox>("Jednostka", dialog->contents());
        auto unitIDs = populateComboBox<Unit>(*db, *unitField, [](const Unit& elem) { return elem.name; },
            [this](const Wt::Dbo::ptr<Unit>& elem) { return elem->ownerID == db->users->find(db->login.user())->user()->firmID; });
        auto validationInfo = new Wt::WText(dialog->contents());

        // setup validators
        auto nameValidator = new Wt::WValidator(true);
        nameValidator->setMandatory(true);
        nameField->setValidator(nameValidator);
        auto priceValidator = new Wt::WDoubleValidator;
        priceValidator->setMandatory(true);
        priceField->setValidator(priceValidator);
        auto kcalValidator = new Wt::WIntValidator;
        kcalField->setValidator(kcalValidator);
        createNutritionValidator(fatField);
        createNutritionValidator(saturatedAcidsField);
        createNutritionValidator(carbohydratesField);
        createNutritionValidator(sugarField);
        createNutritionValidator(proteinField);
        createNutritionValidator(saltField);


        auto addButton = new Wt::WPushButton("Dodaj", dialog->footer());
        addButton->setDefault(true);
        addButton->clicked().connect(std::bind([=] {
            if (nameField->validate() != Wt::WValidator::Valid) {
                validationInfo->setText(Wt::WString(L"Nazwa składnika nie może być pusta!"));
            } else if (priceField->validate() != Wt::WValidator::Valid) {
                validationInfo->setText(Wt::WString(L"Podana cena jest niepoprawna(musi składać się z ciągu cyfr, z opcjonalną kropką decymalną)"));
            } else if(!allValid(kcalField, fatField, saturatedAcidsField, saturatedAcidsField,
                                carbohydratesField, sugarField, proteinField, saltField)) {
                validationInfo->setText(Wt::WString(L"Wartości odżywcze muszą być wypełnione poprawnie(muszą składać się z ciągu cyfr, z opcjonalną kropką decymalną)"));
            } else if(unitField->currentIndex() < -1) {
                validationInfo->setText(Wt::WString(L"Składnik musi posiadać wybraną jednostkę"));
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
                ingredient->kcal = std::stoi(kcalField->text());
                ingredient->fat = std::stod(fatField->text());
                ingredient->saturatedAcids = std::stod(saturatedAcidsField->text());
                ingredient->carbohydrates = std::stod(carbohydratesField->text());
                ingredient->sugar = std::stod(sugarField->text());
                ingredient->protein = std::stod(proteinField->text());
                ingredient->salt = std::stod(saltField->text());
                ingredient->unitID = unitIDs[unitField->currentIndex()];
                ingredient->ownerID = db->users->find(db->login.user())->user()->firmID;
                db->add<Ingredient>(ingredient);
                populateIngredientList();
            }

            delete dialog;
        }));

        dialog->show();
    }

    void populateIngredientTable() {
        rowToID.clear();
        populateTable<Ingredient>(*db, *ingredientList, [&](const Wt::Dbo::ptr<Ingredient>& ingredient, int row) {
            rowToID.insert(std::make_pair(row, ingredient.id()));
            auto transaction = Wt::Dbo::Transaction{*db};

            Wt::Dbo::ptr<Unit> unit = db->find<Unit>().where("id = ?").bind(ingredient->unitID);
            auto unitName = unit.id() != Wt::Dbo::dbo_traits<Unit>::invalidId() ? unit->name : L"Błędna jednostka";

            if(this->db->users->find(this->db->login.user())->user()->accessLevel == 0)
                return std::vector<Wt::WString>{ingredient->name, std::to_string(ingredient->kcal),
                    std::to_string(ingredient->fat), std::to_string(ingredient->saturatedAcids),
                    std::to_string(ingredient->carbohydrates), std::to_string(ingredient->sugar),
                    std::to_string(ingredient->protein), std::to_string(ingredient->salt), unitName, "X"};

            return std::vector<Wt::WString>{ingredient->name, std::to_string(ingredient->kcal),
                std::to_string(ingredient->fat), std::to_string(ingredient->saturatedAcids),
                std::to_string(ingredient->carbohydrates), std::to_string(ingredient->sugar),
                std::to_string(ingredient->protein), std::to_string(ingredient->salt),
                std::to_string(ingredient->price), unitName, "X"};
        },
        [this](const Wt::Dbo::ptr<Ingredient>& ingredient) {
            Wt::Dbo::Transaction t{*db};
            return ingredient->ownerID == this->db->users->find(db->login.user())->user()->firmID;
        });
    }

    void makeTableEditable() {
        // make ingredient name editable
        makeTextCellsInteractive(*ingredientList, 0, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WValidator(true).validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->name = filledField.text();
            return Wt::WString(ingredient->name);
        });

        // make kcal editable
        makeTextCellsInteractive(*ingredientList, 1, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WIntValidator().validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->kcal = std::stoi(filledField.text());
            return Wt::WString(filledField.text());
        });

        // make fat editable
        makeTextCellsInteractive(*ingredientList, 2, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WDoubleValidator().validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->fat = std::stod(filledField.text());
            return Wt::WString(filledField.text());
        });

        // make saturated acids editable
        makeTextCellsInteractive(*ingredientList, 3, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WDoubleValidator().validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->saturatedAcids = std::stod(filledField.text());
            return Wt::WString(filledField.text());
        });

        // make carbohydrates editable
        makeTextCellsInteractive(*ingredientList, 4, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WDoubleValidator().validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->carbohydrates = std::stod(filledField.text());
            return Wt::WString(filledField.text());
        });

        // make sugar editable
        makeTextCellsInteractive(*ingredientList, 5, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WDoubleValidator().validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->sugar = std::stod(filledField.text());
            return Wt::WString(filledField.text());
        });

        // make protein editable
        makeTextCellsInteractive(*ingredientList, 6, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WDoubleValidator().validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->protein = std::stod(filledField.text());
            return Wt::WString(filledField.text());
        });

        // make salt editable
        makeTextCellsInteractive(*ingredientList, 7, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
            if (Wt::WDoubleValidator().validate(filledField.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transaction(*db);
            Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
            ingredient.modify()->salt = std::stod(filledField.text());
            return Wt::WString(filledField.text());
        });

        // make ingredient price editable
        if(this->db->users->find(this->db->login.user())->user()->accessLevel != 0)
            makeTextCellsInteractive(*ingredientList, 8, [&](int row, const Wt::WLineEdit& filledField, Wt::WString oldContent) {
                Wt::WDoubleValidator validator;
                validator.setMandatory(true);
                if (validator.validate(filledField.text()).state() != Wt::WValidator::Valid) {
                    return oldContent.narrow();
                }

                Wt::Dbo::Transaction transaction(*db);
                Wt::Dbo::ptr<Ingredient> ingredient = db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
                ingredient.modify()->price = std::stod(filledField.text());
                return std::to_string(ingredient->price);
            });

        // make ingredient unit editable
        auto unitKeys = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>>();
        makeCellsInteractive<Wt::WComboBox>(
            *ingredientList, (this->db->users->find(this->db->login.user())->user()->accessLevel == 0 ? 8 : 9),
            [this, unitKeys](int row, Wt::WComboBox& editField) {
                *unitKeys =
                    populateComboBox<Unit>(*db, editField, [](const Unit& unit) { return unit.name; },
                                           [this, row](Wt::Dbo::ptr<Unit> potentialUnit) {
                                               auto transaction = Wt::Dbo::Transaction(*db);
                                               auto ingredient = (Wt::Dbo::ptr<Ingredient>)db->find<Ingredient>().where("id = ?").bind(rowToID[row]);

                                               if (ingredient->unitID == potentialUnit.id() || Unit::isDescended(*db, ingredient->unitID, potentialUnit.id()) ||
                                                   Unit::isDescended(*db, potentialUnit.id(), ingredient->unitID)) {
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

                auto ingredient = (Wt::Dbo::ptr<Ingredient>)db->find<Ingredient>().where("id = ?").bind(rowToID[row]);
                auto unit = (Wt::Dbo::ptr<Unit>)db->find<Unit>().where("id = ?").bind((*unitKeys)[filledEditField.currentIndex()]);
                ingredient.modify()->unitID = unit.id();

                return filledEditField.currentText();
            });
    }

    void setupDeleteAction() {
        for (auto row = ingredientList->headerCount(); row < ingredientList->rowCount(); row++) {
            ingredientList->elementAt(row, (this->db->users->find(this->db->login.user())->user()->accessLevel == 0 ? 9 : 10))->clicked().connect(std::bind([this, row] {
                auto confirmationDialog = new Wt::WDialog(L"Potwierdzenie usunięcia składnika");
                auto yesButton = new Wt::WPushButton("Tak", confirmationDialog->footer());
                auto noButton = new Wt::WPushButton("Nie", confirmationDialog->footer());
                new Wt::WText(L"Czy napewno usunąć składnik?", confirmationDialog->contents());
                yesButton->clicked().connect(confirmationDialog, &Wt::WDialog::accept);
                noButton->clicked().connect(confirmationDialog, &Wt::WDialog::reject);
                confirmationDialog->rejectWhenEscapePressed();

                confirmationDialog->finished().connect(std::bind([this, confirmationDialog, row] {
                    if (confirmationDialog->result() != Wt::WDialog::Accepted)
                        return;

                    Wt::Dbo::Transaction transaction(*db);

                    auto ingredient = (Wt::Dbo::ptr<Ingredient>)db->find<Ingredient>().where("id = ?").bind(rowToID[row]);

                    auto recipes = (Wt::Dbo::collection<Wt::Dbo::ptr<Recipe>>)db->find<Recipe>();
                    for (const auto& recipe : recipes) {
                        for (const auto& ingredientRecord : recipe->ingredientRecords) {
                            if (ingredientRecord->ingredientID == ingredient.id()) {
                                auto dialog = new Wt::WDialog(L"Składnik jest używany");
                                auto okButton = new Wt::WPushButton("OK", dialog->footer());
                                okButton->clicked().connect(dialog, &Wt::WDialog::accept);

                                auto message = Wt::WString(L"Składnik jest używany co najmniej w przepisie ") + recipe->name;
                                message += L", więc nie może zostać usunięty.";
                                new Wt::WText(std::move(message), dialog->contents());

                                dialog->finished().connect(std::bind([dialog] { delete dialog; }));

                                dialog->show();

                                return;
                            }
                        }
                    }

                    ingredient.remove();
                    populateIngredientList();  // deleting screws up references to rows in lambdas inside, so rebuild table
                    delete confirmationDialog;
                }));

                confirmationDialog->show();
            }));
        }
    }
};
