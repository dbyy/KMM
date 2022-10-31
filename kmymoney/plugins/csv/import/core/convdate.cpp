/*
    SPDX-FileCopyrightText: 2010-2012 Allan Anderson <agander93@gmail.com>
    SPDX-FileCopyrightText: 2016-2017 Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "convdate.h"

#include <QDebug>
#include <QLocale>
#include <QRegularExpression>

ConvertDate::ConvertDate():
    m_dateFormatIndex(DateFormat::YearMonthDay)

{
}

ConvertDate::~ConvertDate()
{
}

QDate ConvertDate::convertDate(const QString& txt)
{
    QString aYear;
    QString aMonth;
    QString aDay;
    QString aFormat;
    QString dat;
    QDate  aDate;

    aDate = QDate::fromString(txt, Qt::DateFormat::ISODate);

    if (aDate.isValid())
        return aDate;

    QString dateFormatString = stringFormat();

    QRegularExpression rx(QStringLiteral("[\\. :-]"));  //                           replace date field separators '.' ' ' ':' '-'
    QString buffer = txt.trimmed();

    // in case we detect more than 3 sections, it could be,
    // that a single dot is appended to a monthname. We
    // remove this single dot before we continue
    if (buffer.count(rx) > 2) {
        if (buffer.count(QLatin1Char('.')) == 1) {
            buffer.remove(QLatin1Char('.'));
        }
    }

    buffer = buffer.replace(rx, QString::fromLatin1("/"));   //     ....with '/'
    int count = buffer.count(QLatin1Char('/'), Qt::CaseSensitive);
    if (count == 0) {      //                              no separators so use QDate()
        QDate result  = QDate::fromString(buffer, dateFormatString);
        qDebug() << "convertDate1" << txt << buffer << dateFormatString << result;
        if (result.year() < 1950) {
            result = QDate();
        }
        return result;
    }

    QStringList dateSplit = buffer.split(QLatin1Char('/'));
    if (dateSplit.count() > 3) {      //                  it can be date and time
//    qDebug("ConvertDate - assuming date and time format");
        bool dateFound = false;
        for (int i = 0; i < dateSplit.count(); i++) {
            if(dateSplit[i].length() == 4 && dateSplit[i].toInt() > 0) {
                switch (m_dateFormatIndex) {
                case DateFormat::YearMonthDay:   //                                 %y %m %d
                    if(i+2 < dateSplit.count()) {
                        aYear =  dateSplit[i];
                        aMonth = dateSplit[i+1];
                        aDay =   dateSplit[i+2];
                        dateFound = true;
                    }
                    break;
                case DateFormat::MonthDayYear:   //                                 %m %d %y
                    if(i-2 >= 0) {
                        aMonth = dateSplit[i-2];
                        aDay =   dateSplit[i-1];
                        aYear =  dateSplit[i];
                        dateFound = true;
                    }
                    break;
                case DateFormat::DayMonthYear:   //                                 %d %m %y
                    if(i-2 >= 0) {
                        aDay =   dateSplit[i-2];
                        aMonth = dateSplit[i-1];
                        aYear =  dateSplit[i];
                        dateFound = true;
                    }
                    break;
                default:
                    qDebug("ConvertDate - not a valid date format");
                }
                if (dateFound)
                    break;
            }
        }
        if (!dateFound) {
            return QDate();
        }
    } else if(dateSplit.count() == 3) {
        switch (m_dateFormatIndex) {
        case DateFormat::YearMonthDay:   //                                 %y %m %d
            aYear =  dateSplit[0];
            aMonth = dateSplit[1];
            aDay =   dateSplit[2];
            break;
        case DateFormat::MonthDayYear:   //                                 %m %d %y
            aMonth = dateSplit[0];
            aDay =   dateSplit[1];
            aYear =  dateSplit[2];
            break;
        case DateFormat::DayMonthYear:   //                                 %d %m %y
            aDay =   dateSplit[0];
            aMonth = dateSplit[1];
            aYear =  dateSplit[2];
            break;
        default:
            qDebug("ConvertDate - not a valid date format");
        }
    } else if (dateSplit.count() == 2) { //                  it can be date and time without separators
        QString date;
        if (dateSplit.at(0).length() == 8)
            date = dateSplit.at(0);
        else if (dateSplit.at(1).length() == 8)
            date = dateSplit.at(1);
        else
            return QDate();
        switch (m_dateFormatIndex) {
        case DateFormat::YearMonthDay:   //                                 %y %m %d
            aYear =  date.left(4);
            aMonth = date.mid(4,2);
            aDay =   date.right(2);
            break;
        case DateFormat::MonthDayYear:   //                                 %m %d %y
            aMonth = date.left(2);
            aDay =   date.mid(2,2);
            aYear =  date.right(4);
            break;
        case DateFormat::DayMonthYear:   //                                 %d %m %y
            aDay =   date.left(2);
            aMonth = date.mid(2,2);
            aYear =  date.right(4);
            break;
        default:
            qDebug("ConvertDate - not a valid date format");
        }
    } else {                             //                  not a valid date
        return QDate();
    }
//                                                 Check year
    if (aYear.length() == 2) {       //                    2 digits
        if ((aYear.toInt() >= 0) && (aYear.toInt() < 50)) {
            aYear.prepend(QLatin1String("20"));//                      take year to be 2000-2049
        } else if ((aYear.toInt() >= 50) && (aYear.toInt() <= 99))
            aYear.prepend(QLatin1String("19"));//                      take year to be 1950-1999
    } else if (aYear.length() == 4) {
        if ((aYear.toInt() < 1950) || (aYear.toInt() > 2050)) {      //  not a valid year
            return QDate();
        }
    } else {
        return QDate();//                              2 or 4 digits for a valid year
    }
    // only years 1950-2050 valid
    //                                               check day
    if (aDay.length() == 1)
        aDay.prepend(QLatin1Char('0'));//                           add a leading '0'
    if ((aDay.toInt() < 0) || (aDay.toInt() > 31)      //              check day value
            || (aDay.length()  < 1) || (aDay.length()  > 2)) {
        return QDate();//                              not a valid day
    }
//                                                 check month
    if (aMonth.length() == 1) {
        aMonth.prepend(QLatin1Char('0'));
        aFormat = QLatin1String("MM");
    } else if (aMonth.length() == 2) {      //             assume numeric
        bool datefound = ((aMonth.toUInt() > 0) && (aMonth.toUInt() < 13));
        if (!datefound) {
            return QDate();//                            not a valid day
        }
        aFormat = QLatin1String("MM");//                              aMonth is numeric
    } else {//                                           aMonth NOT numeric
        int i;
        if (aMonth.length() > 3) {
            for (i = 1; i <= 12; ++i)
                if (aMonth.compare(QLocale().standaloneMonthName(i, QLocale::LongFormat), Qt::CaseInsensitive) == 0)
                    break;
            if (i == 13) {
                for (i = 1; i <= 12; ++i)
                    if (aMonth.compare(QLocale().monthName(i, QLocale::LongFormat), Qt::CaseInsensitive) == 0)
                        break;
            }
        } else {
            for (i = 1; i <= 12; ++i)
                if (aMonth.compare(QLocale().standaloneMonthName(i, QLocale::ShortFormat), Qt::CaseInsensitive) == 0)
                    break;
            if (i == 13) {
                for (i = 1; i <= 12; ++i)
                    if (aMonth.compare(QLocale().monthName(i, QLocale::ShortFormat), Qt::CaseInsensitive) == 0)
                        break;
            }
        }

        if (i == 13) {
            QLocale locale;
            qDebug() << "convertDate: Unable to find month" << aMonth << "in";
            qDebug() << locale.name() << locale.bcp47Name() << locale.language() << locale.country() << locale.script();
            for (i = 1; i <= 12; ++i) {
                qDebug() << i << locale.standaloneMonthName(i, QLocale::ShortFormat) << locale.monthName(i, QLocale::ShortFormat)
                         << locale.standaloneMonthName(i, QLocale::LongFormat) << locale.monthName(i, QLocale::LongFormat);
            }
            return QDate();
        }
        aMonth = QString::fromLatin1("%1").arg(i, 2, 10, QLatin1Char('0'));
        aFormat = QLatin1String("MM");
    }

    QString dateFormat;
    // deal with Feb 30th
    if ((aMonth == QLatin1String("02")) && (aDay == QLatin1String("30"))) {
        if (QDate(aYear.toUInt(), 2, 29).isValid()) {
            aDay = QLatin1String("29");
        } else {
            aDay = QLatin1String("28");
        }
    }
    switch (m_dateFormatIndex) {
    case DateFormat::YearMonthDay:   //                                 %y %m %d
        dateFormat = QString::fromLatin1("yyyy%1dd").arg(aFormat);
        dat = aYear + aMonth + aDay;
        break;
    case DateFormat::MonthDayYear:   //                                 %m %d %y
        dateFormat = QString::fromLatin1("%1ddyyyy").arg(aFormat);
        dat = aMonth + aDay + aYear;
        break;
    case DateFormat::DayMonthYear:   //                                 %d %m %y
        dateFormat =  QString::fromLatin1("dd%1yyyy").arg(aFormat);
        dat = aDay + aMonth + aYear;
        break;
    default:
        qDebug("ConvertDate - date format unknown");
    }
    aDate = QDate::fromString(dat, dateFormat);
    qDebug() << "convertDate2" << txt << dat << dateFormat << aDate;
    return aDate;
}

void ConvertDate::setDateFormatIndex(const DateFormat _d)
{
    m_dateFormatIndex = _d;
}

QString ConvertDate::stringFormat()
{
    QString dateFormatString;
    switch (m_dateFormatIndex) {
    case DateFormat::YearMonthDay:   //                                 %y %m %d
        dateFormatString = QLatin1String("yyyyMMdd");
        break;
    case DateFormat::MonthDayYear:   //                                 %m %d %y
        dateFormatString = QLatin1String("MMddyyyy");
        break;
    case DateFormat::DayMonthYear:   //                                 %d %m %y
        dateFormatString =  QLatin1String("ddMMyyyy");
        break;
    default:
        qDebug("ConvertDate - date format unknown");
    }
    return dateFormatString;
}
