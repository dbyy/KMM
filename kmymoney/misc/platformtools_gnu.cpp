/*
 * This file is part of KMyMoney, A Personal Finance Manager by KDE
 * SPDX-FileCopyrightText: 2017 Marc Hübner <mahueb55@gmail.com>
 * SPDX-FileCopyrightText: 2020 Thomas Baumgart <tbaumgart@kde.org>
 *
 *SPDX-License-Identifier: GPL-2.0-or-laterrg/licenses/>.
 */

#include "platformtools.h"

#include <pwd.h>
#include <unistd.h>
#include <clocale>

#include <QString>

QString platformTools::osUsername()
{
  QString name;
  struct passwd* pwd = getpwuid(geteuid());
  if( pwd != nullptr) {
    name = QString::fromLatin1(pwd->pw_name);
  }
  return name;
}

uint platformTools::processId()
{
  return getpid();
}

platformTools::currencySymbolPosition_t platformTools::currencySymbolPosition(bool negativeValues)
{
  platformTools::currencySymbolPosition_t  rc = platformTools::AfterQuantityMoneyWithSpace;
  struct lconv* lc = std::localeconv();
  if (lc) {
    const char precedes = negativeValues ? lc->n_cs_precedes : lc->p_cs_precedes;
    const char space = negativeValues ? lc->n_sep_by_space : lc->p_sep_by_space;
    if (precedes != 0) {
      rc = (space != 0) ? platformTools::BeforeQuantityMoneyWithSpace : platformTools::BeforeQuantityMoney;
    } else {
      rc = (space != 0) ? platformTools::AfterQuantityMoneyWithSpace : platformTools::AfterQuantityMoney;
    }
  }
  return rc;
}

platformTools::currencySignPosition_t platformTools::currencySignPosition(bool negativeValues)
{
  platformTools::currencySignPosition_t rc = platformTools::PreceedQuantityAndSymbol;
  struct lconv* lc = std::localeconv();
  if (lc) {
    rc = static_cast<platformTools::currencySignPosition_t>(negativeValues ? lc->n_sign_posn : lc->p_sign_posn);
  }
  return rc;
}
