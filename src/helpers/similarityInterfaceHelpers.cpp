#include "headers/helpers/similarityInterfaceHelpers.h"

void refillSelectorKeepSelection(QComboBox*& selector, const QStringList& files){
    // Freeze the other slots from running while this update happens
    selector->blockSignals(true);
    QString previousSelection = selector->currentData(Qt::UserRole).toString();
    selector->clear();

    // Build the new combobox
    for(const QString& path: files){
        // show only file name
        QStringList testing = path.split("Repos_results/");
        QStringList testing2 = testing[1].split("/");

        if(testing2[2] != "MO")
            continue;

        QString label = QFileInfo(path).fileName();

        selector->addItem(label, path);

        int idx = selector->count() - 1;
        selector->setItemData(idx, path, Qt::ToolTipRole);
    }

    // Accurately updates the index of the selector based on previous selection and new values
    int newIndex = -1;
    if (!previousSelection.isEmpty())
        newIndex = selector->findData(previousSelection, Qt::UserRole);

    if (newIndex < 0 && selector->count() > 0)
        newIndex = 0;
    selector->setCurrentIndex(newIndex);

    // Un-freeze UI
    selector->blockSignals(false);
}

void buildGroupBoxRows(QGroupBox*& group,
                       QLabel*& classificationLabel,
                       QComboBox*& classificationCombo,
                       QLabel*& prLabel,
                       QComboBox*& prCombo,
                       QLabel*& outHunkValueLabel){
    QVBoxLayout *vbox = new QVBoxLayout(group);

    // Row 1: combos for Classification and PRs on the classification
    QHBoxLayout *combosRow = new QHBoxLayout();
    combosRow->addWidget(classificationLabel);
    combosRow->addWidget(classificationCombo);
    combosRow->addSpacing(12);
    combosRow->addWidget(prLabel);
    combosRow->addWidget(prCombo);
    combosRow->addStretch();
    vbox->addLayout(combosRow);

    // Row 2: PR Title
    QHBoxLayout *prTitleRow = new QHBoxLayout();
    QLabel *prTitleText = new QLabel("PR Title:", group);
    QLabel *prTitleValue = new QLabel("-", group);
    prTitleValue->setObjectName("Title");
    prTitleRow->addWidget(prTitleText);
    prTitleRow->addWidget(prTitleValue, 1);
    prTitleRow->addStretch();
    vbox->addLayout(prTitleRow);

    // Row 3: PR Location (hyperlink)
    QHBoxLayout *prLocRow = new QHBoxLayout();
    QLabel *prLocText = new QLabel("PR Link:", group);
    QLabel *prLocValue = new QLabel("-", group);
    prLocValue->setObjectName("URL");

    // Make it behave like a link
    prLocValue->setTextFormat(Qt::RichText);
    prLocValue->setTextInteractionFlags(Qt::TextBrowserInteraction);
    prLocValue->setOpenExternalLinks(true);

    prLocRow->addWidget(prLocText);
    prLocRow->addWidget(prLocValue, 1);
    prLocRow->addStretch();
    vbox->addLayout(prLocRow);

    // Row 4: File Classification
    QHBoxLayout *hunkRow = new QHBoxLayout();
    QLabel *hunkText = new QLabel("File Classification:", group);
    QLabel *hunkValue = new QLabel("-", group);
    hunkValue->setObjectName("Hunk");
    hunkRow->addWidget(hunkText);
    hunkRow->addWidget(hunkValue);
    hunkRow->addStretch();
    vbox->addLayout(hunkRow);

    // Row 5: Similarity / Classification values
    // QHBoxLayout *xyRow = new QHBoxLayout();
    // QLabel *xText  = new QLabel("Similarity:", group);
    // QLabel *similarity = new QLabel("-", group);
    // similarity->setObjectName("Similarity");
    // QLabel *yText  = new QLabel("Hunk Classification:", group);
    // QLabel *classification = new QLabel("-", group);
    // classification->setObjectName("Classification");

    // xyRow->addWidget(xText);
    // xyRow->addWidget(similarity);
    // xyRow->addSpacing(12);
    // xyRow->addWidget(yText);
    // xyRow->addWidget(classification);
    // xyRow->addStretch();
    // vbox->addLayout(xyRow);

    if (outHunkValueLabel)
        outHunkValueLabel = hunkValue;

    group->setLayout(vbox);
}

void changeGroupBoxValues(QGroupBox*& group, const QString &similarity,
                          const QString &classification){
    if (!group)
        return;

    QLabel *xLabel = group->findChild<QLabel*>("Similarity");
    QLabel *yLabel = group->findChild<QLabel*>("Classification");

    if (xLabel)
        xLabel->setText(similarity);

    if (yLabel)
        yLabel->setText(classification);
}

void createSplitWidget(QWidget*& panel, QComboBox*& selector, LineNumberHelpers*& helper, QString& labelName, bool offsetLineNumbers){
    QVBoxLayout *leftRoot = new QVBoxLayout(panel);
    leftRoot->setContentsMargins(0,0,0,0);
    leftRoot->setSpacing(6);

    // Header container so we can force a consistent row height
    QWidget *headerWidget = new QWidget(panel);
    QHBoxLayout *leftFileRow = new QHBoxLayout(headerWidget);
    leftFileRow->setContentsMargins(0,0,0,0);
    leftFileRow->setSpacing(6);

    if (labelName == "Left (src): ") {
        QLabel *hunkLabel = new QLabel("===== ALL HUNKS SHOWING NOW =====", headerWidget);
        hunkLabel->setAlignment(Qt::AlignCenter);
        hunkLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        leftFileRow->addWidget(hunkLabel, 1);
    } else {
        QLabel *leftLabel = new QLabel(labelName, headerWidget);

        selector = new QComboBox(headerWidget);
        selector->setEditable(false);
        selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);

        leftFileRow->addWidget(leftLabel);
        leftFileRow->addWidget(selector, 1);
    }

    // Match the "normal" header height (combo row height) so both panels align
    QComboBox probe(panel);
    probe.setEditable(false);
    headerWidget->setFixedHeight(probe.sizeHint().height());

    leftRoot->addWidget(headerWidget);

    // Editor
    helper = new LineNumberHelpers(panel);
    helper->setOverrideLineNumbers(offsetLineNumbers);
    QFont monospace(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    helper->setFont(monospace);
    leftRoot->addWidget(helper);
}
