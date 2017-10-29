#pragma once
#include <memory>
#include <unordered_map>
#include <Wt/Dbo/Session>
#include <Wt/WContainerWidget>
#include <Wt/WText>
#include <Wt/WLineEdit>
#include <Wt/WDialog>
#include <Wt/WPushButton>
#include <Wt/WDoubleValidator>
#include "Unit.h"
#include "Recipe.h"
#include "Ingredient.h"
#include "helpers.h"

class UnitsWidget : public Wt::WContainerWidget {
   public:
    UnitsWidget(Wt::WContainerWidget*, Database& db) {
        this->db = &db;
        addButton = std::make_unique<Wt::WPushButton>(L"Dodaj jednostkę", this);
        addButton->clicked().connect(this, &UnitsWidget::showAddDialog);

        unitList = std::make_unique<Wt::WTable>(this);
        unitList->addStyleClass("table table-stripped table-bordered");

        populateTableHeader(*unitList, "Nazwa", L"Ilość", L"Jednostka bazowa", L"Usuń");
        populateUnitsList();
    }

    void populateUnitsList() {
        populateUnitsTable();
        makeTableEditable();
        setupDeleteAction();
    }

   private:
    Database* db;
    std::unique_ptr<Wt::WTable> unitList;
    std::unordered_map<int, Wt::Dbo::dbo_traits<Unit>::IdType> rowToID;
    std::unique_ptr<Wt::WPushButton> addButton;

    void showAddDialog() {
        Wt::WDialog* dialog = new Wt::WDialog(L"Dodaj jednostkę");
        auto nameField = createLabeledField<Wt::WLineEdit>("Nazwa", dialog->contents());
        auto quantityField = createLabeledField<Wt::WLineEdit>(L"Ilość", dialog->contents());

        auto baseUnitField = createLabeledField<Wt::WComboBox>("Jednostka bazowa", dialog->contents());
        auto baseUnitIDs = populateComboBox<Unit>(*db, *baseUnitField, [](const Unit& elem) { return elem.name; });
        baseUnitField->insertItem(0, "Brak");
        baseUnitIDs.insert(baseUnitIDs.begin(), Wt::Dbo::dbo_traits<Unit>::invalidId());

        auto validationInfo = new Wt::WText(dialog->contents());

        // setup validators
        auto nameValidator = new Wt::WValidator(true);
        nameField->setValidator(nameValidator);
        auto quantityValidator = new Wt::WDoubleValidator;
        quantityValidator->setMandatory(true);
        quantityField->setValidator(quantityValidator);

        auto addButton = new Wt::WPushButton("Dodaj", dialog->footer());
        addButton->setDefault(true);
        addButton->clicked().connect(std::bind([=] {
            if (nameField->validate() != Wt::WValidator::Valid) {
                validationInfo->setText(Wt::WString(L"Nazwa jednostki nie może być pusta!"));
            } else if (quantityField->validate() != Wt::WValidator::Valid) {
                validationInfo->setText(Wt::WString(L"Podana ilość jest niepoprawna(musi składać się z ciągu cyfr i maksymalnie pojedyńczej kropki)"));
            } else {
                dialog->accept();
            }
        }));

        auto cancelButton = new Wt::WPushButton("Anuluj", dialog->footer());
        cancelButton->clicked().connect(dialog, &Wt::WDialog::reject);
        dialog->rejectWhenEscapePressed();

        dialog->finished().connect(std::bind([=]() {
            if (dialog->result() == Wt::WDialog::Accepted) {
                auto baseUnit = baseUnitIDs[baseUnitField->currentIndex()];
                addUnit(nameField->text(), quantityField->text(), baseUnit);
                populateUnitsList();
            }

            delete dialog;
        }));

        dialog->show();
    }

    void addUnit(const Wt::WString& name, const Wt::WString& quantity, Wt::Dbo::dbo_traits<Unit>::IdType baseUnitID) {
        auto unit = new Unit;  // seems that it's necessary
        unit->name = name;
        unit->quantity = std::stod(quantity);

        Wt::Dbo::Transaction transaction(*db);
        Wt::Dbo::ptr<Unit> baseUnit = db->find<Unit>().where("id = ?").bind(baseUnitID);
        unit->baseUnitID = baseUnit.id();

        db->add<Unit>(unit);
    }

    void populateUnitsTable() {
        rowToID.clear();
        populateTable<Unit>(*db, *unitList, [&](const Wt::Dbo::ptr<Unit>& unit, int row) {
            auto transaction = Wt::Dbo::Transaction{*db};
            rowToID.insert(std::make_pair(row, unit.id()));

            Wt::Dbo::ptr<Unit> baseUnit = db->find<Unit>().where("id = ?").bind(unit->baseUnitID);
            auto baseUnitName = baseUnit.id() != Wt::Dbo::dbo_traits<Unit>::invalidId() ? baseUnit->name : L"Brak";

            return std::vector<Wt::WString>{unit->name, std::to_string(unit->quantity), baseUnitName, "X"};
        },
        [this](const Wt::Dbo::ptr<Unit>& unit) {
            Wt::Dbo::Transaction t{*db};
            return unit->ownerID == this->db->users->find(db->login.user())->user()->firmID; 
        });
    }

    void makeTableEditable() {
        // setup editing name of unit
        makeTextCellsInteractive(*unitList, 0, [&](int row, const Wt::WLineEdit& field, Wt::WString oldContent) {
            if (Wt::WValidator(true).validate(field.text()).state() != Wt::WValidator::Valid) {
                return oldContent;
            }

            Wt::Dbo::Transaction transcation{*db};
            Wt::Dbo::ptr<Unit> unit = db->find<Unit>().where("id = ?").bind(rowToID[row]);

            updateUnits(field.text(), unit->name);
            unit.modify()->name = field.text();

            return Wt::WString(unit->name);
        });

        // setup editing quantity of unit
        makeTextCellsInteractive(*unitList, 1, [&](int row, const Wt::WLineEdit& field, Wt::WString oldContent) {
            Wt::WDoubleValidator validator;
            validator.setMandatory(true);
            if (validator.validate(field.text()).state() != Wt::WValidator::Valid) {
                return oldContent.narrow();
            }

            Wt::Dbo::Transaction transcation{*db};
            Wt::Dbo::ptr<Unit> unit = db->find<Unit>().where("id = ?").bind(rowToID[row]);
            unit.modify()->quantity = std::stod(field.text());
            return std::to_string(unit->quantity);
        });

        // setup editing base unit of unit
        auto unitKeys = std::make_shared<std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>>();
        makeCellsInteractive<Wt::WComboBox>(
            *unitList, 2,
            [this, unitKeys](int row, Wt::WComboBox& editField) {
                auto oldContent = (Wt::WText*)unitList->elementAt(row, 2)->widget(0);
                auto oldUnitName = oldContent->text();

                auto unitIDs =
                    populateComboBox<Unit>(*db, editField, [](const Unit& unit) { return unit.name; },
                                           [this, row](Wt::Dbo::ptr<Unit> potentialNewUnit) {
                                               auto transaction = Wt::Dbo::Transaction(*db);
                                               Wt::Dbo::ptr<Unit> currentUnit = db->find<Unit>().where("id = ?").bind(rowToID[row]);
                                               if (potentialNewUnit.id() == currentUnit.id() || Unit::isDescended(*db, potentialNewUnit.id(), currentUnit.id())) {
                                                   return false;
                                               }
                                               return true;
                                           });

                *unitKeys = std::vector<Wt::Dbo::dbo_traits<Unit>::IdType>{std::move(unitIDs)};
                unitKeys->insert(unitKeys->begin(), Wt::Dbo::dbo_traits<Unit>::invalidId());

                editField.insertItem(0, "Brak");
                auto indexOfOldUnit = editField.findText(oldUnitName);
                editField.setCurrentIndex(indexOfOldUnit);
            },
            [this, unitKeys](int row, const Wt::WComboBox& filledEditField, Wt::WString oldContent) {
                auto transaction = Wt::Dbo::Transaction(*db);
                Wt::Dbo::ptr<Unit> currentUnit = db->find<Unit>().where("id = ?").bind(rowToID[row]);

                if (filledEditField.currentIndex() < 0) {
                    currentUnit.modify()->baseUnitID = Wt::Dbo::dbo_traits<Unit>::invalidId();
                    return oldContent;
                } else {
                    currentUnit.modify()->baseUnitID = (*unitKeys)[filledEditField.currentIndex()];
                    return Wt::WString(filledEditField.currentText());
                }
            });
    }

    void setupDeleteAction() {
        for (auto row = unitList->headerCount(); row < unitList->rowCount(); row++) {
            unitList->elementAt(row, 3)->clicked().connect(std::bind([this, row] {
                Wt::Dbo::Transaction transaction(*db);

                auto unit = (Wt::Dbo::ptr<Unit>)db->find<Unit>().where("id = ?").bind(rowToID[row]);
                {  // units must  be out of scope later(some strange thing happens in WT)
                    auto units = (Wt::Dbo::collection<Wt::Dbo::ptr<Unit>>)db->find<Unit>();
                    for (const auto& potentialBaseUnit : units) {
                        if (potentialBaseUnit->baseUnitID == unit.id()) {
                            auto dialog = new Wt::WDialog(L"Jednostka jest używana");
                            auto okButton = new Wt::WPushButton("OK", dialog->footer());
                            okButton->clicked().connect(dialog, &Wt::WDialog::accept);

                            auto message = Wt::WString(L"Jednoska jest używana jako jednoska bazowa dla ") + potentialBaseUnit->name;
                            message += L", więc nie może zostać usunięta";
                            new Wt::WText(std::move(message), dialog->contents());

                            dialog->finished().connect(std::bind([dialog] { delete dialog; }));

                            dialog->show();
                            return;
                        }
                    }
                }

                auto recipes = (Wt::Dbo::collection<Wt::Dbo::ptr<Recipe>>)db->find<Recipe>();
                for (const auto& recipe : recipes) {
                    for (const auto& ingredientRecord : recipe->ingredientRecords) {
                        if (ingredientRecord->unitID == unit.id()) {
                            auto dialog = new Wt::WDialog(L"Jednoskta jest używana");
                            auto okButton = new Wt::WPushButton("OK", dialog->footer());
                            okButton->clicked().connect(dialog, &Wt::WDialog::accept);

                            auto message = Wt::WString(L"Jednostka jest używana co najmniej w przepisie ") + recipe->name;
                            message += L", więc nie może zostać usunięta.";
                            new Wt::WText(std::move(message), dialog->contents());

                            dialog->finished().connect(std::bind([dialog] { delete dialog; }));

                            dialog->show();
                            return;
                        }
                    }
                }

                auto ingredients = (Wt::Dbo::collection<Wt::Dbo::ptr<Ingredient>>)db->find<Ingredient>();
                for (const auto& ingredient : ingredients) {
                    if (ingredient->unitID == unit.id()) {
                        auto dialog = new Wt::WDialog(L"Jednostka jest używana");
                        auto okButton = new Wt::WPushButton("OK", dialog->footer());
                        okButton->clicked().connect(dialog, &Wt::WDialog::accept);

                        auto message = Wt::WString(L"Jednostka jest używana co najmniej w składniku ") + ingredient->name;
                        message += L", więc nie może zostać usunięta.";
                        new Wt::WText(std::move(message), dialog->contents());

                        dialog->finished().connect(std::bind([dialog] { delete dialog; }));

                        dialog->show();
                        return;
                    }
                }

                unit.remove();
                populateUnitsList();
            }));
        }
    }

    void updateUnits(const Wt::WString& newName, const Wt::WString& oldName) {
        for (auto row = unitList->headerCount(); row < unitList->rowCount(); row++) {
            auto currentName = (Wt::WText*)unitList->elementAt(row, 2)->widget(0);
            if (currentName->text() == oldName) {
                currentName->setText(newName);
            }
        }
    }
};
