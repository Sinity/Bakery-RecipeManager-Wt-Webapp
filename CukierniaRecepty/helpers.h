#pragma once
#include <functional>
#include <Wt/WContainerWidget>
#include <Wt/WText>
#include <Wt/WTable>
#include <Wt/WLabel>
#include <Wt/WComboBox>
#include <Wt/WLineEdit>
#include "database.h"

namespace {
void populateTableHeaderImpl(Wt::WTable& table, int column) {
}

template <class HeadString, class... String>
void populateTableHeaderImpl(Wt::WTable& table, int column, const HeadString& head, const String&... tail) {
    table.elementAt(0, column)->addWidget(new Wt::WText(head));
    populateTableHeaderImpl(table, column + 1, tail...);
}
}

template <class T>
void makeCellsInteractive(Wt::WTable& table, int column, std::function<void(int row, T& editField)> fieldInitializer,
                          std::function<Wt::WString(int row, const T& editField, Wt::WString oldContent)> editAction) {
    for (auto row = table.headerCount(); row < table.rowCount(); row++) {
        table.elementAt(row, column)->clicked().connect(std::bind([=, &table] {
                auto elem = dynamic_cast<Wt::WText*>(table.elementAt(row, column)->widget(0));
                if (!elem) {
                    fprintf(stderr, "makeCellsInteractive(): one of cells contains something else than Wt::WText\n");
                    return;
                }

                auto oldContent = elem->text();

                // setup widget, which is editable representation of table cell
                auto editField = new T;
                fieldInitializer(row, *editField);

                // put editable widget in place of static text that was content of table cell until now
                table.elementAt(row, column)->removeWidget(elem);
                table.elementAt(row, column)->insertWidget(0, editField);

                // setup confirmation of entered data
                editField->enterPressed().connect(std::bind([&table, row, column, editAction, oldContent] {
                    // perform user-defined action on data entered by user to widget setuped here, retrieve string containing final content of this widget
                    auto filledEditField = (T*)table.elementAt(row, column)->widget(0);
                    auto finalText = editAction(row, *filledEditField, std::move(oldContent));

                    // replace editable widget with static text widget containing final content(retreived from caller)
                    table.elementAt(row, column)->removeWidget(filledEditField);
                    table.elementAt(row, column)->insertWidget(0, new Wt::WText(finalText));
                }));
            }));
    }
}

// edit action Returns: final content of the table cell
void makeTextCellsInteractive(Wt::WTable& table, int column,
                              std::function<Wt::WString(int row, const Wt::WLineEdit& editField, Wt::WString oldContent)> editAction) {
    makeCellsInteractive<Wt::WLineEdit>(table, column, [&table, column](int row, Wt::WLineEdit& editField) {
        auto currentTextField = (Wt::WText*)table.elementAt(row, column)->widget(0);
        editField.setText(currentTextField->text());
    }, editAction);
}

// Returns primary keys of objects which are reffered by combo box, order by indices of combo box
template <class T>
std::vector<typename Wt::Dbo::dbo_traits<T>::IdType> populateComboBox(Wt::WComboBox& comboBox, std::function<Wt::WString(const T&)> fieldSelector,
                                                                      std::function<bool(typename Wt::Dbo::ptr<T>)> filter = [](typename Wt::Dbo::ptr<T>) {
                                                                          return true;
                                                                      }) {
    auto transaction = Wt::Dbo::Transaction{db};
    auto records = Wt::Dbo::collection<Wt::Dbo::ptr<T>>{db.find<T>()};
    auto primaryKeys = std::vector<typename Wt::Dbo::dbo_traits<T>::IdType>{};

    for (auto& record : records) {
        if (filter(record)) {
            comboBox.addItem(fieldSelector(*record));
            primaryKeys.emplace_back(record.id());
        }
    }

    return primaryKeys;
}

template <class T>
void populateTable(Wt::WTable& table, std::function<std::vector<Wt::WString>(const Wt::Dbo::ptr<T>& element, int row)> fieldLayoutMapper,
                   std::function<bool(const Wt::Dbo::ptr<T>& element)> filter = [](const Wt::Dbo::ptr<T>& element) { return true; }) {
    while (table.rowCount() != table.headerCount()) {
        table.deleteRow(table.rowCount() - 1);
    }

    auto transaction = Wt::Dbo::Transaction{db};
    auto records = Wt::Dbo::collection<Wt::Dbo::ptr<T>>{db.find<T>()};

    auto row = table.headerCount();
    for (auto& record : records) {
        if (filter(record)) {
            auto mapping = fieldLayoutMapper(record, row);
            for (auto i = 0u; i < mapping.size(); i++) {
                table.elementAt(row, i)->addWidget(new Wt::WText(std::move(mapping[i])));
            }
            row++;
        }
    }
}

template <class... String>
void populateTableHeader(Wt::WTable& table, const String&... contents) {
    table.setHeaderCount(1);
    populateTableHeaderImpl(table, 0, contents...);
}

template <class T, class String>
T* createLabeledField(String labelText, Wt::WContainerWidget* parent) {
    auto label = new Wt::WLabel(std::move(labelText), parent);
    auto lineEdit = new T(parent);
    label->setBuddy(lineEdit);
    return lineEdit;
}
