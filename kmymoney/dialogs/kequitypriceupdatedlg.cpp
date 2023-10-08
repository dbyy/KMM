/*
    SPDX-FileCopyrightText: 2004-2017 Thomas Baumgart <tbaumgart@kde.org>
    SPDX-FileCopyrightText: 2004 Kevin Tambascio <ktambascio@users.sourceforge.net>
    SPDX-FileCopyrightText: 2004-2006 Ace Jones <acejones@users.sourceforge.net>
    SPDX-FileCopyrightText: 2017-2018 Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kequitypriceupdatedlg.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KMessageBox>
#include <KTextEdit>
#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KStandardGuiItem>
#include <KLocalizedString>
#include <KColorScheme>

// ----------------------------------------------------------------------------
// Project Includes

#include "ui_kequitypriceupdatedlg.h"

#include <alkimia/alkonlinequote.h>
#include <alkimia/alkonlinequotesource.h>

#include "dialogenums.h"
#include "icons.h"
#include "kequitypriceupdateconfdlg.h"
#include "kmmonlinequotesprofilemanager.h"
#include "kmymoneyutils.h"
#include "mymoneyaccount.h"
#include "mymoneyexception.h"
#include "mymoneyfile.h"
#include "mymoneymoney.h"
#include "mymoneyprice.h"
#include "mymoneysecurity.h"
#include "mymoneystatement.h"
#include "widgethintframe.h"

#include "kmmyesno.h"

#define SUPPORT_CSV_PRICE_QUOTES 0

class KEquityPriceUpdateDlgPrivate : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(KEquityPriceUpdateDlgPrivate)
    Q_DECLARE_PUBLIC(KEquityPriceUpdateDlg)

public:
    enum Columns {
        WEBID_COL,
        NAME_COL,
        PRICE_COL,
        DATE_COL,
        KMMID_COL,
        SOURCE_COL,
    };

    explicit KEquityPriceUpdateDlgPrivate(KEquityPriceUpdateDlg* qq)
        : QObject(qq)
        , q_ptr(qq)
        , ui(new Ui::KEquityPriceUpdateDlg)
        , m_fUpdateAll(false)
        , m_abortUpdate(false)
        , m_updatingPricePolicy(eDialogs::UpdatePrice::All)
        , m_splitRegex("([0-9a-z\\.]+)[^a-z0-9]+([0-9a-z\\.]+)", QRegularExpression::CaseInsensitiveOption)
        , m_currentItem(nullptr)
    {
    }

    ~KEquityPriceUpdateDlgPrivate()
    {
        delete ui;
    }

    AlkOnlineQuotesProfile* quoteProfile(const QString& profileName)
    {
        auto& manager = KMMOnlineQuotesProfileManager::instance();
        if (profileName.isEmpty()) {
            return manager.profile(QLatin1String("kmymoney5"));
        }
        return manager.profile(profileName);
    }

    void init(const QString& securityId)
    {
        Q_Q(KEquityPriceUpdateDlg);
        ui->setupUi(q);

#if SUPPORT_CSV_PRICE_QUOTES == 0
        // as long as we don't support the CSV import of prices
        // for a period of multiple days, we simply don't show the
        // date entry widgets.
        ui->m_dateContainer->hide();
#endif

        m_fUpdateAll = false;
        QStringList headerList;
        headerList << i18n("ID") << i18nc("Equity name", "Name")
                   << i18n("Price") << i18n("Date");

        ui->lvEquityList->header()->setSortIndicator(0, Qt::AscendingOrder);
        ui->lvEquityList->setColumnWidth(NAME_COL, 125);

        // This is a "get it up and running" hack.  Will replace this in the future.
        headerList << i18nc("Internal identifier", "Internal ID")
                   << i18nc("Online quote source", "Source");
        ui->lvEquityList->setColumnWidth(KMMID_COL, 0);

        ui->lvEquityList->setHeaderLabels(headerList);

        ui->lvEquityList->setSelectionMode(QAbstractItemView::MultiSelection);
        ui->lvEquityList->setAllColumnsShowFocus(true);

        ui->closeStatusButton->setIcon(Icons::get(Icons::Icon::DialogClose));
        connect(ui->closeStatusButton, &QToolButton::clicked, ui->statusContainer, &QWidget::hide);

        // hide the status widgets until there is activity
        ui->statusContainer->hide();

        auto file = MyMoneyFile::instance();

        //
        // Add each price pair that we know about
        //

        // send in securityId == "XXX YYY" to get a single-shot update for XXX to YYY.
        // for consistency reasons, this accepts the same delimiters as AlkOnlineQuote::launch()
        QRegularExpressionMatch match(m_splitRegex.match(securityId));
        MyMoneySecurityPair currencyIds;
        if (match.hasMatch()) {
            currencyIds = MyMoneySecurityPair(match.captured(1), match.captured(2));
        }

        MyMoneyPriceList prices = file->priceList();
        for (MyMoneyPriceList::ConstIterator it_price = prices.constBegin(); it_price != prices.constEnd(); ++it_price) {
            const MyMoneySecurityPair& pair = it_price.key();
            if (file->security(pair.first).isCurrency() && (securityId.isEmpty() || (pair == currencyIds))) {
                if (pair.first.trimmed().isEmpty() || pair.second.trimmed().isEmpty()) {
                    qDebug() << "A currency pair" << pair << "has one of its elements present, while the other one is empty. Omitting.";
                    continue;
                }
                if (!file->security(pair.second).isCurrency()) {
                    qDebug() << "A currency pair" << pair << "is invalid (from currency to equity). Omitting.";
                    continue;
                }
                const MyMoneyPriceEntries& entries = (*it_price);
                if (entries.count() > 0 && entries.begin().key() <= QDate::currentDate()) {
                    addPricePair(pair, false);
                }
            }
        }

        //
        // Add each investment
        //

        QList<MyMoneySecurity> securities = file->securityList();
        for (QList<MyMoneySecurity>::const_iterator it = securities.constBegin(); it != securities.constEnd(); ++it) {
            if (!(*it).isCurrency() //
                    && (securityId.isEmpty() || ((*it).id() == securityId)) //
                    && !(*it).value("kmm-online-source").isEmpty()
               ) {
                addInvestment(*it);
            }
        }

        // if list is empty and a price pair has been requested, add it
        if (ui->lvEquityList->invisibleRootItem()->childCount() == 0 && !securityId.isEmpty()) {
            addPricePair(currencyIds, true);
        }

        connect(ui->btnUpdateSelected, &QAbstractButton::clicked, this, &KEquityPriceUpdateDlgPrivate::slotUpdateSelected);
        connect(ui->btnUpdateAll, &QAbstractButton::clicked, this, &KEquityPriceUpdateDlgPrivate::slotUpdateAll);

        connect(ui->m_fromDate, &KMyMoneyDateEdit::dateChanged, this, &KEquityPriceUpdateDlgPrivate::slotDateChanged);
        connect(ui->m_toDate, &KMyMoneyDateEdit::dateChanged, this, &KEquityPriceUpdateDlgPrivate::slotDateChanged);

        connect(&m_quote, &AlkOnlineQuote::status, this, &KEquityPriceUpdateDlgPrivate::logStatusMessage);
        connect(&m_quote, &AlkOnlineQuote::error, this, &KEquityPriceUpdateDlgPrivate::logErrorMessage);
        connect(&m_quote, &AlkOnlineQuote::failed, this, &KEquityPriceUpdateDlgPrivate::slotQuoteFailed);
        connect(&m_quote, &AlkOnlineQuote::quote, this, &KEquityPriceUpdateDlgPrivate::slotReceivedQuote);
#if SUPPORT_CSV_PRICE_QUOTES
        connect(&m_quote, &AlkOnlineQuote::quotes, this, &KEquityPriceUpdateDlgPrivate::slotReceivedQuotes);
#endif

        connect(ui->lvEquityList, &QTreeWidget::itemSelectionChanged, this, [&]() {
            updateButtonState();
        });

        ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(i18nc("@action:button Configuration of price update", "Configure"));
        connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, this, [&]() {
            QPointer<EquityPriceUpdateConfDlg> dlg = new EquityPriceUpdateConfDlg(m_updatingPricePolicy);
            if (dlg->exec() == QDialog::Accepted && (dlg != nullptr)) {
                m_updatingPricePolicy = dlg->policy();
            }
            delete dlg;
        });

        if (!securityId.isEmpty()) {
            ui->btnUpdateSelected->hide();
            ui->btnUpdateAll->hide();

            QTimer::singleShot(100, this, &KEquityPriceUpdateDlgPrivate::slotUpdateAll);
        }

        // Hide OK button until we have received the first update
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        updateButtonState();

        // previous versions of this dialog allowed to store a "Don't ask again" switch.
        // Since we don't support it anymore, we just get rid of it
        KSharedConfigPtr config = KSharedConfig::openConfig();
        KConfigGroup grp = config->group("Notification Messages");
        grp.deleteEntry("KEquityPriceUpdateDlg::slotQuoteFailed::Price Update Failed");
        grp.sync();
        grp = config->group("Equity Price Update");
        int policyValue = grp.readEntry("PriceUpdatingPolicy", (int)eDialogs::UpdatePrice::Missing);
        if (policyValue > (int)eDialogs::UpdatePrice::Ask || policyValue < (int)eDialogs::UpdatePrice::All)
            m_updatingPricePolicy = eDialogs::UpdatePrice::Missing;
        else
            m_updatingPricePolicy = static_cast<eDialogs::UpdatePrice>(policyValue);
    }

    void updateButtonState()
    {
        if (ui->m_fromDate->date().isValid() && ui->m_toDate->date().isValid()) {
            // Only enable the update all button if there is an item in the list
            ui->btnUpdateAll->setEnabled(ui->lvEquityList->invisibleRootItem()->childCount() > 0);
            // Only enable the update button if there is a selection
            ui->btnUpdateSelected->setEnabled(!ui->lvEquityList->selectedItems().empty());
        } else {
            // upon invalid date, disable all buttons
            ui->btnUpdateAll->setEnabled(false);
            ui->btnUpdateSelected->setEnabled(false);
        }
    }

    void addPricePair(const MyMoneySecurityPair& pair, bool dontCheckExistance)
    {
        Q_ASSERT(!pair.first.trimmed().isEmpty());
        Q_ASSERT(!pair.second.trimmed().isEmpty());

        auto file = MyMoneyFile::instance();

        const auto symbol = QString::fromLatin1("%1 > %2").arg(pair.first, pair.second);
        const auto id = QString::fromLatin1("%1 %2").arg(pair.first, pair.second);
        // Check that the pair does not already exist
        if (ui->lvEquityList->findItems(id, Qt::MatchExactly, KMMID_COL).empty()) {
            const MyMoneyPrice &pr = file->price(pair.first, pair.second);
            if (pr.source() != QLatin1String("KMyMoney")) {
                bool keep = true;
                if ((pair.first == file->baseCurrency().id())
                        || (pair.second == file->baseCurrency().id())) {
                    const QString& foreignCurrency = file->foreignCurrency(pair.first, pair.second);
                    // check that the foreign currency is still in use
                    QList<MyMoneyAccount>::const_iterator it_a;
                    QList<MyMoneyAccount> list;
                    file->accountList(list);
                    for (it_a = list.constBegin(); !dontCheckExistance && it_a != list.constEnd(); ++it_a) {
                        // if it's an account denominated in the foreign currency
                        // keep it
                        if (((*it_a).currencyId() == foreignCurrency)
                                && !(*it_a).isClosed())
                            break;
                        // if it's an investment traded in the foreign currency
                        // keep it
                        if ((*it_a).isInvest() && !(*it_a).isClosed()) {
                            MyMoneySecurity sec = file->security((*it_a).currencyId());
                            if (sec.tradingCurrency() == foreignCurrency)
                                break;
                        }
                    }
                    // if it is in use, it_a is not equal to list.end()
                    if (it_a == list.constEnd() && !dontCheckExistance)
                        keep = false;
                }

                if (keep) {
                    auto item = new QTreeWidgetItem();
                    item->setText(WEBID_COL, symbol);
                    item->setText(NAME_COL, i18n("%1 units in %2", pair.first, pair.second));
                    if (pr.isValid()) {
                        MyMoneySecurity fromCurrency = file->currency(pair.second);
                        MyMoneySecurity toCurrency = file->currency(pair.first);
                        item->setText(PRICE_COL, pr.rate(pair.second).formatMoney(fromCurrency.tradingSymbol(), toCurrency.pricePrecision()));
                        item->setText(DATE_COL, pr.date().toString(Qt::ISODate));
                    }
                    item->setText(KMMID_COL, id);
                    item->setText(SOURCE_COL, "KMyMoney Currency");  // This string value should not be localized
                    ui->lvEquityList->invisibleRootItem()->addChild(item);
                }
            }
        }
    }

    void addInvestment(const MyMoneySecurity& inv)
    {
        const auto id = inv.id();
        // Check that the pair does not already exist
        if (ui->lvEquityList->findItems(id, Qt::MatchExactly, KMMID_COL).empty()) {
            auto file = MyMoneyFile::instance();
            // check that the security is still in use
            QList<MyMoneyAccount>::const_iterator it_a;
            QList<MyMoneyAccount> list;
            file->accountList(list);
            for (it_a = list.constBegin(); it_a != list.constEnd(); ++it_a) {
                if ((*it_a).isInvest()
                        && ((*it_a).currencyId() == inv.id())
                        && !(*it_a).isClosed())
                    break;
            }
            // if it is in use, it_a is not equal to list.end()
            if (it_a != list.constEnd()) {
                QString webID;
                AlkOnlineQuoteSource alkOnlineSource(inv.value(QLatin1String("kmm-online-source")),
                                                     quoteProfile(inv.value(QLatin1String("kmm-online-quote-system"))));
                if (alkOnlineSource.idSelector() == AlkOnlineQuoteSource::IdentificationNumber) {
                    webID = inv.value("kmm-security-id");   // insert ISIN number...
                } else if (alkOnlineSource.idSelector() == AlkOnlineQuoteSource::Name) {
                    webID = inv.name();                     // ...or name...
                } else {
                    webID = inv.tradingSymbol();            // ...or symbol
                }

                QTreeWidgetItem* item = new QTreeWidgetItem();
                item->setForeground(WEBID_COL, KColorScheme(QPalette::Normal).foreground(KColorScheme::NormalText));
                if (webID.isEmpty()) {
                    webID = i18n("[No identifier]");
                    item->setForeground(WEBID_COL, KColorScheme(QPalette::Normal).foreground(KColorScheme::NegativeText));
                }
                item->setText(WEBID_COL, webID);
                item->setText(NAME_COL, inv.name());
                MyMoneySecurity currency = file->currency(inv.tradingCurrency());
                const MyMoneyPrice &pr = file->price(id.toUtf8(), inv.tradingCurrency());
                if (pr.isValid()) {
                    item->setText(PRICE_COL, pr.rate(currency.id()).formatMoney(currency.tradingSymbol(), inv.pricePrecision()));
                    item->setText(DATE_COL, pr.date().toString(Qt::ISODate));
                }
                item->setText(KMMID_COL, id);
                if (inv.value("kmm-online-quote-system") == "Finance::Quote")
                    item->setText(SOURCE_COL, QString("Finance::Quote %1").arg(inv.value("kmm-online-source")));
                else
                    item->setText(SOURCE_COL, inv.value("kmm-online-source"));

                ui->lvEquityList->invisibleRootItem()->addChild(item);

                // If this investment is denominated in a foreign currency, ensure that
                // the appropriate price pair is also on the list

                if (currency.id() != file->baseCurrency().id()) {
                    addPricePair(MyMoneySecurityPair(currency.id(), file->baseCurrency().id()), false);
                }
            }
        }
    }

    void slotDateChanged()
    {
        QSignalBlocker blockFrom(ui->m_fromDate);
        QSignalBlocker blockTo(ui->m_toDate);
        if (ui->m_toDate->date() > QDate::currentDate())
            ui->m_toDate->setDate(QDate::currentDate());
        if (ui->m_fromDate->date() > ui->m_toDate->date())
            ui->m_fromDate->setDate(ui->m_toDate->date());
    }

    void slotUpdateSelected()
    {
        // disable sorting while the update is running to maintain the current order of items on which
        // the update process depends and which could be changed with sorting enabled due to the updated values
        ui->lvEquityList->setSortingEnabled(false);

        // show the status widgets
        ui->statusContainer->show();
        ui->prgOnlineProgress->show();

        ui->prgOnlineProgress->setMaximum(1 + ui->lvEquityList->model()->rowCount());

        QTreeWidgetItemIterator it(ui->lvEquityList);
        int processed(1);
        bool noneProcessed(true);

        const QString fqName = QLatin1String("Finance::Quote");

        while (*it && !m_abortUpdate) {
            ui->prgOnlineProgress->setValue(processed);
            if ((*it)->isSelected() || m_fUpdateAll) {
                noneProcessed = false;
                m_currentItem = (*it);
                const auto profileName = m_currentItem->text(SOURCE_COL).startsWith(fqName) ? fqName : QString();
                m_quote.setProfile(quoteProfile(profileName));
                m_quote.launch((*it)->text(WEBID_COL), (*it)->text(KMMID_COL), (*it)->text(SOURCE_COL));
            }
            ++it;
            ++processed;
        }

        // we've run past the end, reset to the default value.
        m_fUpdateAll = false;

        // force progress bar to show 100% and hide it after a while
        ui->prgOnlineProgress->setValue(ui->prgOnlineProgress->maximum());
        QTimer::singleShot(500, ui->prgOnlineProgress, &QProgressBar::hide);

        // re-enable the sorting that was disabled during the update process
        ui->lvEquityList->setSortingEnabled(true);

        if (noneProcessed) {
            logErrorMessage(i18nc("@info online update price info", "No security selected."));
        }
    }

    void slotUpdateAll()
    {
        if (ui->lvEquityList->model()->rowCount() > 0) {
            m_fUpdateAll = true;
            slotUpdateSelected();
        } else {
            logErrorMessage(i18nc("@info online update price info", "No Security to be updated."));
        }
    }

    void logErrorMessage(const QString& message)
    {
        logStatusMessage(QString("<font color=\"red\"><b>") + message + QString("</b></font>"));
    }

    void logStatusMessage(const QString& message)
    {
        ui->lbStatus->append(message);
    }

    void slotQuoteFailed(const QString& _kmmID, const QString& _webID)
    {
        Q_Q(KEquityPriceUpdateDlg);
        // Give the user some options
        int result;
        if (m_currentItem) {
            if (_kmmID.contains(" ")) {
                result = KMessageBox::warningContinueCancel(
                    q,
                    i18n("Failed to retrieve an exchange rate for %1 from %2. It will be skipped this time.", _webID, m_currentItem->text(SOURCE_COL)),
                    i18n("Price Update Failed"));
            } else {
                result = KMessageBox::questionTwoActionsCancel(
                    q,
                    QString::fromLatin1("<qt>%1</qt>")
                        .arg(i18n(
                            "Failed to retrieve a quote for %1 from %2.  Press <b>No</b> to remove the online price source from this security permanently, "
                            "<b>Yes</b> to continue updating this security during future price updates or <b>Cancel</b> to stop the current update operation.",
                            _webID,
                            m_currentItem->text(SOURCE_COL))),
                    i18n("Price Update Failed"),
                    KMMYesNo::yes(),
                    KMMYesNo::no());
            }

            if (result == KMessageBox::SecondaryAction) {
                // Disable price updates for this security

                MyMoneyFileTransaction ft;
                try {
                    // Get this security (by ID)
                    MyMoneySecurity security = MyMoneyFile::instance()->security(_kmmID.toUtf8());

                    // Set the quote source to blank
                    security.setValue("kmm-online-source", QString());
                    security.setValue("kmm-online-quote-system", QString());

                    // Re-commit the security
                    MyMoneyFile::instance()->modifySecurity(security);
                    ft.commit();
                } catch (const MyMoneyException& e) {
                    KMessageBox::error(q,
                                       QString("<qt>") + i18n("Cannot update security <b>%1</b>: %2", _webID, QString::fromLatin1(e.what())) + QString("</qt>"),
                                       i18n("Price Update Failed"));
                }
            }

            // As long as the user doesn't want to cancel, move on!
            m_abortUpdate = (result == KMessageBox::Cancel);
        }
    }

    void slotReceivedQuote(const QString& _kmmID, const QString& _webID, const QDate& _date, const double& _price)
    {
        if (m_currentItem) {
            if (_price > 0.0f && _date.isValid()) {
                QDate date = _date;
                if (date > QDate::currentDate())
                    date = QDate::currentDate();

                MyMoneyMoney price = MyMoneyMoney::ONE;
                QString id = _kmmID.toUtf8();
                MyMoneySecurity fromCurrency, toCurrency;
                if (_kmmID.contains(" ") == 0) {
                    MyMoneySecurity security = MyMoneyFile::instance()->security(id);
                    QString factor = security.value("kmm-online-factor");
                    if (!factor.isEmpty()) {
                        price = price * MyMoneyMoney(factor);
                    }
                    try {
                        toCurrency = MyMoneyFile::instance()->security(id);
                        fromCurrency = MyMoneyFile::instance()->security(toCurrency.tradingCurrency());
                    } catch (const MyMoneyException&) {
                        fromCurrency = toCurrency = MyMoneySecurity();
                    }

                } else {
                    QRegularExpressionMatch match(m_splitRegex.match(_kmmID));
                    if (match.hasMatch()) {
                        try {
                            fromCurrency = MyMoneyFile::instance()->security(match.captured(2).toUtf8());
                            toCurrency = MyMoneyFile::instance()->security(match.captured(1).toUtf8());
                        } catch (const MyMoneyException&) {
                            fromCurrency = toCurrency = MyMoneySecurity();
                        }
                    }
                }
                price *= MyMoneyMoney(_price, MyMoneyMoney::precToDenom(toCurrency.pricePrecision()));

                m_currentItem->setText(PRICE_COL, price.formatMoney(fromCurrency.tradingSymbol(), toCurrency.pricePrecision()));
                m_currentItem->setText(DATE_COL, date.toString(Qt::ISODate));
                logStatusMessage(i18nc("@info Online price update %1 online id, %2 internal id", "Price for %1 updated (id %2)", _webID, _kmmID));
                // make sure to make OK button available
                ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
            } else {
                logErrorMessage(i18nc("@info Online price update %1 online id", "Received an invalid price for %1, unable to update.", _webID));
            }

            m_currentItem->setSelected(false);
        }
    }

#if SUPPORT_CSV_PRICE_QUOTES
    void slotReceivedQuotes(const QString& securityId, const QString& symbol, const AlkDatePriceMap& quotes)
    {
        auto foundItems = ui->lvEquityList->findItems(securityId, Qt::MatchExactly, KEquityPriceUpdateDlgPrivate::KMMID_COL);
        QTreeWidgetItem* item = nullptr;

        if (!foundItems.empty())
            item = foundItems.at(0);

        QTreeWidgetItem* next = nullptr;

        if (item) {
            auto file = MyMoneyFile::instance();
            MyMoneySecurity fromCurrency, toCurrency;

            if (!securityId.contains(QLatin1Char(' '))) {
                try {
                    toCurrency = MyMoneyFile::instance()->security(securityId);
                    fromCurrency = MyMoneyFile::instance()->security(toCurrency.tradingCurrency());
                } catch (const MyMoneyException&) {
                    fromCurrency = toCurrency = MyMoneySecurity();
                }

            } else {
                QRegularExpressionMatch match(m_splitRegex.match(securityId));
                if (match.hasMatch()) {
                    try {
                        fromCurrency = MyMoneyFile::instance()->security(match.captured(2));
                        toCurrency = MyMoneyFile::instance()->security(match.captured(1));
                    } catch (const MyMoneyException&) {
                        fromCurrency = toCurrency = MyMoneySecurity();
                    }
                }
            }

#if 1
            if (m_updatingPricePolicy != eDialogs::UpdatePrice::All) {
                QStringList qSources = WebPriceQuote::quoteSources();
                for (auto it = st.m_listPrices.begin(); it != st.m_listPrices.end();) {
                    MyMoneyPrice storedPrice = file->price(toCurrency.id(), fromCurrency.id(), (*it).m_date, true);
                    bool priceValid = storedPrice.isValid();
                    if (!priceValid)
                        ++it;
                    else {
                        switch (m_updatingPricePolicy) {
                        case eDialogs::UpdatePrice::Missing:
                            it = st.m_listPrices.erase(it);
                            break;
                        case eDialogs::UpdatePrice::Downloaded:
                            if (!qSources.contains(storedPrice.source()))
                                it = st.m_listPrices.erase(it);
                            else
                                ++it;
                            break;
                        case eDialogs::UpdatePrice::SameSource:
                            if (storedPrice.source().compare((*it).m_sourceName) != 0)
                                it = st.m_listPrices.erase(it);
                            else
                                ++it;
                            break;
                        case eDialogs::UpdatePrice::Ask: {
                            auto result = KMessageBox::questionTwoActionsCancel(this,
                                                                                i18n("For <b>%1</b> on <b>%2</b> price <b>%3</b> already exists.<br>"
                                                                                     "Do you want to replace it with <b>%4</b>?",
                                                                                     storedPrice.from(),
                                                                                     storedPrice.date().toString(Qt::ISODate),
                                                                                     QString().setNum(storedPrice.rate(storedPrice.to()).toDouble(), 'g', 10),
                                                                                     QString().setNum((*it).m_amount.toDouble(), 'g', 10)),
                                                                                i18n("Price Already Exists"),
                                                                                KMMYesNo::yes(),
                                                                                KMMYesNo::no());
                            switch (result) {
                            case KMessageBox::ButtonCode::PrimaryAction:
                                ++it;
                                break;
                            case KMessageBox::ButtonCode::SecondaryAction:
                                it = st.m_listPrices.erase(it);
                                break;
                            default:
                            case KMessageBox::ButtonCode::Cancel:
                                finishUpdate();
                                return;
                                break;
                            }
                            break;
                        }
                        default:
                            ++it;
                            break;
                        }
                    }
                }
            }

            if (!quotes.isEmpty()) {
                MyMoneyFileTransaction ft;
                KMyMoneyUtils::processPriceList(st);
                ft.commit();

                // latest price could be in the last or in the first row
                MyMoneyStatement::Price priceClass;
                QDate firstEntry;
                QDate lastEntry;
                if (st.m_listPrices.first().m_date > st.m_listPrices.last().m_date) {
                    priceClass = st.m_listPrices.first();
                    firstEntry = st.m_listPrices.last().m_date;
                } else {
                    priceClass = st.m_listPrices.last();
                    firstEntry = st.m_listPrices.first().m_date;
                }
                lastEntry = priceClass.m_date;
                // update latest price in dialog if applicable
                auto latestDate = QDate::fromString(item->text(KEquityPriceUpdateDlgPrivate::DATE_COL), Qt::ISODate);
                if (latestDate <= priceClass.m_date && priceClass.m_amount.isPositive()) {
                    item->setText(KEquityPriceUpdateDlgPrivate::PRICE_COL,
                                  priceClass.m_amount.formatMoney(fromCurrency.tradingSymbol(), toCurrency.pricePrecision()));
                    item->setText(KEquityPriceUpdateDlgPrivate::DATE_COL, priceClass.m_date.toString(Qt::ISODate));
                    item->setText(KEquityPriceUpdateDlgPrivate::SOURCE_COL, priceClass.m_sourceName);
                }
                logStatusMessage(i18nc("Log message e.g. 'Prices for EUR > USD updated between 2001-01-01 and 2001-02-01 (id EUR USD)'",
                                       "Prices for %1 updated between %3 and %4 (id %2)",
                                       symbol,
                                       securityId,
                                       firstEntry.toString(Qt::ISODate),
                                       lastEntry.toString(Qt::ISODate)));
                // make sure to make OK button available
            }
            d->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);

            d->ui->prgOnlineProgress->setValue(d->ui->prgOnlineProgress->value() + 1);
            item->setSelected(false);

            // launch the NEXT one ... in case of m_fUpdateAll == false, we
            // need to parse the list to find the next selected one
            next = d->ui->lvEquityList->invisibleRootItem()->child(d->ui->lvEquityList->invisibleRootItem()->indexOfChild(item) + 1);
            if (!d->m_fUpdateAll) {
                while (next && !next->isSelected()) {
                    d->ui->prgOnlineProgress->setValue(d->ui->prgOnlineProgress->value() + 1);
                    next = d->ui->lvEquityList->invisibleRootItem()->child(d->ui->lvEquityList->invisibleRootItem()->indexOfChild(next) + 1);
                }
            }
#endif
        } else {
            logErrorMessage(i18n("Received a price for %1 (id %2), but this symbol is not on the list. Aborting entire update.", symbol, securityId));
        }

#if 0
        if (next) {
            d->m_quote.launch(next->text(WEBID_COL), next->text(KMMID_COL), next->text(SOURCE_COL));
        } else {
            finishUpdate();
        }
#endif
    }
#endif // SUPPORT_CSV

    KEquityPriceUpdateDlg* q_ptr;
    Ui::KEquityPriceUpdateDlg* ui;
    bool m_fUpdateAll;
    bool m_abortUpdate;
    eDialogs::UpdatePrice m_updatingPricePolicy;
    QRegularExpression m_splitRegex;
    AlkOnlineQuote m_quote;
    QTreeWidgetItem* m_currentItem;
};

KEquityPriceUpdateDlg::KEquityPriceUpdateDlg(QWidget *parent, const QString& securityId) :
    QDialog(parent),
    d_ptr(new KEquityPriceUpdateDlgPrivate(this))
{
    Q_D(KEquityPriceUpdateDlg);
    d->init(securityId);

    auto decorateEditWidget = [&](const QDate& date, QWidget* widget) {
        if (widget) {
            WidgetHintFrame::hide(widget, QString());
            if (!date.isValid()) {
                WidgetHintFrame::show(widget, i18nc("@info:tooltip", "The date is invalid."));
            }
        }
    };

    auto frameCollection = new WidgetHintFrameCollection(this);
    frameCollection->addFrame(new WidgetHintFrame(d->ui->m_fromDate));
    frameCollection->addFrame(new WidgetHintFrame(d->ui->m_toDate));

    connect(d->ui->m_fromDate, &KMyMoneyDateEdit::dateValidityChanged, this, [&](const QDate& date) {
        Q_D(KEquityPriceUpdateDlg);
        decorateEditWidget(date, qobject_cast<QWidget*>(sender()));
        d->updateButtonState();
    });
    connect(d->ui->m_toDate, &KMyMoneyDateEdit::dateValidityChanged, this, [&](const QDate& date) {
        Q_D(KEquityPriceUpdateDlg);
        decorateEditWidget(date, qobject_cast<QWidget*>(sender()));
        d->updateButtonState();
    });
}

KEquityPriceUpdateDlg::~KEquityPriceUpdateDlg()
{
    Q_D(KEquityPriceUpdateDlg);
    auto config = KSharedConfig::openConfig();
    auto grp = config->group("Equity Price Update");
    grp.writeEntry("PriceUpdatingPolicy", static_cast<int>(d->m_updatingPricePolicy));
    grp.sync();
    delete d;
}

MyMoneyPrice KEquityPriceUpdateDlg::price(const QString& id) const
{
    Q_D(const KEquityPriceUpdateDlg);
    MyMoneyPrice price;
    QList<QTreeWidgetItem*> foundItems = d->ui->lvEquityList->findItems(id, Qt::MatchExactly, KEquityPriceUpdateDlgPrivate::KMMID_COL);

    if (!foundItems.empty()) {
        const auto* const item = foundItems.at(0);
        MyMoneyMoney rate(item->text(KEquityPriceUpdateDlgPrivate::PRICE_COL));
        if (!rate.isZero()) {
            QString kmm_id = item->text(KEquityPriceUpdateDlgPrivate::KMMID_COL).toUtf8();

            // if the ID has a space, then this is TWO ID's, so it's a currency quote
            if (kmm_id.contains(" ")) {
                QStringList ids = kmm_id.split(' ', Qt::SkipEmptyParts);
                QString fromid = ids[0].toUtf8();
                QString toid = ids[1].toUtf8();
                price = MyMoneyPrice(fromid,
                                     toid,
                                     QDate().fromString(item->text(KEquityPriceUpdateDlgPrivate::DATE_COL), Qt::ISODate),
                                     rate,
                                     item->text(KEquityPriceUpdateDlgPrivate::SOURCE_COL));
            } else
                // otherwise, it's a security quote
            {
                MyMoneySecurity security = MyMoneyFile::instance()->security(kmm_id);
                price = MyMoneyPrice(kmm_id,
                                     security.tradingCurrency(),
                                     QDate().fromString(item->text(KEquityPriceUpdateDlgPrivate::DATE_COL), Qt::ISODate),
                                     rate,
                                     item->text(KEquityPriceUpdateDlgPrivate::SOURCE_COL));
            }
        }
    }
    return price;
}

void KEquityPriceUpdateDlg::storePrices()
{
    Q_D(KEquityPriceUpdateDlg);
    // update the new prices into the equities

    auto file = MyMoneyFile::instance();
    QString name;

    MyMoneyFileTransaction ft;
    try {
        for (auto i = 0; i < d->ui->lvEquityList->invisibleRootItem()->childCount(); ++i) {
            QTreeWidgetItem* item = d->ui->lvEquityList->invisibleRootItem()->child(i);
            // turn on signals before we modify the last entry in the list
            file->blockSignals(i < d->ui->lvEquityList->invisibleRootItem()->childCount() - 1);

            MyMoneyMoney rate(item->text(KEquityPriceUpdateDlgPrivate::PRICE_COL));
            if (!rate.isZero()) {
                QString id = item->text(KEquityPriceUpdateDlgPrivate::KMMID_COL);
                QString fromid;
                QString toid;

                // if the ID has a space, then this is TWO ID's, so it's a currency quote
                if (id.contains(QLatin1Char(' '))) {
                    QStringList ids = id.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    fromid = ids.at(0);
                    toid = ids.at(1);
                    name = QString::fromLatin1("%1 --> %2").arg(fromid, toid);
                } else { // otherwise, it's a security quote
                    MyMoneySecurity security = file->security(id);
                    name = security.name();
                    fromid = id;
                    toid = security.tradingCurrency();
                }
                // TODO (Ace) Better handling of the case where there is already a price
                // for this date.  Currently, it just overrides the old value.  Really it
                // should check to see if the price is the same and prompt the user.
                file->addPrice(MyMoneyPrice(fromid,
                                            toid,
                                            QDate::fromString(item->text(KEquityPriceUpdateDlgPrivate::DATE_COL), Qt::ISODate),
                                            rate,
                                            item->text(KEquityPriceUpdateDlgPrivate::SOURCE_COL)));
            }
        }
        ft.commit();

    } catch (const MyMoneyException &) {
        qDebug("Unable to add price information for %s", qPrintable(name));
    }
}

#include "kequitypriceupdatedlg.moc"
