/*
 * Copyright 2019       Thomas Baumgart <tbaumgart@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef MYMONEYMODELBASE_H
#define MYMONEYMODELBASE_H

// ----------------------------------------------------------------------------
// Qt Includes

#include <QAbstractItemModel>

// ----------------------------------------------------------------------------
// KDE Includes

// ----------------------------------------------------------------------------
// Project Includes

#include "kmm_models_export.h"


class KMM_MODELS_EXPORT MyMoneyModelBase : public QAbstractItemModel
{
  Q_OBJECT
public:
  explicit MyMoneyModelBase(QObject* parent);
  virtual ~MyMoneyModelBase();

  /**
   * This method returns a list with the first QModelIndex that mathches the @a name
   * in the Qt::DisplayRole role in the model.
   *
   * @sa
   */
  QModelIndexList indexListByName(const QString& name, const QModelIndex parent = QModelIndex()) const;

  /**
   * This is convenience method returning the value of
   * mapToBaseSource(idx).model()
   *
   * @sa mapToBaseSource
   */
  static const QAbstractItemModel* baseModel(const QModelIndex& idx);

  /**
   * This static method returns the model index in the base model of a
   * QModelIndex @a idx. The method traverses any possible filter
   * model until it finds the base model. In case the index already
   * points to the base model or points to an unknown filter model type
   * it is returned unaltered.
   *
   * @note The following filter models (and any derivatives are supported:
   * QSortFilterProxyModel, KConcatenateRowsProxyModel
   */
  static QModelIndex mapToBaseSource(const QModelIndex& idx);

  /**
   * This method returns a QModelIndex for a stacked @a proxyModel based
   * on the model index @a idx pointing to the base model.
   */
  static QModelIndex mapFromBaseSource(QAbstractItemModel* proxyModel, const QModelIndex& idx);

Q_SIGNALS:
  void modelLoaded() const;

};

#endif // MYMONEYMODELBASE_H