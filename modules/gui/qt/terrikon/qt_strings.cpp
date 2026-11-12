/*****************************************************************************
 * qt_strings.cpp: Qt's own dialogs in the interface's language
 *****************************************************************************
 * Copyright (C) 2026 Terrikon contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt_strings.hpp"
#include "qt.hpp"

#include <QCoreApplication>
#include <QHash>
#include <QTranslator>

namespace terrikon {

namespace {

class RussianQtStrings : public QTranslator
{
public:
    using QTranslator::QTranslator;

    bool isEmpty() const override { return false; }

    QString translate(const char *context, const char *source, const char *, int) const override
    {
        static const QHash<QByteArray, QString> strings = {
            { "QColorDialog|Select Color", QStringLiteral("Выбор цвета") },
            { "QColorDialog|&Basic colors", QStringLiteral("&Основные цвета") },
            { "QColorDialog|&Custom colors", QStringLiteral("&Свои цвета") },
            { "QColorDialog|&Add to Custom Colors", QStringLiteral("&Добавить в свои цвета") },
            { "QColorDialog|&Pick Screen Color", QStringLiteral("&Взять цвет с экрана") },
            { "QColorDialog|Hu&e:", QStringLiteral("&Тон:") },
            { "QColorDialog|&Sat:", QStringLiteral("&Насыщ.:") },
            { "QColorDialog|&Val:", QStringLiteral("&Ярк.:") },
            { "QColorDialog|&Red:", QStringLiteral("&Красный:") },
            { "QColorDialog|&Green:", QStringLiteral("&Зелёный:") },
            { "QColorDialog|Bl&ue:", QStringLiteral("&Синий:") },
            { "QColorDialog|A&lpha channel:", QStringLiteral("&Прозрачность:") },
            { "QColorDialog|&HTML:", QStringLiteral("&HTML:") },
            { "QColorDialog|Cursor at %1, %2\nPress ESC to cancel", QStringLiteral("Курсор: %1, %2\nEsc — отмена") },
            { "QPlatformTheme|OK", QStringLiteral("ОК") },
            { "QPlatformTheme|Cancel", QStringLiteral("Отмена") },
            { "QPlatformTheme|&Yes", QStringLiteral("&Да") },
            { "QPlatformTheme|&No", QStringLiteral("&Нет") },
            { "QPlatformTheme|Close", QStringLiteral("Закрыть") },
            { "QPlatformTheme|Apply", QStringLiteral("Применить") },
            { "QPlatformTheme|Reset", QStringLiteral("Сбросить") },
            { "QPlatformTheme|Save", QStringLiteral("Сохранить") },
            { "QPlatformTheme|Open", QStringLiteral("Открыть") },
            { "QPlatformTheme|Help", QStringLiteral("Справка") },
            { "QPlatformTheme|Restore Defaults", QStringLiteral("По умолчанию") },
            { "QLineEdit|&Undo", QStringLiteral("&Отменить") },
            { "QLineEdit|&Redo", QStringLiteral("&Повторить") },
            { "QLineEdit|Cu&t", QStringLiteral("Вы&резать") },
            { "QLineEdit|&Copy", QStringLiteral("&Копировать") },
            { "QLineEdit|&Paste", QStringLiteral("&Вставить") },
            { "QLineEdit|Delete", QStringLiteral("Удалить") },
            { "QLineEdit|Select All", QStringLiteral("Выделить всё") },
            { "QWidgetTextControl|&Undo", QStringLiteral("&Отменить") },
            { "QWidgetTextControl|&Redo", QStringLiteral("&Повторить") },
            { "QWidgetTextControl|Cu&t", QStringLiteral("Вы&резать") },
            { "QWidgetTextControl|&Copy", QStringLiteral("&Копировать") },
            { "QWidgetTextControl|&Paste", QStringLiteral("&Вставить") },
            { "QWidgetTextControl|Delete", QStringLiteral("Удалить") },
            { "QWidgetTextControl|Select All", QStringLiteral("Выделить всё") },
        };
        const auto it = strings.constFind(QByteArray(context) + '|' + source);
        return it == strings.constEnd() ? QString() : *it;
    }
};

}

void installQtStrings(QCoreApplication &app)
{
    /* the interface is in Russian when VLC's own strings are */
    if (!qtr("Cancel").startsWith(QStringLiteral("Отм")))
        return;
    app.installTranslator(new RussianQtStrings(&app));
}

}
