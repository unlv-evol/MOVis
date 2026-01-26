#ifndef SIMILARITYINTERFACEHELPERS_H
#define SIMILARITYINTERFACEHELPERS_H

#include <QComboBox>
#include <QStringList>
#include <QGroupBox>
#include <QLabel>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include "../LineNumberHelpers.h"

void refillSelectorKeepSelection(QComboBox*& selector, const QStringList& files);
void buildGroupBoxRows(QGroupBox*& group,
                       QLabel*& classificationLabel,
                       QComboBox*& classificationCombo,
                       QLabel*& prLabel,
                       QComboBox*& prCombo,
                       QLabel*& outHunkValueLabel);
void changeGroupBoxValues(QGroupBox*& group, const QString &similarity,
                          const QString &classification);
void createSplitWidget(QWidget*& panel, QComboBox*& selector, LineNumberHelpers*& helper, QString& labelName, bool offsetLineNumbers);

#endif // SIMILARITYINTERFACEHELPERS_H
