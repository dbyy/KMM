/*
    SPDX-FileCopyrightText: 2019-2020 Thomas Baumgart <tbaumgart@kde.org>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPECIALDATESFILTER_H
#define SPECIALDATESFILTER_H

#include "kmm_models_export.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QSortFilterProxyModel>

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include "ledgersortproxymodel.h"

class SpecialLedgerItemFilterPrivate;
class KMM_MODELS_EXPORT SpecialLedgerItemFilter : public LedgerSortProxyModel
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(SpecialLedgerItemFilter)
    Q_DISABLE_COPY(SpecialLedgerItemFilter)

public:
    typedef enum {
        FilterBalanceNormal,
        FilterBalanceReconciliation,
    } FilterBalanceMode;

    explicit SpecialLedgerItemFilter(QObject* parent);

    /**
     * Reimplemented to propagate sorting to sourceModel
     */
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    void setSourceModel(LedgerSortProxyModel* model);

    /**
     * This method returns the data from the base model and
     * intercepts the balance column and returns a fixed string
     * in case a balance cannot be provided (e.g. sorting is
     * not by date or some data is filtered)
     */
    QVariant data(const QModelIndex& index, int role) const override;

    /**
     * Reimplemented to propagate setting of sort order to sourceModel
     */
    void setLedgerSortOrder(LedgerSortOrder sortOrder) override;

    LedgerSortOrder ledgerSortOrder() const override;

    /**
     * Reimplemented to propagate setting to sourceModel
     */
    void setSortingEnabled(bool enable) override;

    /**
     * Control visibility of reconciliation entries in ledger
     */
    void setShowReconciliationEntries(LedgerViewSettings::ReconciliationHeader show);

    /**
     * Make sure to suppress multiple reconciliation headers following each other
     */
    void setHideReconciledTransactions(bool hide);

    /**
     * Setup how the balance display shall be filtered. In @a FilterBalanceNormal mode
     * balances will be filtered if any filter is active or the transactions are
     * not sorted by date. In @a FilterBalanceReconciliation mode balances
     * will be filtered if any text filter is active, the state filter is not
     * equal to NotReconciled or the transactions are not sorted by date.
     */
    void setFilterBalanceMode(FilterBalanceMode mode);

public Q_SLOTS:
    void forceReload();

protected:
    /**
     * @note Does not call base class implementation
     */
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

    /**
     * Reimplemented to propagate setting to sourceModel
     */
    void doSortOnIdle() override;

private:
    // make sure that only LedgerSortProxyModel models can be used as sources
    void setSourceModel(QAbstractItemModel* model) override;
};

#endif // SPECIALDATESFILTER_H
